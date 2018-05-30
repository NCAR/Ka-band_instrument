#include <string>
#include <csignal>
#include <cmath>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/program_options.hpp>
#include <logx/Logging.h>
#include <toolsa/pmu.h>
#include <QFunctionWrapper.h>
#include <QtCore/QCoreApplication>
#include <QtCore/QTimer>
#include <QXmlRpcServerAbyss.h>

#include <QtCore/QResource>

#include <gitInfo.h>
#include <p7142sd3c.h>
#include <p7142Up.h>

#include "KaOscControl.h"
#include "KaDrxPub.h"
#include "KaPmc730.h"
#include "KaDrxConfig.h"
#include "KadrxStatus.h"
#include "KaMerge.h"
#include "KaMonitor.h"
#include "NoXmitBitmap.h"

LOGGING("kadrx")

using namespace Pentek;
namespace po = boost::program_options;

QCoreApplication * _app = NULL; ///< Our QApplication instance
std::string _drxConfig;         ///< DRX configuration file
std::string _instance;          ///< instance name for PMU
int _chans = KaDrxPub::KA_N_CHANNELS; ///< number of channels
KaDrxPub * _hThread = NULL;     ///< thread to read and publish H channel data
KaDrxPub * _vThread = NULL;     ///< thread to read and publish H channel data
KaDrxPub * _burstThread = NULL; ///< thread to read and publish burst channel data
const float STATUS_INTERVAL_SECS = 10.0;        ///< interval for status logging
int _tsLength = 256;            ///< The time series length
std::string _gaussianFile = ""; ///< gaussian filter coefficient file
std::string _kaiserFile = "";   ///< kaiser filter coefficient file
bool _simulate = false;         ///< Set true for simulate mode
int _simWavelength = 5000;      ///< The simulated data wavelength, in samples
int _simPauseMs = 20;           ///< The number of milliseconds to pause when reading in simulate mode.
std::string _xmitdHost("localhost"); ///< The host on which ka_xmitd is running
int _xmitdPort = 8080;          ///< The port on which ka_xmitd is listening
bool _afcEnabled = false;       ///< Is AFC enabled?
bool _allowBlanking = true;     ///< Are we allowing transmit disable via XML-RPC calls?
p7142sd3c * _sd3c = NULL;       ///< Our SD3C instance

// Bitmap of conditions which currently disable transmit. The transmitter
// will not be allowed to fire if _noXmitBitmap has any bits set.
static NoXmitBitmap _noXmitBitmap;

// Our KaMonitor instance
KaMonitor * _kaMonitor = NULL;

bool _terminate = false;         ///< set true to signal the main loop to terminate
bool _hup = false;               ///< set true to signal the main loop we got a hup signal
bool _usr1 = false;              ///< set true to signal the main loop we got a usr1 signal

/////////////////////////////////////////////////////////////////////
void sigHandler(int sig) {
    ILOG << "Initiating program exit on '" << strsignal(sig) << "' signal";
    // Stop our QCoreApplication instance
    _app->quit();
}

/////////////////////////////////////////////////////////////////////
/// @brief Note receipt of a HUP signal
void hupHandler(int sig) {
  ILOG << "HUP received, setting _hup flag";
  _hup = true;
  // If we received a USR1 which has not been handled, this later signal
  // overrides it
  if (_usr1) {
      WLOG << "HUP is overriding previously received USR1";
      _usr1 = false;
  }
}

