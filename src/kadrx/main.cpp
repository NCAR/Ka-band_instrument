#include <iomanip>
#include <iostream>
#include <string>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <fcntl.h>
#include <stdio.h>
#include <math.h>
#include <sched.h>
#include <unistd.h>
#include <sys/timeb.h>
#include <ctime>
#include <cerrno>
#include <cstdlib>
#include <boost/program_options.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <csignal>
#include <logx/Logging.h>
#include <toolsa/pmu.h>
#include <XmlRpc.h>

#include <QResource>

#include <gitInfo.h>
#include <p7142sd3c.h>
#include <p7142Up.h>

#include "KaOscControl.h"
#include "KaDrxPub.h"
#include "KaPmc730.h"
#include "KaDrxConfig.h"
#include "KaMerge.h"
#include "KaMonitor.h"

LOGGING("kadrx")

using namespace std;
using namespace boost::posix_time;
namespace po = boost::program_options;

/// Our RPC server
using namespace XmlRpc;

std::string _drxConfig;          ///< DRX configuration file
std::string _instance;           ///< application instance
int _chans = KaDrxPub::KA_N_CHANNELS; ///< number of channels
int _tsLength = 256;             ///< The time series length
std::string _gaussianFile = "";  ///< gaussian filter coefficient file
std::string _kaiserFile = "";    ///< kaiser filter coefficient file
bool _simulate = false;          ///< Set true for simulate mode
int _simWavelength = 5000;       ///< The simulated data wavelength, in samples
int _simPauseMs = 20;            ///< The number of milliseconds to pause when reading in simulate mode.
std::string _xmitdHost("localhost"); ///< The host on which ka_xmitd is running
int _xmitdPort = 8080;           ///< The port on which ka_xmitd is listening
bool _allowBlanking = true;      ///< Are we allowing sector blanking via XML-RPC calls?
Pentek::p7142sd3c * _sd3c;       ///< Our SD3C instance

// Note that the transmitter should only fire if _limitersWorking is true
// and both _inBlankingSector and _xmlrpcDisabledTx are false.
bool _limitersWorking = false;   ///< Set true when timers are seen, 
                                 /// so T/R limiters should be functioning
bool _inBlankingSector;          ///< Set true if antenna is in a sector which
                                 /// should be blanked (i.e., xmitter must be off)
bool _xmlrpcDisabledTx = false;  /// was transmit disabled via XML-RPC call?

bool _terminate = false;         ///< set true to signal the main loop to terminate
bool _hup = false;               ///< set true to signal the main loop we got a hup signal
bool _usr1 = false;              ///< set true to signal the main loop we got a usr1 signal
bool _usr2 = false;              ///< set true to signal the main loop we got a usr2 signal
bool _triggersEnabled = false;

/////////////////////////////////////////////////////////////////////
void sigHandler(int sig) {
    ILOG << "Interrupt received...termination may take a few seconds";
    _terminate = true;
}

/////////////////////////////////////////////////////////////////////
void hupHandler(int sig) {
  ILOG << "HUP received...response  may take a few seconds";
  _hup = true;
}

/////////////////////////////////////////////////////////////////////
void usr1Handler(int sig) {
  ILOG << "USR1 received...response  may take a few seconds";
  _usr1 = true;
}

/////////////////////////////////////////////////////////////////////
void usr2Handler(int sig) {
  ILOG << "USR2 received...response  may take a few seconds";
  _usr2 = true;
}

