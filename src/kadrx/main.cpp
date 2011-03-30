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

#include "KaOscControl.h"
#include "KaDrxPub.h"
#include "KaPmc730.h"
#include "p7142sd3c.h"
#include "KaDrxConfig.h"
#include "KaMerge.h"
#include "KaMonitor.h"

LOGGING("kadrx")

using namespace std;
using namespace boost::posix_time;
namespace po = boost::program_options;

/// Our RPC server
using namespace XmlRpc;
XmlRpcServer RpcServer;

std::string _devRoot("/dev/pentek/p7142/0"); ///< Device root e.g. /dev/pentek/0
std::string _drxConfig;          ///< DRX configuration file
std::string _instance;           ///< application instance
int _chans = KaDrxPub::KA_N_CHANNELS; ///< number of channels
int _tsLength = 256;             ///< The time series length
std::string _gaussianFile = "";  ///< gaussian filter coefficient file
std::string _kaiserFile = "";    ///< kaiser filter coefficient file
KaMerge* _merge = 0;             ///< The merge object - also IWRF TCP server
bool _simulate = false;          ///< Set true for simulate mode
int _simWavelength = 5000;       ///< The simulated data wavelength, in samples
int _simPauseMs = 20;            ///< The number of milliseconds to pause when reading in simulate mode.
std::string _xmitdHost("kadrx"); ///< The host on which ka_xmitd is running
int _xmitdPort = 8080;           ///< The port on which ka_xmitd is listening

bool _terminate = false;         ///< set true to signal the main loop to terminate

/////////////////////////////////////////////////////////////////////
/// Xmlrpc++ method to reset the Ka transmitter serial port
class ResetXmitterTtyMethod : public XmlRpcServerMethod {
public:
    ResetXmitterTtyMethod(XmlRpcServer *s) : XmlRpcServerMethod("resetXmitterTty", s) {}
    void execute(XmlRpcValue & paramList, XmlRpcValue & retvalP) {
        ILOG << "Received 'resetXmitterTty' command";
        // Raise the DIO line for serial port reset for a few ms, then lower
        // it again.
        KaPmc730::setTxSerialReset(true);
        usleep(25000);
        KaPmc730::setTxSerialReset(false);
    }
} resetXmitterTtyMethod(&RpcServer);