/////////////////////////////////////////////////////////////////////
/// @brief Note receipt of a USR1 signal
void usr1Handler(int sig) {
  ILOG << "USR1 received, setting _usr1 flag";
  _usr1 = true;
  // If we received a HUP which has not been handled, this later signal
  // overrides it
  if (_hup) {
      WLOG << "USR1 is overriding previously received HUP";
      _hup = false;
  }
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
void
startUpconverter(p7142sd3c * sd3c) {

    // create the signal
    unsigned int n = sd3c->txPulseWidthCounts() * 2;
    int32_t IQ[n];

    for (unsigned int i = 0; i < n/2; i++) {
        IQ[i]   = 0x8000 << 16 | 0x8000;
    }
    for (unsigned int i = n/2; i < n; i++) {
        IQ[i]   = 0x8000 << 16 | 0x8000;
    }
    // load mem2
    sd3c->upconverter()->write(IQ, n);

    // start the upconverter
    sd3c->upconverter()->startDAC();
}

///////////////////////////////////////////////////////////
void
updateTxEnableLineState() {
    static NoXmitBitmap prevNoXmitBitmap;

    // If _noXmitBitmap changed from the last time this was called, log
    // whether transmit is allowed, and the reasons why if disallowed.
    if (_noXmitBitmap != prevNoXmitBitmap) {
        if (_noXmitBitmap.allBitsClear()) {
            ILOG << "Transmit is now allowed";
        } else {
            // List the reason(s) transmit is now disallowed
            WLOG << "Transmit is now disallowed for the following reason(s):";
            for (NoXmitBitmap::BITNUM bitnum = NoXmitBitmap::BITNUM(0);
                 bitnum < NoXmitBitmap::NBITS; bitnum++) {
                if (_noXmitBitmap.bitIsSet(bitnum)) {
                    WLOG << "    " << NoXmitBitmap::NoXmitReason(bitnum);
                }
            }
        }
    }
    prevNoXmitBitmap = _noXmitBitmap;

    // Enable transmit if no bits are set in _noXmitBitmap
    bool enableTx = _noXmitBitmap.allBitsClear();
    KaPmc730::setTxEnable(enableTx);
}

///////////////////////////////////////////////////////////
// Set the selected bit in _noXmitBitmap, then call updateTxEnableLineState()
// to set the state of the Tx Enable line based on the new bitmap.
void
setNoXmitBit(NoXmitBitmap::BITNUM bitnum) {
    // Set the selected bit in _noXmitBitmap, then set the state
    // of the Tx Enable line
    _noXmitBitmap.setBit(bitnum);
    updateTxEnableLineState();
}

///////////////////////////////////////////////////////////
// Clear the selected bit in _noXmitBitmap, then call updateTxEnableLineState()
// to set the state of the Tx Enable line based on the new bitmap.
void
clearNoXmitBit(NoXmitBitmap::BITNUM bitnum) {
    // Clear the selected bit in _noXmitBitmap, then set the state
    // of the Tx Enable line
    _noXmitBitmap.clearBit(bitnum);
    updateTxEnableLineState();
}

///////////////////////////////////////////////////////////
/// @brief Shutdown and cleanup function
void
shutDownAndClean() {
    ILOG << "Shutting down";
    // Drop the Transmit Enable line
    ILOG << "Lowering the transmit enable line";
    KaPmc730::setTxEnable(false);

    // Stop the various threads. These have no event loops, so they
    // must be stopped using "terminate()"
    _kaMonitor->terminate();
    _hThread->terminate();
    _vThread->terminate();
    _burstThread->terminate();

    // Wait for threads' termination (up to 1 second for each)
    _kaMonitor->wait(100);
    _hThread->wait(1000);
    _vThread->wait(1000);
    _burstThread->wait(1000);

    // Clean up dynamically allocated objects
    delete(_kaMonitor);
    delete(_hThread);
    delete(_vThread);
    delete(_burstThread);

    // stop the DAC
    _sd3c->upconverter()->stopDAC();

    // stop the timers
    ILOG << "Stopping the timers";
    _sd3c->timersStartStop(false);
    ILOG << "Stopping the timers - done";

    ILOG << "kadrx is finished";
}

///////////////////////////////////////////////////////////
/// @brief Return true iff sync pulses are being generated by the Pentek
void
exitIfNoSyncPulses() {
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
        shutDownAndClean();
        exit(1);
    }
    return;
}