//////////////////////////////////////////////////////////////////////
//
/// Parse the command line options, and also set some options
/// that are not specified on the command line.
/// @return The runtime options that can be passed to the
/// threads that interact with the RR314.
void parseOptions(int argc,
        char** argv)
{

    // get the options
    po::options_description descripts("Options");
    descripts.add_options()
    ("help", "Describe options")
    ("drxConfig", po::value<std::string>(&_drxConfig), "DRX configuration file")
    ("instance", po::value<std::string>(&_instance), "App instance for procmap")
    ("simulate",                                   "Enable simulation")
    ("simPauseMS",  po::value<int>(&_simPauseMs),  "Simulation pause interval (ms)")
    ("simWavelength", po::value<int>(&_simWavelength), "The simulated data wavelength, in samples")
    ("tsLength", po::value<int>(&_tsLength), "The time series length")
    ("xmitdHost", po::value<std::string>(&_xmitdHost), "Host machine for ka_xmitd")
    ("xmitdPort", po::value<int>(&_xmitdPort), "Port for contacting ka_xmitd")
            ;
    // If we get an option on the command line with no option name, it
    // is treated like --drxConfig=<option> was given.
    po::positional_options_description pd;
    pd.add("drxConfig", 1);

    po::variables_map vm;
    po::store(po:: command_line_parser(argc, argv).options(descripts).positional(pd).run(), vm);
    po::notify(vm);

    if (vm.count("help")) {
        std::cout << "Usage: " << argv[0] <<
            " [OPTION]... [--drxConfig] <configFile>" << std::endl;
        std::cout << descripts << std::endl;
        exit(0);
    }

    if (vm.count("simulate"))
        _simulate = true;
    if (vm.count("drxConfig") != 1) {
        ELOG << "Exactly one DRX configuration file must be given!";
        exit(1);
    }
}

///////////////////////////////////////////////////////////
/// If the user has root privileges, make the process real-time.
void
makeRealTime()
{
    uid_t id = getuid();

    // don't even try if we are not root.
    if (id != 0) {
        WLOG << "Not root, unable to change scheduling priority";
        return;
    }

    sched_param sparam;
    sparam.sched_priority = 50;

    if (sched_setscheduler(0, SCHED_RR, &sparam)) {
        ELOG << "warning, unable to set scheduler parameters: " << strerror(errno);
    }
}

///////////////////////////////////////////////////////////
/// @return The current time, in seconds since Jan. 1, 1970.
double nowTime()
{
    struct timeb timeB;
    ftime(&timeB);
    return timeB.time + timeB.millitm/1000.0;
}

///////////////////////////////////////////////////////////
void startUpConverter(Pentek::p7142Up& upConverter,
        unsigned int pulsewidth_counts) {

    // create the signal
    unsigned int n = pulsewidth_counts * 2;
    int32_t IQ[n];

    for (unsigned int i = 0; i < n/2; i++) {
        IQ[i]   = 0x8000 << 16 | 0x8000;
    }
    for (unsigned int i = n/2; i < n; i++) {
        IQ[i]   = 0x8000 << 16 | 0x8000;
    }
    // load mem2
    upConverter.write(IQ, n);

    // start the upconverter
    upConverter.startDAC();

}

///////////////////////////////////////////////////////////
void
setTxEnableLine() {
    bool enableTx = _limitersWorking && ! _inBlankingSector &&
            ! _xmlrpcDisabledTx;
    // Enable the transmitter iff the T/R limiters are working and we're not
    // in a blanking sector and transmit has not been disabled via XMLRPC call.
    KaPmc730::setTxTriggerEnable(enableTx);
    _triggersEnabled = enableTx;
}

///////////////////////////////////////////////////////////
void
verifyTimersAndEnableTx() {
    if (_simulate)
        return;
    // Wait until we see at least countThreshold pulse counts before we
    // enable the transmitter
    const uint32_t CountThreshold = 50;
    // Loop time and max # of loops to see our threshold number of counts
    const float LoopTime = 0.05;    // seconds
    const int MaxTries = 100;

    uint32_t initialCount = 0;
    uint32_t pulsesSeen = 0;
    // Loop until we get CountThreshold pulses
    int i;
    for (i = 0; i < MaxTries; i++) {
        uint32_t pulseCount = KaPmc730::getPulseCounter();
        if (i == 0)
            initialCount = pulseCount;
        pulsesSeen = pulseCount - initialCount;
        if (pulsesSeen > CountThreshold) {
            break;
        }
        usleep(useconds_t(1000000 * LoopTime));
    }
    ILOG << "Saw " << pulsesSeen << " timer pulses in " << i * LoopTime <<
        " seconds";
    if (i == MaxTries) {
        ELOG << "Pentek timers don't seem to be running...";
        exit(1);
    }
    // Set _limitersWorking to true, since T/R limiters should be
    // functioning now.
    ILOG << "T/R limiters should now be working";
    _limitersWorking = true;
    setTxEnableLine();
}

