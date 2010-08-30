/*
 * main.cpp
 *
 *  Created on: Jan 20, 2009
 *      Author: martinc
 */

#include <iomanip>
#include <iostream>
#include <string>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <stdio.h>
#include <math.h>
#include <sched.h>
#include <sys/timeb.h>
#include <boost/program_options.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

// For configuration management
#include <QtConfig.h>
// Proxy argc/argv
#include <ArgvParams.h>

#include "DDSSubscriber.h"
#include "SnifferTSReader.h"

using namespace std;
using namespace boost::posix_time;
namespace po = boost::program_options;
using namespace RadarDDS;

bool _publish;                   ///< set true if the pentek data should be published to DDS.
std::string _devRoot;            ///< Device root e.g. /dev/pentek/0
std::string _dnName;             ///< Downconvertor name e.g. 0B
int _bufferSize;                 ///< Buffer size
std::string _ORB;                ///< path to the ORB configuration file.
std::string _DCPS;               ///< path to the DCPS configuration file.
std::string _DCPSInfoRepo;       ///< URL to access DCPSInfoRepo
std::string _tsTopic;            ///< The published timeseries topic
int _DCPSDebugLevel=0;           ///< the DCPSDebugLevel
int _DCPSTransportDebugLevel=0;  ///< the DCPSTransportDebugLevel
DDSSubscriber* _subscriber = 0;  ///< The publisher.
SnifferTSReader* _tsReader = 0;        ///< The time series writer.

/////////////////////////////////////////////////////////////////////
void createDDSservices()
{
	std::cout <<__FILE__ << " creating DDS services" << std::endl;

	ArgvParams argv("sniffer");
	argv["-ORBSvcConf"] = _ORB;
	argv["-DCPSConfigFile"] = _DCPS;
	argv["-DCPSInfoRepo"] = _DCPSInfoRepo;
	if (_DCPSDebugLevel > 0)
		argv["-DCPSDebugLevel"] = _DCPSDebugLevel;
	if (_DCPSTransportDebugLevel > 0)
		argv["-DCPSTransportDebugLevel"] = _DCPSTransportDebugLevel;

	// create our DDS subscriber
	char **theArgv = argv.argv();
	//    std::cerr << "theArgv[0] = " << theArgv[0] << std::endl;
	_subscriber = new DDSSubscriber(argv.argc(), theArgv);
	if (_subscriber->status()) {
		std::cerr << "Unable to create a subscriber, exiting." << std::endl;
		exit(1);
	}

	// create the time series writer
	_tsReader = new SnifferTSReader(*_subscriber, _tsTopic.c_str());
}

//////////////////////////////////////////////////////////////////////
///
/// get parameters that are specified in the configuration file.
/// These can be overriden by command line specifications.
void getConfigParams()
{

	QtConfig config("NCAR", "KaSniffer");

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
	_ORB          = config.getString("DDS/ORBConfigFile",  orbFile);
	_DCPS         = config.getString("DDS/DCPSConfigFile", dcpsFile);
	_tsTopic      = config.getString("DDS/TopicTS",        "KATS");
	_DCPSInfoRepo = config.getString("DDS/DCPSInfoRepo",   dcpsInfoRepo);
	_bufferSize   = config.getInt("Device/BufferSize",     100000);
	_devRoot      = config.getString("Device/DeviceRoot",  "/dev/pentek/p7140/0");
	_dnName       = config.getString("Device/DownName",    "0C");
	std::cerr << "read DCPSInfoRepo = " << _DCPSInfoRepo << " from config" << std::endl;
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
	("help", "describe options")
	("ORB", po::value<std::string>(&_ORB), "ORB service configuration file (Corba ORBSvcConf arg)")
	("DCPS", po::value<std::string>(&_DCPS), "DCPS configuration file (OpenDDS DCPSConfigFile arg)")
	("DCPSInfoRepo", po::value<std::string>(&_DCPSInfoRepo), "DCPSInfoRepo URL (OpenDDS DCPSInfoRepo arg)")
	("DCPSDebugLevel", po::value<int>(&_DCPSDebugLevel), "DCPSDebugLevel ")
	("DCPSTransportDebugLevel", po::value<int>(&_DCPSTransportDebugLevel),
			"DCPSTransportDebugLevel ")
			;

	po::variables_map vm;
	po::store(po::parse_command_line(argc, argv, descripts), vm);
	po::notify(vm);

	_publish = vm.count("nopublish") == 0;
	if (vm.count("help")) {
		std::cout << descripts << std::endl;
		exit(1);
	}
}

///////////////////////////////////////////////////////////
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
double nowTime()
{
	struct timeb timeB;
	ftime(&timeB);
	return timeB.time + timeB.millitm/1000.0;
}
///////////////////////////////////////////////////////////
int
main(int argc, char** argv)
{

	// get the configuration parameters from the configuration file
	getConfigParams();

	// parse the command line optins, substituting for config params.
	parseOptions(argc, argv);

	// create the dds services
	createDDSservices();

	// try to change scheduling to real-time
	makeRealTime();

	// start the loop
	double total = 0;
	double subTotal = 0;
	double startTime = nowTime();

	int lastMb = 0;
	int samples = 0;

	while (1) {

		KaTimeSeriesSequence* tss = _tsReader->nextTSS();

		if (!tss) {
			usleep(1000);    // 1 ms polling should be fine...
		} else {
			samples++;
			// sum data sizes from all of the pulses in this sequence
			for (unsigned int p = 0; p < tss->tsList.length(); p++) {
			    const KaTimeSeries &ts = tss->tsList[p];
			    total += ts.data.length() * sizeof(ts.data[0]);
			    subTotal += ts.data.length() * sizeof(ts.data[0]);
			}
			_tsReader->returnItem(tss);

			int mb = (int)(total/1.0e6);
			if ((mb % 100) == 0 && mb > lastMb) {
				lastMb = mb;
				double elapsed = nowTime() - startTime;
				double bw = (subTotal/elapsed)/1.0e6;
				startTime = nowTime();
				subTotal = 0;

				std::cout << "total " << std::setw(5) << mb << " MB,  BW "
				<< std::setprecision(4) << std::setw(5) << bw
				<< " MB/s  samples " << samples << std::endl;
			}
		}
	}
}