//////////////////////////////////////////////////////////////////////
/// @brief If the _hup or _usr1 flag is set, perform the associated actions.
///
/// This function should be called frequently for timely response to the
/// received signals. It performs heavy lifting that should not be done in
/// the signal handlers.
void actOnFlags() {
    if (! _hup && ! _usr1) {
        return;
    }
    // We should not see both _hup and _usr1 true. If both are true, report
    // a bug and ignore _usr1. I.e., if the action is unclear, favor _hup
    // which disables transmit.
    if (_hup && _usr1) {
        ELOG << "BUG: The _hup and _usr1 flags are both set. " <<
                "Ignoring _usr1.";
        _usr1 = false;
    }

    // If _hup is true, disable transmit by setting the HUP_SIGNAL_RECEIVED
    // bit. If _usr1 is true, clear the HUP_SIGNAL_RECEIVED bit to allow
    // transmit if no other NoXmitBitmap bits are set.
    if (_hup) {
      ILOG << "_hup flag is set...disabling transmit";
      // Set the HUP_SIGNAL_RECEIVED bit in the NoXmitBitmap
      setNoXmitBit(NoXmitBitmap::HUP_SIGNAL_RECEIVED);
      _hup = false;
    } else if (_usr1) {
      if (! _noXmitBitmap.bitIsSet(NoXmitBitmap::HUP_SIGNAL_RECEIVED)) {
          WLOG << "_usr1 flag ignored because transmit is not currently " <<
                  "disabled by HUP signal";
      } else {
          ILOG << "_usr1 flag is set...canceling HUP transmit disable";
          clearNoXmitBit(NoXmitBitmap::HUP_SIGNAL_RECEIVED);
          exitIfNoSyncPulses();
      }
      _usr1 = false;
    }
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

    // Set the NOXMIT_IN_BLANKING_SECTOR bit
    DLOG << "Entered blanking sector";
    setNoXmitBit(NoXmitBitmap::IN_BLANKING_SECTOR);

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

    // Clear the IN_BLANKING_SECTOR bit
    DLOG << "Left blanking sector";
    clearNoXmitBit(NoXmitBitmap::IN_BLANKING_SECTOR);

    KaOscControl::theControl().setBlankingEnabled(false, pulseSeqNum);
}