///////////////////////////////////////////////////////////
// Disable transmitter within a blanking sector, starting at the
// given pulse number.
// @param pulseSeqNum the pulse number at which to apply this transmitter
// state
void
setBlankingOn(int64_t pulseSeqNum) {
    // We don't care about pulseSeqNum here, since we get calls from the 
    // synchro-to-digital box with effectively zero latency. Just disable
    // the transmitter.
    _inBlankingSector = true;
    setTxEnableLine();

    KaOscControl::theControl().setBlankingEnabled(true, pulseSeqNum);
}

///////////////////////////////////////////////////////////
// Enable transmitter within a blanking sector, starting at the
// given pulse number.
// @param pulseSeqNum the pulse number at which to apply this transmitter
// state
void
setBlankingOff(int64_t pulseSeqNum) {
    // We don't care about pulseSeqNum here, since we get calls from the 
    // synchro-to-digital box with effectively zero latency. Just enable
    // the transmitter.
    _inBlankingSector = false;
    setTxEnableLine();
    
    KaOscControl::theControl().setBlankingEnabled(false, pulseSeqNum);
}

///////////////////////////////////////////////////////////
void
verifyPpsAndNtp(const KaMonitor & kaMonitor) {
    if (! kaMonitor.gpsTimeServerGood()) {
        ELOG << "GPS time server is reporting fault, so we don't trust 1 PPS!";
        exit(1);
    }
    
    // Get the SystemClockOffset.py script as a QResource
    QResource pyScriptResource(":/SystemClockOffset.py");

    // Build a simple sh script to execute the Python script
    std::string shScript("python << __EOF__\n");
    shScript.append(reinterpret_cast<const char*>(pyScriptResource.data()),
    		        pyScriptResource.size());
    shScript.append("\n__EOF__");

    
    // Execute the sh script via popen and get the output, which will be
    // the system clock offset in seconds.
    FILE *scriptOutput = popen(shScript.c_str(), "r");
    float offset_s;
    fscanf(scriptOutput, "%f", &offset_s);
    if (pclose(scriptOutput) != 0) {
        ELOG << "Failed to get system clock offset. Not starting.";
        exit(1);
    }
    // p7142sd3c::timersStartStop() currently assumes we're within 200 ms
    if (fabs(offset_s) >= 0.2) {
        ELOG << "System time offset of " << offset_s << " s is too large!";
        exit(1);
    }
    ILOG << "System clock offset is currently " << offset_s << " s";
}

/////////////////////////////////////////////////////////////////////
/// Xmlrpc++ method to raise the Ka transmitter serial reset line.
//
// Control of the transmitter serial reset is really the concern of ka_xmitd,
// but access to the line is via the PMC-730 card, which is owned here. Hence,
// we have to provide an API for driving the line.
class RaiseXmitTtyResetMethod : public XmlRpcServerMethod {
public:
    RaiseXmitTtyResetMethod() : XmlRpcServerMethod("raiseXmitTtyReset") {}
    void execute(XmlRpcValue & paramList, XmlRpcValue & retvalP) {
        ILOG << "Received 'raiseXmitTtyReset' command";
        KaPmc730::setTxSerialResetLine(true);
    }
};

/////////////////////////////////////////////////////////////////////
/// Xmlrpc++ method to lower the Ka transmitter serial reset line
//
// Control of the transmitter serial reset is really the concern of ka_xmitd,
// but access to the line is via the PMC-730 card, which is owned here. Hence,
// we have to provide an API for driving the line.
class LowerXmitTtyResetMethod : public XmlRpcServerMethod {
public:
    LowerXmitTtyResetMethod() : XmlRpcServerMethod("lowerXmitTtyReset") {}
    void execute(XmlRpcValue & paramList, XmlRpcValue & retvalP) {
        ILOG << "Received 'lowerXmitTtyReset' command";
        KaPmc730::setTxSerialResetLine(false);
    }
};