/////////////////////////////////////////////////////////////////////
void sigHandler(int sig) {
    ILOG << "Interrupt received...termination may take a few seconds";
    _terminate = true;
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
	("devRoot", po::value<std::string>(&_devRoot), "Device root (e.g. /dev/pentek/0)")
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
    // Enable the transmitter
    ILOG << "Enabling transmitter";
    KaPmc730::setTxTriggerEnable(true);
}

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

    // Make sure our KaPmc730 is created in simulation mode if requested
    KaPmc730::doSimulate(kaConfig.simulate_pmc730());

    // set to ignore SIGPIPE errors which occur when sockets
    // are broken between client and server

    signal(SIGPIPE, SIG_IGN);

    // Start our status monitoring thread.
    KaMonitor kaMonitor(_xmitdHost, _xmitdPort);
    kaMonitor.start();

    // create the merge object
    _merge = new KaMerge(kaConfig, kaMonitor);

    // Turn off transmitter trigger enable until we know we're generating
    // timing signals (and hence that the T/R limiters are presumably
    // operating).
    PMU_auto_register("trigger enable");
    KaPmc730::setTxTriggerEnable(false);

    // Instantiate our p7142sd3c
    PMU_auto_register("pentek initialize");
    Pentek::p7142sd3c sd3c(_devRoot, _simulate, kaConfig.tx_delay(),
        kaConfig.tx_pulse_width(), kaConfig.prt1(), kaConfig.prt2(),
        kaConfig.staggered_prt(), kaConfig.gates(), 1, false,
        Pentek::p7142sd3c::DDC10DECIMATE, kaConfig.external_start_trigger());

    // Use SD3C's general purpose timer 0 (timer 3) for transmit pulse modulation
    PMU_auto_register("timers enable");
    sd3c.setGPTimer0(kaConfig.tx_pulse_mod_delay(), kaConfig.tx_pulse_mod_width());

    // Use SD3C's general purpose timer 1 (timer 5) for test target pulse.
    sd3c.setGPTimer1(kaConfig.test_target_delay(), kaConfig.test_target_width());

    // Use SD3C's general purpose timer 2 (timer 6) for T/R LIMITER trigger.
    // It must be *low* from T0 to 500 ns after the transmit pulse ends, and
    // high for the rest of the PRT.
    double trLimiterWidth = kaConfig.tx_pulse_mod_delay() +
        kaConfig.tx_pulse_mod_width() + 7.0e-7;
    sd3c.setGPTimer2(0.0, trLimiterWidth, true);

    // Use SD3C's general purpose timer 3 (timer 7) for PIN SW trigger.
    // This signal is the inverse of the T/R LIMITER signal above.
    sd3c.setGPTimer3(0.0, trLimiterWidth, false);

	// Create (but don't yet start) the downconversion threads.

    // H channel (0)
    PMU_auto_register("set up threads");
    KaDrxPub hThread(sd3c, KaDrxPub::KA_H_CHANNEL, kaConfig, _merge,
        _tsLength, _gaussianFile, _kaiserFile, _simPauseMs, _simWavelength);

    // V channel (1)
    KaDrxPub vThread(sd3c, KaDrxPub::KA_V_CHANNEL, kaConfig, _merge,
        _tsLength, _gaussianFile, _kaiserFile, _simPauseMs, _simWavelength);

    // Burst channel (2)
    KaDrxPub burstThread(sd3c, KaDrxPub::KA_BURST_CHANNEL, kaConfig, _merge,
        _tsLength, _gaussianFile, _kaiserFile, _simPauseMs, _simWavelength);

    // Create the upConverter.
    // Configure the DAC to use CMIX by fDAC/4 (coarse mixer mode = 9)
    PMU_auto_register("create upconverter");
    Pentek::p7142Up & upConverter = *sd3c.addUpconverter("0C",
        sd3c.adcFrequency(), sd3c.adcFrequency() / 4, 9);

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
    sd3c.startFilters();

    // Load the DAC memory bank 2, clear the DACM fifo, and enable the
    // DAC memory counters. This must take place before the timers are started.
    PMU_auto_register("start upconverter");
    startUpConverter(upConverter, sd3c.txPulseWidthCounts());

    // start the merge
    PMU_auto_register("start merge");
    _merge->start();

    // Start the timers, which will allow data to flow.
    sd3c.timersStartStop(true);

    // Verify that timers have started, by seeing that we have TX sync pulses
    // being generated, then raise the TX enable line.
    verifyTimersAndEnableTx();

    // Initialize our RPC server on port 8081
    if (! RpcServer.bindAndListen(8081)) {
        ELOG << "Exiting on XmlRpcServer error!";
        exit(1);
    }
    RpcServer.enableIntrospection(true);
    
	double startTime = nowTime();
	while (1) {
	    // Listen for XML-RPC commands for a bit.
        // Note that work() mostly goes for 2x the given time, but sometimes
        // goes for 1x the given time. Who knows why?
        RpcServer.work(0.05);
        
        // If we got a ^C or similar termination request, bail out.
		if (_terminate) {
			break;
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
                " MB/s  ovr: " << hThread.downconverter()->overUnderCount() <<
                " drop: " << hThread.downconverter()->droppedPulses() <<
                " sync errs: " << hThread.downconverter()->syncErrors();

        ILOG << std::setprecision(3) << std::setw(5) << "V channel " <<
                vThread.downconverter()->bytesRead() * 1.0e-6 / elapsed <<
                " MB/s  ovr: " << vThread.downconverter()->overUnderCount() <<
                " drop: " << vThread.downconverter()->droppedPulses() <<
                " sync errs: " << vThread.downconverter()->syncErrors();

        ILOG << std::setprecision(3) << std::setw(5) << "burst channel " <<
                burstThread.downconverter()->bytesRead() * 1.0e-6 / elapsed <<
                " MB/s  ovr: " << burstThread.downconverter()->overUnderCount() <<
                " drop: " << burstThread.downconverter()->droppedPulses() <<
                " sync errs: " << burstThread.downconverter()->syncErrors();
	}

    // Turn off transmitter trigger enable
    KaPmc730::setTxTriggerEnable(false);

    // Stop the downconverter threads.
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
    sd3c.timersStartStop(false);

    ILOG << "terminated on command";
}