///////////////////////////////////////////////////////////
void
verifyPpsAndNtp() {
    if (! _kaMonitor->gpsTimeServerGood()) {
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

///////////////////////////////////////////////////////////
/// @brief Function which is called on a periodic basis to log
/// some basic status.
void
logStatus() {
    ILOG << std::setprecision(3) << std::setw(5) << "H channel " <<
            _hThread->downconverter()->bytesRead() / (1.0e6 * STATUS_INTERVAL_SECS) <<
            " MB/s, drop: " << _hThread->downconverter()->droppedPulses() <<
            ", sync errs: " << _hThread->downconverter()->syncErrors();

    ILOG << std::setprecision(3) << std::setw(5) << "V channel " <<
            _vThread->downconverter()->bytesRead() / (1.0e6 * STATUS_INTERVAL_SECS) <<
            " MB/s, drop: " << _vThread->downconverter()->droppedPulses() <<
            " sync errs: " << _vThread->downconverter()->syncErrors();

    ILOG << std::setprecision(3) << std::setw(5) << "burst channel " <<
            _burstThread->downconverter()->bytesRead() / (1.0e6 * STATUS_INTERVAL_SECS) <<
            " MB/s, drop: " << _burstThread->downconverter()->droppedPulses() <<
            " sync errs: " << _burstThread->downconverter()->syncErrors();
}

///////////////////////////////////////////////////////////
/// @brief Function which is called on a periodic basis to check N2
/// waveguide pressure, and set or clear the associated NoXmitBitmap bit.
void
checkN2Pressure() {
    // Set or clear the N2_PRESSURE_LOW bit based on the current
    // state of the N2 pressure switch
    KaPmc730::wgPressureGood() ?
            clearNoXmitBit(NoXmitBitmap::N2_PRESSURE_LOW) :
            setNoXmitBit(NoXmitBitmap::N2_PRESSURE_LOW);
}

/// @brief xmlrpc_c::method to get status from the kadrx process.
///
/// The method returns a xmlrpc_c::value_struct (dictionary) mapping
/// std::string keys to xmlrpc_c::value values. The dictionary can be
/// passed to the constructor for the DrxStatus class to create a DrxStatus
/// object.
///
/// Example client usage, where kadrx is running on machine `drxhost`:
/// @code
///     #include <xmlrpc-c/client_simple.hpp>
///     #include <KadrxStatus.h>
///     ...
///
///     // Execute "getStatus" on drxhost/port 8081
///     xmlrpc_c::clientSimple client;
///     xmlrpc_c::value result;
///     client.call("http://drxhost:8081/RPC2", "getStatus", "", &result);
///
///     // Instantiate a KadrxStatus using the returned dictionary
///     xmlrpc_c::value_struct statusDict(result);
///     KadrxStatus status(statusDict);
/// @endcode
class GetStatusMethod : public xmlrpc_c::method {
public:
    GetStatusMethod() {
        this->_signature = "S:";
        this->_help = "This method returns the latest status from kadrx.";
    }
    void
    execute(const xmlrpc_c::paramList & paramList, xmlrpc_c::value* retvalP) {
        DLOG << "Received 'getStatus' XML-RPC command";
        // Construct a KadrxStatus from the current values
        KadrxStatus status(_noXmitBitmap,
                           _afcEnabled,
                           _kaMonitor->gpsTimeServerGood(),
                           _kaMonitor->locked100MHz(),
                           _kaMonitor->wgPressureGood(),
                           _kaMonitor->osc0Frequency(),
                           _kaMonitor->osc1Frequency(),
                           _kaMonitor->osc2Frequency(),
                           _kaMonitor->osc3Frequency(),
                           _kaMonitor->derivedTxFrequency(),
                           _kaMonitor->hTxPowerRaw(),
                           _kaMonitor->vTxPowerRaw(),
                           _kaMonitor->testTargetPowerRaw(),
                           _kaMonitor->procDrxTemp(),
                           _kaMonitor->procEnclosureTemp(),
                           _kaMonitor->rxBackTemp(),
                           _kaMonitor->rxFrontTemp(),
                           _kaMonitor->rxTopTemp(),
                           _kaMonitor->txEnclosureTemp(),
                           _kaMonitor->psVoltage());
        *retvalP = status.toXmlRpcValue();
    }
};

/////////////////////////////////////////////////////////////////////
/// @brief xmlrpc_c::method to raise the Ka transmitter serial reset line.
///
/// Control of the transmitter serial reset is really the concern of ka_xmitd,
/// but access to the line is via the PMC-730 card, which is owned here. Hence,
/// we have to provide an API for driving the line.
class RaiseXmitTtyResetMethod : public xmlrpc_c::method {
public:
    RaiseXmitTtyResetMethod() {
        this->_signature = "n:";
        this->_help = "This method raises the transmitter TTY reset line.";
    }
    void
    execute(const xmlrpc_c::paramList & paramList, xmlrpc_c::value * retvalP) {
        ILOG << "Received 'raiseXmitTtyReset' XML-RPC command";
        KaPmc730::setTxSerialResetLine(true);
    }
};

/////////////////////////////////////////////////////////////////////
/// @brief xmlrpc_c::method to lower the Ka transmitter serial reset line
///
/// Control of the transmitter serial reset is really the concern of ka_xmitd,
/// but access to the line is via the PMC-730 card, which is owned here. Hence,
/// we have to provide an API for driving the line.
class LowerXmitTtyResetMethod : public xmlrpc_c::method {
public:
    LowerXmitTtyResetMethod() {
        this->_signature = "n:";
        this->_help = "This method lowers the transmitter TTY reset line.";
    }
    void
    execute(const xmlrpc_c::paramList & paramList, xmlrpc_c::value * retvalP) {
        ILOG << "Received 'lowerXmitTtyReset' XML-RPC command";
        KaPmc730::setTxSerialResetLine(false);
    }
};

/////////////////////////////////////////////////////////////////////
/// @brief xmlrpc_c::method to disable transmit in a blanking sector.
///
/// Returns 0 if the method is successful, or -1 if XML-RPC calls for blanking
/// are currently disallowed.
class SetBlankingOnMethod : public xmlrpc_c::method {
public:
    SetBlankingOnMethod() {
        this->_signature = "i:ddd";
        this->_help = "This method disables transmit in a blanking sector.";
    }
    void
    execute(const xmlrpc_c::paramList & paramList, xmlrpc_c::value * retvalP) {
        if (! _allowBlanking) {
            DLOG << "Blanking via XML-RPC calls is currently disallowed";
            *retvalP = xmlrpc_c::value_int(-1);
            return;
        }
        double epochSeconds(paramList.getDouble(0));
        double elev(paramList.getDouble(1));
        double azim(paramList.getDouble(2));
        DLOG << "Received 'setBlankingOn' command with time: " <<
                epochSeconds << ", elev: " << elev << ", azim: " << azim;
        boost::posix_time::ptime pTime =
                boost::posix_time::from_time_t(int(epochSeconds));
        int usecs = int(1.0e6 * fmod(epochSeconds, 1.0));
        pTime += boost::posix_time::microseconds(usecs);
        int64_t pulseSeqNum = _sd3c->pulseAtTime(pTime);
        setBlankingOn(pulseSeqNum);
        *retvalP = xmlrpc_c::value_int(0);
    }
};

/////////////////////////////////////////////////////////////////////
/// @brief xmlrpc_c::method to allow transmit after leaving a blanking sector.
///
/// Returns 0 if the method is successful, or -1 if XML-RPC calls for blanking
/// are currently disallowed.
class SetBlankingOffMethod : public xmlrpc_c::method {
public:
    SetBlankingOffMethod() {
        this->_signature = "i:ddd";
        this->_help = "This method allows transmit after leaving a blanking sector.";
    }
    void
    execute(const xmlrpc_c::paramList & paramList, xmlrpc_c::value * retvalP) {
        if (! _allowBlanking) {
            DLOG << "Blanking via XML-RPC calls is currently disallowed";
            *retvalP = xmlrpc_c::value_int(-1);
            return;
        }
        double epochSeconds(paramList.getDouble(0));
        double elev(paramList.getDouble(1));
        double azim(paramList.getDouble(2));
        DLOG << "Received 'setBlankingOff' command with time: " <<
                epochSeconds << ", elev: " << elev << ", azim: " << azim;
        boost::posix_time::ptime pTime =
                boost::posix_time::from_time_t(int(epochSeconds));
        int usecs = int(1.0e6 * fmod(epochSeconds, 1.0));
        pTime += boost::posix_time::microseconds(usecs);
        int64_t pulseSeqNum = _sd3c->pulseAtTime(pTime);
        setBlankingOff(pulseSeqNum);
        *retvalP = xmlrpc_c::value_int(0);
    }
};

/////////////////////////////////////////////////////////////////////
/// @brief xmlrpc_c::method to set the XMLRPC_REQUEST 'no transmit' bit.
/// If this method is called, then the XML-RPC "enableTransmit" method must be
/// called to re-enable transmit.
class DisableTransmitMethod : public xmlrpc_c::method {
public:
    DisableTransmitMethod() {
        this->_signature = "n:";
        this->_help = "This method sets the XMLRPC_REQUEST 'no transmit' bit";
    }
    void
    execute(const xmlrpc_c::paramList & paramList, xmlrpc_c::value* retvalP) {
        DLOG << "Received 'disableTransmit' XML-RPC command";
        // set the XMLRPC_REQUEST bit
        setNoXmitBit(NoXmitBitmap::XMLRPC_REQUEST);
    }
};

/////////////////////////////////////////////////////////////////////
/// @brief xmlrpc_c::method to clear the XMLRPC_REQUEST 'no transmit' bit.
class EnableTransmitMethod : public xmlrpc_c::method {
public:
    EnableTransmitMethod() {
        this->_signature = "n:";
        this->_help = "This method clears the XMLRPC_REQUEST 'no transmit' bit";
    }
    void
    execute(const xmlrpc_c::paramList & paramList, xmlrpc_c::value* retvalP) {
        DLOG << "Received 'enableTransmit' XML-RPC command";
        // clear the XMLRPC_REQUEST bit
        clearNoXmitBit(NoXmitBitmap::XMLRPC_REQUEST);
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

    // Parse out our command line options
    parseOptions(argc, argv);

    // Log start time
    boost::posix_time::ptime now(boost::posix_time::second_clock::universal_time());
    ILOG << "kadrx start at: " << now;

    ILOG << "Git commit " << REPO_REVISION << "," << REPO_EXTERNALS;

    // QApplication
    _app = new QCoreApplication(argc, argv);

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

    // Start out assuming that N2 waveguide pressure is low until we confirm
    //otherwise.
    _noXmitBitmap.setBit(NoXmitBitmap::N2_PRESSURE_LOW);

    // Start with the IN_BLANKING_SECTOR bit set if we're allowing for
    // blanking (we'll blank until we're explicitly told we're *not* in a
    // blanking sector), otherwise false (since no sectors will be blanked).
    _allowBlanking = kaConfig.allow_blanking();
    if (_allowBlanking) {
        _noXmitBitmap.setBit(NoXmitBitmap::IN_BLANKING_SECTOR);
    } else {
        _noXmitBitmap.clearBit(NoXmitBitmap::IN_BLANKING_SECTOR);
    }

    // Make sure our KaPmc730 is created in simulation mode if requested
    KaPmc730::doSimulate(kaConfig.simulate_pmc730());

    // set to ignore SIGPIPE errors which occur when sockets
    // are broken between client and server
    signal(SIGPIPE, SIG_IGN);

    // Create our status monitoring thread.
    _kaMonitor = new KaMonitor(_xmitdHost, _xmitdPort);

    // create the merge object (which is also the IWRF TCP server)
    KaMerge merge(kaConfig, *_kaMonitor);

    // Figure out the lowest SD3C timer clock divisor which will support the
    // given PRT(s). Allowed divisor values are 2, 4, 8, or 16.
    double maxPrt = fmax(kaConfig.prt1(), kaConfig.prt2());
    double adcFrequency = p7142sd3c::DefaultAdcFrequency(p7142sd3c::DDC10DECIMATE);
    int sd3cTimerDivisor = 2;
    for (; sd3cTimerDivisor <= 16; sd3cTimerDivisor *= 2) {
        // Get PRT counts in SD3C timer clock units (ADC clock / divisor). We
        // assume the default ADC clock for DDC10.
        double prtCounts = maxPrt * (adcFrequency / sd3cTimerDivisor);

        // Highest count which can be used for a SD3C timer is 65535, so if
        // our PRT count is below that, we're good using this divisor. Otherwise
        // double the divisor and try again.
        if (prtCounts <= 65535) {
            break;
        }
    }

    if (sd3cTimerDivisor > 16) {
        ELOG << "PRT of " << 1.0e6 * maxPrt << " us is too large to fit " <<
                "in an SD3C timer, even using the max timer clock divider!";
        ELOG << "Exiting kadrx";
        exit(1);
    }

    if (sd3cTimerDivisor > 2) {
        ILOG << "Using SD3C timer divisor of " << sd3cTimerDivisor <<
                " to fit a PRT of " << 1.0e6 * maxPrt << " us";
    }

    // Instantiate our p7142sd3c

    PMU_auto_register("pentek initialize");
    _sd3c = new p7142sd3c(false,    // simulate?
                          kaConfig.tx_delay(),
                          kaConfig.tx_pulse_width(),
                          kaConfig.prt1(),
                          kaConfig.prt2(),
                          kaConfig.staggered_prt(),
                          kaConfig.gates(),
                          1,        // nsum
                          false,    // free run
                          p7142sd3c::DDC10DECIMATE,
                          kaConfig.external_start_trigger(),
                          50,       // sim pause, ms
                          true,     // use first card
                          false,    // rim
                          0,        // code length (N/A for us)
                          0,        // ADC clock (0 = use default for our DDC type)
                          true,     // reset_clock_managers
                          true,     // abortOnError
                          sd3cTimerDivisor
                          );

    // Die if the card has DDC10 rev. 783 loaded, since it has a known problem
    // with timer programming
    if (_sd3c->sd3cRev() == 783) {
        ELOG << "Timer programming in SD3C DDC10 revision 783 is broken!";
        ELOG << "Load revision 535, or a revision later than 783.";
        ELOG << "EXITING kadrx";
        exit(1);
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
    _hThread = new KaDrxPub(*_sd3c, KaDrxPub::KA_H_CHANNEL, kaConfig, &merge,
                            _tsLength, _gaussianFile, _kaiserFile, _simPauseMs,
                            _simWavelength);

    // V channel (1)
    _vThread = new KaDrxPub(*_sd3c, KaDrxPub::KA_V_CHANNEL, kaConfig, &merge,
                            _tsLength, _gaussianFile, _kaiserFile, _simPauseMs,
                            _simWavelength);

    // Burst channel (2)
    _burstThread = new KaDrxPub(*_sd3c, KaDrxPub::KA_BURST_CHANNEL, kaConfig,
                                &merge, _tsLength, _gaussianFile, _kaiserFile,
                                _simPauseMs, _simWavelength);

    // Create the upConverter.
    // Configure the DAC to use CMIX by fDAC/4 (coarse mixer mode = 9)
    PMU_auto_register("create upconverter");
    _sd3c->addUpconverter(_sd3c->adcFrequency(), _sd3c->adcFrequency() / 4, 9);

    // Set up oscillator control/AFC
    PMU_auto_register("oscillator control");
    KaOscControl::createTheControl(kaConfig,
            _burstThread->downconverter()->dataInterruptPeriod());

    _afcEnabled = kaConfig.afc_enabled();
    if (! _afcEnabled) {
        WLOG << "AFC is disabled!";
    }

    // Shut down on SIGINT (from control-C) or SIGTERM (the default from 'kill')
    signal(SIGINT, sigHandler);
    signal(SIGTERM, sigHandler);

    // Disable transmit upon receipt of HUP signal. The associated disable
    // bit will remain set until a USR1 signal is received.
    signal(SIGHUP, hupHandler);

    // On receipt of USR1 signal, release transmit disable which was initiated
    // by a HUP signal. Other possible reasons for transmit disable will still
    // apply.
    signal(SIGUSR1, usr1Handler);

    // Start monitor and merge
    PMU_auto_register("start monitor and merge");
    _kaMonitor->start();
    merge.start();

    // Start the downconverter threads.
    PMU_auto_register("start threads");
    _hThread->start();
    _vThread->start();
    _burstThread->start();

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
    startUpconverter(_sd3c);

    // If we're going to start timers based on an external trigger (i.e., 1 PPS
    // from the GPS time server), make sure the time server is OK. Also make
    // sure our system time is close enough that we can calculate the correct
    // start second.
    if (kaConfig.external_start_trigger()) {
        verifyPpsAndNtp();
    }

    // Start the timers, which will allow data to flow.
    _sd3c->timersStartStop(true);

    // Verify that we have TX sync pulses being generated, or exit now
    exitIfNoSyncPulses();

    // Start our XML-RPC server on port 8081
    xmlrpc_c::registry myRegistry;
    myRegistry.addMethod("getStatus", new GetStatusMethod);
    myRegistry.addMethod("raiseXmitTtyReset", new RaiseXmitTtyResetMethod);
    myRegistry.addMethod("lowerXmitTtyReset", new LowerXmitTtyResetMethod);
    myRegistry.addMethod("disableTransmit", new DisableTransmitMethod);
    myRegistry.addMethod("enableTransmit", new EnableTransmitMethod);
    myRegistry.addMethod("setBlankingOn", new SetBlankingOnMethod);
    myRegistry.addMethod("setBlankingOff", new SetBlankingOffMethod);
    QXmlRpcServerAbyss rpcServer(&myRegistry, 8081);

    // Create a QFunctionWrapper and QTimer to periodically log our status
    QFunctionWrapper qLogStatus(logStatus);

    QTimer statusTimer(_app);
    QObject::connect(&statusTimer, SIGNAL(timeout()),
                     &qLogStatus, SLOT(callFunction()));
    statusTimer.setInterval(1000 * STATUS_INTERVAL_SECS);
    statusTimer.start();

    // Create a QFunctionWrapper and timer to periodically test N2 waveguide
    // pressure, and set or clear the associated NoXmitBitmap bit.
    QFunctionWrapper qCheckN2Pressure(checkN2Pressure);

    QTimer n2TestTimer(_app);
    QObject::connect(&n2TestTimer, SIGNAL(timeout()),
                     &qCheckN2Pressure, SLOT(callFunction()));
    n2TestTimer.setInterval(1000);      // check every 1000 ms
    n2TestTimer.start();

    // Create a QFunctionWrapper and timer to act on flags that are set
    // on receipt of HUP and USR1 signals.
    QFunctionWrapper qActOnFlags(actOnFlags);

    QTimer flagCheckTimer(_app);
    QObject::connect(&flagCheckTimer, SIGNAL(timeout()),
                     &qActOnFlags, SLOT(callFunction()));
    flagCheckTimer.setInterval(250);	// check every 250 ms
    flagCheckTimer.start();

    // Run the app until we're interrupted by SIGINT or SIGTERM
    _app->exec();

    shutDownAndClean();
    return(0);
}