/////////////////////////////////////////////////////////////////////
/// Xmlrpc++ method to turn on transmit blanking (i.e., disable the transmitter)
/// The param list should have:
///     1) (double) the time since 1970-01-01 00:00:00 UTC in seconds
///     2) (double) antenna elevation
///     3) (double) antenna azimuth
/// Returns 0 if the method is successful, or -1 if XML-RPC calls for blanking
/// are currently disallowed.
class SetBlankingOnMethod : public XmlRpcServerMethod {
public:
    SetBlankingOnMethod() : XmlRpcServerMethod("setBlankingOn") {}
    void execute(XmlRpcValue & paramList, XmlRpcValue & retvalP) {
        double time = paramList[0];
        double elev = paramList[1];
        double azim = paramList[2];
        DLOG << "Received 'setBlankingOn' command with time: " << time <<
                ", elev: " << elev << ", azim: " << azim;
        if (! _allowBlanking) {
            DLOG << "Blanking via XML-RPC calls is currently disallowed";
            retvalP = -1;
            return;
        }
        ptime pTime = boost::posix_time::from_time_t(int(time));
        int usecs = int(1.0e6 * fmod(time, 1.0));
        pTime += boost::posix_time::microseconds(usecs);
        int64_t pulseSeqNum = _sd3c->pulseAtTime(pTime);
        setBlankingOn(pulseSeqNum);
        retvalP = 0;
    }
};

/////////////////////////////////////////////////////////////////////
/// Xmlrpc++ method to turn off transmit blanking (i.e., enable the transmitter)
/// The param list should have:
///     1) (double) the time since 1970-01-01 00:00:00 UTC in seconds
///     2) (double) antenna elevation
///     3) (double) antenna azimuth
/// Returns 0 if the method is successful, or -1 if XML-RPC calls for blanking
/// are currently disallowed.
class SetBlankingOffMethod : public XmlRpcServerMethod {
public:
    SetBlankingOffMethod() : XmlRpcServerMethod("setBlankingOff") {}
    void execute(XmlRpcValue & paramList, XmlRpcValue & retvalP) {
        double time = paramList[0];
        double elev = paramList[1];
        double azim = paramList[2];
        DLOG << "Received 'setBlankingOff' command with time: " << time <<
                ", elev: " << elev << ", azim: " << azim;
        if (! _allowBlanking) {
            DLOG << "Blanking via XML-RPC calls is currently disallowed";
            retvalP = -1;
            return;
        }
        ptime pTime = boost::posix_time::from_time_t(int(time));
        int usecs = int(1.0e6 * fmod(time, 1.0));
        pTime += boost::posix_time::microseconds(usecs);
        int64_t pulseSeqNum = _sd3c->pulseAtTime(pTime);
        setBlankingOff(pulseSeqNum);
        retvalP = 0;
    }
};

/////////////////////////////////////////////////////////////////////
/// Xmlrpc++ method to disable the transmitter.
// If this method is called, then the XMLRPC "enableTransmit" method must be
// called to re-enable transmit.
class DisableTransmitMethod : public XmlRpcServerMethod {
public:
    DisableTransmitMethod() : XmlRpcServerMethod("disableTransmit") {}
    void execute(XmlRpcValue & paramList, XmlRpcValue & retvalP) {
        DLOG << "Received 'disableTransmit' command";
        _xmlrpcDisabledTx = true;
        setTxEnableLine();
        retvalP = 0;
    }
};

/////////////////////////////////////////////////////////////////////
/// Xmlrpc++ method to re-enable the transmitter after a call to the
/// XMLRPC "disableTransmit" method.
class EnableTransmitMethod : public XmlRpcServerMethod {
public:
    EnableTransmitMethod() : XmlRpcServerMethod("enableTransmit") {}
    void execute(XmlRpcValue & paramList, XmlRpcValue & retvalP) {
        DLOG << "Received 'enableTransmit' command";
        _xmlrpcDisabledTx = false;
        setTxEnableLine();
        retvalP = 0;
    }
};

