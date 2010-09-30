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
#include "p7142.h"
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
int _simWaveLength;              ///< The simulated data wavelength, in samples
int _simPauseMS;                 ///< The number of millisecnds to pause when reading in simulate mode.

bool _terminate = false;         ///< set true to signal the main loop to terminate

/////////////////////////////////////////////////////////////////////
void sigHandler(int sig) {
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
	_simPauseMS    = config.getInt   ("SimPauseMs",         20);
	_simWaveLength = config.getInt   ("SimWavelength",      5000);

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
	("simPauseMS",  po::value<int>(&_simPauseMS),  "Simulation pause interval (ms)")
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
void startUpConverter(Pentek::p7142up& upConverter, 
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

    if (_simulate)
        std::cout << "*** Operating in simulation mode" << std::endl;

	// create the dds services
	if (_publish)
		createDDSservices();
	
	// create the down converter threads. Remember that
	// these are multiply inherited from the down converters
	// and QThread. The threads are not run at creation, but
	// they do instantiate the down converters.
	std::vector<KaDrxPub*> down7142(_chans);

	for (int c = 0; c < _chans; c++) {

		std::cout << "*** Channel " << c << " ***" << std::endl;
		down7142[c] = new KaDrxPub(
                kaConfig,
                _tsWriter,
                _publish,
                _tsLength,
                _devRoot,
                c,
                _gaussianFile,
                _kaiserFile,
                false,
                _simulate,
                _simPauseMS,
                _simWaveLength);
		if (!down7142[c]->ok()) {
			std::cerr << "cannot access " << down7142[c]->dnName() << "\n";
			perror("");
			exit(1);
		}
	}

    // Create the upConverter.
    // Configure the DAC to use CMIX by fDAC/4 (coarse mixer mode = 9)
    Pentek::p7142up upConverter(_devRoot, "0C", down7142[0]->adcFrequency(), 
            down7142[0]->adcFrequency() / 4, 9, _simulate); 

    if (!upConverter.ok()) {
        std::cerr << "cannot access " << upConverter.upName() << "\n";
        exit(1);
    }

    // catch a control-C
    signal(SIGINT, sigHandler);

	for (int c = 0; c < _chans; c++) {
		// run the downconverter thread. This will cause the
		// thread code to call the run() method, which will
		// start reading data, but should block on the first
		// read since the timers and filters are not running yet.
		down7142[c]->start();
		std::cout << "processing enabled on " << down7142[c]->dnName() << std::endl;
	}

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

	// all of the filters are started by any call to
    // start filters(). So just call it for channel 0
    down7142[0]->startFilters();

	// Load the DAC memory bank 2, clear the DACM fifo, and enable the 
	// DAC memory counters. This must take place before the timers are started.
    unsigned int pulsewidth_counts = (unsigned int)
      (down7142[0]->rcvrPulseWidth() * down7142[0]->adcFrequency());
	startUpConverter(upConverter, pulsewidth_counts);

	// Start the timers, which will allow data to flow.
    // All timers are started by calling timerStartStop for
    // any one channel.
    down7142[0]->timersStartStop(true);

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

		std::vector<long> bytes(_chans);
		std::vector<int> overUnder(_chans);
		std::vector<unsigned long> discards(_chans);
		std::vector<unsigned long> droppedPulses(_chans);
		std::vector<unsigned long> syncErrors(_chans);
		
		for (int c = 0; c < _chans; c++) {
			bytes[c] = down7142[c]->bytesRead();
			overUnder[c] = down7142[c]->overUnderCount();
			discards[c] = down7142[c]->tsDiscards();
			droppedPulses[c] = down7142[c]->droppedPulses();
            syncErrors[c] = down7142[c]->syncErrors();
		}
		
		for (int c = 0; c < _chans; c++) {
			std::cout << std::setprecision(3) << std::setw(5)
                      << "chan " << c << " -- "
                      << bytes[c]/1000000.0/elapsed << " MB/s "
					  << " ovr:" << overUnder[c]
					  << " nopub:"<< discards[c]
					  << " drop:" << droppedPulses[c]
	                  << " sync:" << syncErrors[c] << std::endl;
		}
	}

	// stop the DAC
	upConverter.stopDAC();

	// stop the timers
    down7142[0]->timersStartStop(false);

	std::cout << "terminated on command" << std::endl;
}

