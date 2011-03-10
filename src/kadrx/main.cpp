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

LOGGING("kadrx")

// For configuration management
#include <QtConfig.h>

#include "KaOscControl.h"
#include "KaDrxPub.h"
#include "KaPmc730.h"
#include "p7142sd3c.h"
#include "KaDrxConfig.h"
#include "KaMerge.h"
#include "KaMonitor.h"

using namespace std;
using namespace boost::posix_time;
namespace po = boost::program_options;

std::string _devRoot;            ///< Device root e.g. /dev/pentek/0
std::string _drxConfig;          ///< DRX configuration file
int _chans = KaDrxPub::KA_N_CHANNELS; ///< number of channels
int _tsLength;                   ///< The time series length
std::string _gaussianFile = "";  ///< gaussian filter coefficient file
std::string _kaiserFile = "";    ///< kaiser filter coefficient file
KaMerge* _merge = 0;             ///< The merge object - also IWRF TCP server
bool _simulate;                  ///< Set true for simulate mode
int _simWavelength;              ///< The simulated data wavelength, in samples
int _simPauseMs;                 ///< The number of millisecnds to pause when reading in simulate mode.

bool _terminate = false;         ///< set true to signal the main loop to terminate

/////////////////////////////////////////////////////////////////////
void sigHandler(int sig) {
    ILOG << "Interrupt received...termination may take a few seconds";
    _terminate = true;
}

//////////////////////////////////////////////////////////////////////
///
/// get parameters that are specified in the Qt configuration file.
/// These can be overridden by command line specifications.
void getConfigParams()
{

	QtConfig config("KaDrx", "KaDrx");

	// set up the default configuration directory path
	std::string KaDir("/conf/");
	char* e = getenv("KADIR");
	if (e) {
		KaDir = e + KaDir;
	} else {
		ELOG << "Environment variable KADIR must be set.";
		exit(1);
	}

	// and create the default DDS configuration file paths, since these
	// depend upon KADIR
	std::string orbFile      = KaDir + "ORBSvc.conf";
	std::string dcpsFile     = KaDir + "DDSClient.ini";
	std::string dcpsInfoRepo = "iiop://localhost:50000/DCPSInfoRepo";

	_devRoot       = config.getString("Device/DeviceRoot",  "/dev/pentek/p7142/0");
	_tsLength      = config.getInt   ("Radar/TsLength",     256);
	_simulate      = config.getBool  ("Simulate",           false);
	_simPauseMs    = config.getInt   ("SimPauseMs",         20);
	_simWavelength = config.getInt   ("SimWavelength",      5000);

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
	("simulate",                                   "Enable simulation")
	("simPauseMS",  po::value<int>(&_simPauseMs),  "Simulation pause interval (ms)")
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
    
	// get the configuration parameters from the configuration file
	getConfigParams();

	// parse the command line options, substituting for config params.
	parseOptions(argc, argv);

	// Read the KA configuration file
    KaDrxConfig kaConfig(_drxConfig);
    if (! kaConfig.isValid()) {
        ELOG << "Exiting on incomplete configuration!";
        exit(1);
    }

    // set to ignore SIGPIPE errors which occur when sockets
    // are broken between client and server

    signal(SIGPIPE, SIG_IGN);

    // create the merge object

    _merge = new KaMerge(kaConfig);
    
    // Reference to the singleton KaMonitor is sufficient to start it up...
    KaMonitor::theMonitor();
    
    // Turn off transmitter trigger enable until we know we're generating
    // timing signals (and hence that the T/R limiters are presumably 
    // operating).
    KaPmc730::setTxTriggerEnable(false);
    
    // Instantiate our p7142sd3c
    Pentek::p7142sd3c sd3c(_devRoot, _simulate, kaConfig.tx_delay(),
        kaConfig.tx_pulse_width(), kaConfig.prt1(), kaConfig.prt2(),
        kaConfig.staggered_prt(), kaConfig.gates(), 1, false,
        Pentek::p7142sd3c::DDC10DECIMATE, kaConfig.external_start_trigger());
    
    // Use SD3C's general purpose timer 0 (timer 3) for transmit pulse modulation
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
    Pentek::p7142Up & upConverter = *sd3c.addUpconverter("0C", 
        sd3c.adcFrequency(), sd3c.adcFrequency() / 4, 9); 

    // Set up oscillator control from the configuration. (The first reference 
    // to theControl() is what actually starts the oscillator control thread.)
    KaOscControl & oscControl = KaOscControl::theControl();
    if (kaConfig.afc_enabled()) {
        oscControl.setG0ThresholdDbm(kaConfig.afc_g0_threshold_dbm());
        oscControl.setCoarseStep(kaConfig.afc_coarse_step());
        oscControl.setFineStep(kaConfig.afc_fine_step());
        oscControl.setMaxDataLatency(burstThread.downconverter()->dataInterruptPeriod());
    } else {
        WLOG << "AFC is disabled!";
    }
    
    // catch a SIGINT (from control-C) or SIGTERM (the default from 'kill')
    signal(SIGINT, sigHandler);
    signal(SIGTERM, sigHandler);

    // Start the downconverter threads.
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
    sd3c.startFilters();

    // Load the DAC memory bank 2, clear the DACM fifo, and enable the 
    // DAC memory counters. This must take place before the timers are started.
    startUpConverter(upConverter, sd3c.txPulseWidthCounts());

    // start the merge
    _merge->start();

    // Start the timers, which will allow data to flow.
    sd3c.timersStartStop(true);
    
    // Verify that timers have started, by seeing that we have TX sync pulses
    // being generated, then raise the TX enable line.
    verifyTimersAndEnableTx();

	double startTime = nowTime();
	while (1) {
		for (int i = 0; i < 100; i++) {
			// check for the termination request
			if (_terminate) {
				break;
			}
			usleep(100000);
		}
		if (_terminate) {
			break;
		}

		double currentTime = nowTime();
		double elapsed = currentTime - startTime;
		startTime = currentTime;
        
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