///////////////////////////////////////////////////////////
int
main(int argc, char** argv)
{
    // try to change scheduling to real-time
    makeRealTime();

    // Let logx get and strip out its arguments
    logx::ParseLogArgs(argc, argv);

    // parse the command line options
    parseOptions(argc, argv);

    ILOG << "kadrx - Git commit " << REPO_REVISION << "," << REPO_EXTERNALS;

    // set up registration with procmap if instance is specified
    if (_instance.size() > 0) {
        PMU_auto_init("kadrx", _instance.c_str(), PROCMAP_REGISTER_INTERVAL);
        ILOG << "will register with procmap, instance: " << _instance;
    }

    // Read the KA configuration file
    KaDrxConfig kaConfig(_drxConfig);
    if (! kaConfig.isValid()) {
        ELOG << "Exiting on incomplete configuration!";
        exit(1);
    }
    
    // Initial value for _inBlankingSector is true if we're allowing for 
    // blanking (we'll blank until we're explicitly told we're *not* in a 
    // blanking sector), otherwise false (since no sectors will be blanked).
    _allowBlanking = kaConfig.allow_blanking();
    _inBlankingSector = _allowBlanking ? true : false;

    // Make sure our KaPmc730 is created in simulation mode if requested
    KaPmc730::doSimulate(kaConfig.simulate_pmc730());

    // set to ignore SIGPIPE errors which occur when sockets
    // are broken between client and server

    signal(SIGPIPE, SIG_IGN);

    // Create our status monitoring thread.
    KaMonitor kaMonitor(_xmitdHost, _xmitdPort);

    // create the merge object (which is also the IWRF TCP server)
    KaMerge merge(kaConfig, kaMonitor);

    // Turn off transmitter trigger enable until we know we're generating
    // timing signals (and hence that the T/R limiters are presumably
    // operating).
    PMU_auto_register("trigger enable");
    KaPmc730::setTxTriggerEnable(false);
    _triggersEnabled = false;

    // check PRT, set timer divisor accordingly
    // if PRF < 800 Hz (PRT > 0.00125 sec) set divisor to 4
    // otherwise use the default of 2

    int sd3cTimerDivisor = 2;
    if (kaConfig.prt1() > 0.00125) {
      sd3cTimerDivisor = 4;
    }
    DLOG << "==>> prt1, sd3cTimerDivisor: " << kaConfig.prt1() << ", " << sd3cTimerDivisor;
    
    // Instantiate our p7142sd3c

    PMU_auto_register("pentek initialize");
    _sd3c = new Pentek::p7142sd3c(false, // simulate?
                                  kaConfig.tx_delay(),
                                  kaConfig.tx_pulse_width(),
                                  kaConfig.prt1(),
                                  kaConfig.prt2(),
                                  kaConfig.staggered_prt(),
                                  kaConfig.gates(),
                                  1, // nsum
                                  false, // free run
                                  Pentek::p7142sd3c::DDC10DECIMATE,
                                  kaConfig.external_start_trigger(),
                                  50, // sim pause, ms
                                  true, // use first card
                                  false, // rim
                                  0, // code length (N/A for us)
                                  0, // ADC clock (0 = use default for our DDC type)
                                  true, // reset_clock_managers
                                  true, // abortOnError
                                  sd3cTimerDivisor
                                  );

    // Die if the card has DDC10 rev. 783 loaded, since it has a known problem
    // with timer programming
    if (_sd3c->sd3cRev() == 783) {
        ELOG << "Timer programming in SD3C DDC10 revision 783 is broken!";
        ELOG << "Load revision 535, or a revision later than 783.";
        ELOG << "EXITING kadrx";
        exit(0);
    }

    // Use SD3C's general purpose timer 0 (timer 3) for transmit pulse modulation
    PMU_auto_register("timers enable");
    _sd3c->setGPTimer0(kaConfig.tx_pulse_mod_delay(), kaConfig.tx_pulse_mod_width());

    // Use SD3C's general purpose timer 1 (timer 5) for test target pulse.
    _sd3c->setGPTimer1(kaConfig.test_target_delay(), kaConfig.test_target_width());

    // Time in seconds from leading edge of modulation pulse to leading edge of
    // transmit pulse (lab-measured).
    double tx_lag = 600e-9; // ~600 ns transmitter lag time

    // Out-and-back time to far field at 34.7 GHz with a 0.7 m antenna
    double far_field_time = 1.1e-6;

    // Use SD3C's general purpose timer 2 (timer 6) for T/R LIMITER trigger.
    // We hold it *low* from T0 until the trailing edge of the transmit pulse
    // has had time to reach far-field and return. The trigger is high for the
    // rest of the PRT.
    double trLimiterWidth = kaConfig.tx_pulse_mod_delay() +
    		kaConfig.tx_pulse_mod_width() + tx_lag + far_field_time;

    _sd3c->setGPTimer2(0.0, trLimiterWidth, true);

    // Use SD3C's general purpose timer 3 (timer 7) for PIN SW trigger.
    // This signal is just the inverse of the T/R LIMITER signal above.
    _sd3c->setGPTimer3(0.0, trLimiterWidth, false);

    // Create (but don't yet start) the downconversion threads.

    // H channel (0)
    PMU_auto_register("set up threads");
    KaDrxPub hThread(*_sd3c, KaDrxPub::KA_H_CHANNEL, kaConfig, &merge,
        _tsLength, _gaussianFile, _kaiserFile, _simPauseMs, _simWavelength);

    // V channel (1)
    KaDrxPub vThread(*_sd3c, KaDrxPub::KA_V_CHANNEL, kaConfig, &merge,
        _tsLength, _gaussianFile, _kaiserFile, _simPauseMs, _simWavelength);

    // Burst channel (2)
    KaDrxPub burstThread(*_sd3c, KaDrxPub::KA_BURST_CHANNEL, kaConfig, &merge,
        _tsLength, _gaussianFile, _kaiserFile, _simPauseMs, _simWavelength);

    // Create the upConverter.
    // Configure the DAC to use CMIX by fDAC/4 (coarse mixer mode = 9)
    PMU_auto_register("create upconverter");
    Pentek::p7142Up & upConverter = *_sd3c->addUpconverter(
    		_sd3c->adcFrequency(),
    		_sd3c->adcFrequency() / 4,
			9);

    // Set up oscillator control/AFC
    PMU_auto_register("oscillator control");
    KaOscControl::createTheControl(kaConfig, 
            burstThread.downconverter()->dataInterruptPeriod());
    
    if (! kaConfig.afc_enabled()) {
        WLOG << "AFC is disabled!";
    }

    // catch a SIGINT (from control-C) or SIGTERM (the default from 'kill')
    signal(SIGINT, sigHandler);
    signal(SIGTERM, sigHandler);
    signal(SIGHUP, hupHandler);
    signal(SIGUSR1, usr1Handler);
    signal(SIGUSR2, usr2Handler);

    // Start monitor and merge
    PMU_auto_register("start monitor and merge");
    kaMonitor.start();
    merge.start();

    // Start the downconverter threads.
    PMU_auto_register("start threads");
    hThread.start();
    vThread.start();
    burstThread.start();

    // wait awhile, so that the threads can all get to the first read.
    struct timespec sleepTime = { 1, 0 }; // 1 second, 0 nanoseconds
    while (nanosleep(&sleepTime, &sleepTime)) {
      if (errno != EINTR) {
        ELOG << "Error " << errno << " from nanosleep().  Aborting.";
        abort();
      } else {
        // We were interrupted. Return to sleeping until the interval is done.
        continue;
      }
    }

    // Start filters on all downconverters
    PMU_auto_register("start filters");
    _sd3c->startFilters();

    // Load the DAC memory bank 2, clear the DACM fifo, and enable the
    // DAC memory counters. This must take place before the timers are started.
    PMU_auto_register("start upconverter");
    startUpConverter(upConverter, _sd3c->txPulseWidthCounts());

    // If we're going to start timers based on an external trigger (i.e., 1 PPS
    // from the GPS time server), make sure the time server is OK. Also make
    // sure our system time is close enough that we can calculate the correct
    // start second.
    if (kaConfig.external_start_trigger()) {
        verifyPpsAndNtp(kaMonitor);
    }

    // Start the timers, which will allow data to flow.
    _sd3c->timersStartStop(true);

    // Verify that timers have started, by seeing that we have TX sync pulses
    // being generated, then raise the TX enable line.
    verifyTimersAndEnableTx();

    // Initialize our RPC server on port 8081
    XmlRpc::XmlRpcServer rpcServer;
    rpcServer.addMethod(new RaiseXmitTtyResetMethod());
    rpcServer.addMethod(new LowerXmitTtyResetMethod());
    rpcServer.addMethod(new SetBlankingOnMethod());
    rpcServer.addMethod(new SetBlankingOffMethod());
    if (! rpcServer.bindAndListen(8081)) {
        ELOG << "Exiting on XmlRpcServer error!";
        exit(1);
    }
    rpcServer.enableIntrospection(true);
    

    double startTime = nowTime();
    while (1) {
        // Process XML-RPC commands for a brief bit
        rpcServer.work(0.01);
        
        // If we got a ^C or similar termination request, bail out.
        if (_terminate) {
            break;
        }

        if (_hup) {
          cerr << "HUP received...disabling the triggers" << endl;
          // Turn off transmitter trigger enable
          KaPmc730::setTxTriggerEnable(false);
          cerr << "HUP received...disabling the triggers - done" << endl;
          _hup = false;
          _triggersEnabled = false;
        }

        if (_usr1) {
          cerr << "USR1 received...re-enabling the triggers" << endl;
          if (_triggersEnabled) {
            cerr << "Triggers already enabled - no action taken" << endl;
          } else {
            verifyTimersAndEnableTx();
            cerr << "USR1 received...re-enabling the triggers - done" << endl;
          }
          _usr1 = false;
        }

        if (_usr2) {
//            cerr << "USR2 received...disabling triggers, resetting serial port and re-anbling triggers " << endl;
//            RaiseXmitTtyResetMethod resetTty;
//            XmlRpcValue  paramList,  retvalP;
//            resetTty.execute(paramList, retvalP);
            ILOG << "USR2 signal handling is now a no-op!";
            _usr2 = false;
        }
        // How long since our last status print?
        double currentTime = nowTime();
        double elapsed = currentTime - startTime;

        // If it's been long enough, go on to the status print below, otherwise
        // go back to the top of the loop.
        if (elapsed > 10.0) {
            startTime = currentTime;
        } else {
            continue;
        }

        ILOG << std::setprecision(3) << std::setw(5) << "H channel " <<
                hThread.downconverter()->bytesRead() * 1.0e-6 / elapsed <<
                " MB/s, drop: " << hThread.downconverter()->droppedPulses() <<
                ", sync errs: " << hThread.downconverter()->syncErrors();

        ILOG << std::setprecision(3) << std::setw(5) << "V channel " <<
                vThread.downconverter()->bytesRead() * 1.0e-6 / elapsed <<
                " MB/s, drop: " << vThread.downconverter()->droppedPulses() <<
                " sync errs: " << vThread.downconverter()->syncErrors();

        ILOG << std::setprecision(3) << std::setw(5) << "burst channel " <<
                burstThread.downconverter()->bytesRead() * 1.0e-6 / elapsed <<
                " MB/s, drop: " << burstThread.downconverter()->droppedPulses() <<
                " sync errs: " << burstThread.downconverter()->syncErrors();
    }

    // Turn off transmitter trigger enable
    cerr << "Setting trigger enable to false" << endl;
    KaPmc730::setTxTriggerEnable(false);
    cerr << "Setting trigger enable to false - done" << endl;

    cerr << "Sleeping 2 sec" << endl;
    sleep(2);
    cerr << "Sleep done" << endl;

    // Stop the downconverter threads. These have no event loops, so they
    // must be stopped using "terminate()"
    hThread.terminate();
    vThread.terminate();
    burstThread.terminate();

    // Wait for threads' termination (up to 1 second for each)
    hThread.wait(1000);
    vThread.wait(1000);
    burstThread.wait(1000);

    // stop the DAC
    upConverter.stopDAC();

    // stop the timers
    cerr << "Stopping the timers" << endl;
    _sd3c->timersStartStop(false);
    cerr << "Stopping the timers - done" << endl;

    ILOG << "terminated on command";
}

