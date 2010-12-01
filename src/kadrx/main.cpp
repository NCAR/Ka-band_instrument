#include <iomanip>
#include <iostream>
#include <string>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <fcntl.h>
#include <stdio.h>
#include <math.h>
#include <sched.h>
#include <sys/timeb.h>
#include <ctime>
#include <cerrno>
#include <cstdlib>
#include <boost/program_options.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <csignal>

// For configuration management
#include <QtConfig.h>
// Proxy argc/argv
#include <ArgvParams.h>

// This is required for some tests below which try to work around issues
// with OpenDDS 2.0/2.1
#include <dds/Version.h>

#include "KaDrxPub.h"
#include "p7142sd3c.h"
#include "DDSPublisher.h"
#include "KaTSWriter.h"

#include "KaDrxConfig.h"

using namespace std;
using namespace boost::posix_time;
namespace po = boost::program_options;

bool _publish;                   ///< set true if the pentek data should be published to DDS.
std::string _devRoot;            ///< Device root e.g. /dev/pentek/0
std::string _drxConfig;          ///< DRX configuration file
int _chans = KaDrxPub::KA_N_CHANNELS;                  ///< number of channels
std::string _ORB;                ///< path to the ORB configuration file.
std::string _DCPS;               ///< path to the DCPS configuration file.
std::string _DCPSInfoRepo;       ///< URL to access DCPSInfoRepo
std::string _tsTopic;            ///< The published timeseries topic
int _DCPSDebugLevel = 0;         ///< the DCPSDebugLevel
int _DCPSTransportDebugLevel = 0;///< the DCPSTransportDebugLevel
int _tsLength;                   ///< The time series length
std::string _gaussianFile = "";  ///< gaussian filter coefficient file
std::string _kaiserFile = "";    ///< kaiser filter coefficient file
DDSPublisher* _publisher = 0;    ///< The publisher.
KaTSWriter* _tsWriter = 0;         ///< The time series writer.
bool _simulate;                  ///< Set true for simulate mode
int _simWavelength;              ///< The simulated data wavelength, in samples
int _simPauseMs;                 ///< The number of millisecnds to pause when reading in simulate mode.

bool _terminate = false;         ///< set true to signal the main loop to terminate

/////////////////////////////////////////////////////////////////////
void sigHandler(int sig) {
    std::cout << "Interrupt received...termination may take a few seconds" << 
        std::endl;
    _terminate = true;
}

/////////////////////////////////////////////////////////////////////
void createDDSservices()
{
	ArgvParams argv("sd3cdrx");
	argv["-DCPSInfoRepo"] = _DCPSInfoRepo;
	argv["-DCPSConfigFile"] = _DCPS;
	if (_DCPSDebugLevel > 0)
		argv["-DCPSDebugLevel"] = _DCPSDebugLevel;
	if (_DCPSTransportDebugLevel > 0)
		argv["-DCPSTransportDebugLevel"] = _DCPSTransportDebugLevel;
	argv["-ORBSvcConf"] = _ORB;

	// create our DDS publisher
	char **theArgv = argv.argv();
	_publisher = new DDSPublisher(argv.argc(), theArgv);
	if (_publisher->status()) {
		std::cerr << "Unable to create a publisher, exiting." << std::endl;
		exit(1);
	}

	// create the DDS time series writer
	_tsWriter = new KaTSWriter(*_publisher, _tsTopic.c_str());
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
		std::cerr << "Environment variable KADIR must be set." << std::endl;
		exit(1);
	}

	// and create the default DDS configuration file paths, since these
	// depend upon KADIR
	std::string orbFile      = KaDir + "ORBSvc.conf";
	std::string dcpsFile     = KaDir + "DDSClient.ini";
	std::string dcpsInfoRepo = "iiop://localhost:50000/DCPSInfoRepo";

	// get parameters
	_publish       = config.getBool  ("DDS/Publish",        true);
	_ORB           = config.getString("DDS/ORBConfigFile",  orbFile);
	_DCPS          = config.getString("DDS/DCPSConfigFile", dcpsFile);
	_tsTopic       = config.getString("DDS/TopicTS",        "KATS");
	_DCPSInfoRepo  = config.getString("DDS/DCPSInfoRepo",   dcpsInfoRepo);
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
	("nopublish",                                  "Do not publish data")
	("simulate",                                   "Enable simulation")
	("simPauseMS",  po::value<int>(&_simPauseMs),  "Simulation pause interval (ms)")
	("ORB", po::value<std::string>(&_ORB),         "ORB service configuration file (Corba ORBSvcConf arg)")
	("DCPS", po::value<std::string>(&_DCPS),       "DCPS configuration file (OpenDDS DCPSConfigFile arg)")
	("DCPSInfoRepo", po::value<std::string>(&_DCPSInfoRepo),
	                                               "DCPSInfoRepo URL (OpenDDS DCPSInfoRepo arg)")
	("DCPSDebugLevel", po::value<int>(&_DCPSDebugLevel), "DCPSDebugLevel")
	("DCPSTransportDebugLevel", po::value<int>(&_DCPSTransportDebugLevel),
			                                       "DCPSTransportDebugLevel")
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

	if (vm.count("nopublish"))
	    _publish = false;
	if (vm.count("simulate"))
	    _simulate = true;
	if (vm.count("drxConfig") != 1) {
	    std::cerr << "Exactly one DRX configuration file must be given!" << std::endl;
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
		std::cerr << "Not root, unable to change scheduling priority" << std::endl;
		return;
	}

	sched_param sparam;
	sparam.sched_priority = 50;

	if (sched_setscheduler(0, SCHED_RR, &sparam)) {
		std::cerr << "warning, unable to set scheduler parameters: ";
		perror("");
		std::cerr << "\n";
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
	long IQ[n];

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
int
main(int argc, char** argv)
{
	// try to change scheduling to real-time
	makeRealTime();

	// get the configuration parameters from the configuration file
	getConfigParams();

	// parse the command line options, substituting for config params.
	parseOptions(argc, argv);

	// Read the KA configuration file
    KaDrxConfig kaConfig(_drxConfig);
    if (! kaConfig.isValid()) {
        std::cerr << "Exiting on incomplete configuration!" << std::endl;
        exit(1);
    }
    
    // For OpenDDS 2.1, keep the published sample size less than ~64 KB,
    // since larger samples are a problem...
    if (DDS_MAJOR_VERSION == 2 && DDS_MINOR_VERSION == 1) {
        // this is just an approximation...
        int onePulseSize = sizeof(RadarDDS::SysHousekeeping) + kaConfig.gates() * 4;
        int maxTsLength = 65000 / onePulseSize;
        if (! maxTsLength) {
            std::cerr << "Cannot adjust tsLength to meet OpenDDS 2.1 " <<
                    "max sample size of 2^16 bytes" << std::endl;
            exit(1);
        } else if (_tsLength > maxTsLength) {
            int oldTsLength = _tsLength;
            // Set _tsLength to the greatest power of 2 which is <= maxTsLength
            _tsLength = 1;
            while ((_tsLength * 2) <= maxTsLength)
                _tsLength *= 2;
            std::cerr << "Adjusted tsLength from " << oldTsLength << " to " <<
                    _tsLength << " to stay under OpenDDS 2.1 64 KB sample size limit." <<
                    std::endl;
        }
    }

	// create the dds services
	if (_publish)
		createDDSservices();
	
    // Instantiate our p7142sd3c
    Pentek::p7142sd3c sd3c(_devRoot, _simulate, kaConfig.tx_delay(),
        kaConfig.tx_pulse_width(), kaConfig.prt1(), kaConfig.prt2(),
        kaConfig.staggered_prt(), kaConfig.gates(), 1, false,
        Pentek::p7142sd3c::DDC10DECIMATE, kaConfig.external_start_trigger());
    
    // We use SD3C's first general purpose timer for transmit pulse modulation
    sd3c.setGPTimer0(kaConfig.tx_pulse_mod_delay(), kaConfig.tx_pulse_mod_width());
    
	// Create (but don't yet start) the downconversion threads.
    
    // H channel (0)
    KaDrxPub hThread(sd3c, KaDrxPub::KA_H_CHANNEL, kaConfig, _tsWriter, _publish,
        _tsLength, _gaussianFile, _kaiserFile, _simPauseMs, _simWavelength); 

    // V channel (1)
    KaDrxPub vThread(sd3c, KaDrxPub::KA_V_CHANNEL, kaConfig, _tsWriter, _publish,
        _tsLength, _gaussianFile, _kaiserFile, _simPauseMs, _simWavelength); 

    // Burst channel (2)
    KaDrxPub burstThread(sd3c, KaDrxPub::KA_BURST_CHANNEL, kaConfig, _tsWriter, 
        _publish, _tsLength, _gaussianFile, _kaiserFile, _simPauseMs, _simWavelength); 

    // Create the upConverter.
    // Configure the DAC to use CMIX by fDAC/4 (coarse mixer mode = 9)
    Pentek::p7142Up & upConverter = *sd3c.addUpconverter("0C", 
        sd3c.adcFrequency(), sd3c.adcFrequency() / 4, 9); 

    // catch a control-C
    signal(SIGINT, sigHandler);

    // Start the downconverter threads.
    hThread.start();
    vThread.start();
    burstThread.start();

    // wait awhile, so that the threads can all get to the first read.
	struct timespec sleepTime = { 1, 0 }; // 1 second, 0 nanoseconds
	while (nanosleep(&sleepTime, &sleepTime)) {
	    if (errno != EINTR) {
	        std::cerr << "Error " << errno << " from nanosleep().  Aborting." <<
                std::endl;
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

	// Start the timers, which will allow data to flow.
    sd3c.timersStartStop(true);

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

        std::cout << std::setprecision(3) << std::setw(5) << "H channel " << 
                hThread.downconverter()->bytesRead() * 1.0e-6 / elapsed <<
                " MB/s  ovr: " << hThread.downconverter()->overUnderCount() <<
                " nopub: " << hThread.tsDiscards() <<
                " drop: " << hThread.downconverter()->droppedPulses() <<
                " sync errs: " << hThread.downconverter()->syncErrors() << 
                std::endl;
        
        std::cout << std::setprecision(3) << std::setw(5) << "V channel " << 
                vThread.downconverter()->bytesRead() * 1.0e-6 / elapsed <<
                " MB/s  ovr: " << vThread.downconverter()->overUnderCount() <<
                " nopub: " << vThread.tsDiscards() <<
                " drop: " << vThread.downconverter()->droppedPulses() <<
                " sync errs: " << vThread.downconverter()->syncErrors() << 
                std::endl;
        
        std::cout << std::setprecision(3) << std::setw(5) << "burst channel " << 
                burstThread.downconverter()->bytesRead() * 1.0e-6 / elapsed <<
                " MB/s  ovr: " << burstThread.downconverter()->overUnderCount() <<
                " nopub: " << burstThread.tsDiscards() <<
                " drop: " << burstThread.downconverter()->droppedPulses() <<
                " sync errs: " << burstThread.downconverter()->syncErrors() << 
                std::endl;
	}
    
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

	std::cout << "terminated on command" << std::endl;
}

