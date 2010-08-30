/*
 * main.cpp
 *
 *  Created on: Jan 21, 2009
 *      Author: martinc
 */

#include <QApplication>
#include <QPushButton>

#include <iostream>
#include <boost/program_options.hpp>
#include "QtConfig.h"
#include "ArgvParams.h"

#include "DDSSubscriber.h"
#include "AScopeReader.h"
#include "AScope.h"

std::string _ORB;                ///< path to the ORB configuration file.
std::string _DCPS;               ///< path to the DCPS configuration file.
std::string _DCPSInfoRepo;       ///< URL to access DCPSInfoRepo
std::string _tsTopic;            ///< The published timeseries topic
int _DCPSDebugLevel=0;           ///< the DCPSDebugLevel
int _DCPSTransportDebugLevel=0;  ///< the DCPSTransportDebugLevel

double _refreshHz;               ///< The scope refresh rate in Hz

std::string _title;              ///< The scope window title
std::string _saveDir;            ///< The image save directory

namespace po = boost::program_options;

//////////////////////////////////////////////////////////////////////
///
/// get parameters that are specified in the configuration file.
/// These can be overriden by command line specifications.
void getConfigParams()
{

	QtConfig config("KaScope", "KaScope");

	_saveDir = config.getString("SaveDir", ".");
	_title = config.getString("Title", "KAScope");

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

	_refreshHz    = config.getDouble("RefreshHz",  50.0);
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
    ("RefreshHz", po::value<double>(&_refreshHz), "Refresh rate (Hz)")
    ("ORB", po::value<std::string>(&_ORB), "ORB service configuration file (Corba ORBSvcConf arg)")
	("DCPS", po::value<std::string>(&_DCPS), "DCPS configuration file (OpenDDS DCPSConfigFile arg)")
	("DCPSInfoRepo", po::value<std::string>(&_DCPSInfoRepo), "DCPSInfoRepo URL (OpenDDS DCPSInfoRepo arg)")
	("DCPSDebugLevel", po::value<int>(&_DCPSDebugLevel), "DCPSDebugLevel ")
	("DCPSTransportDebugLevel", po::value<int>(&_DCPSTransportDebugLevel), "DCPSTransportDebugLevel ")
			;

	po::variables_map vm;
	po::store(po::parse_command_line(argc, argv, descripts), vm);
	po::notify(vm);

	if (vm.count("help")) {
		std::cout << descripts << std::endl;
		exit(1);
	}
}


int
main (int argc, char** argv) {


	// get the configuration parameters from the configuration file
	getConfigParams();

	// parse the command line optins, substituting for config params.
	parseOptions(argc, argv);

	ArgvParams newargv("kascope");
	newargv["-ORBSvcConf"] = _ORB;
	newargv["-DCPSConfigFile"] = _DCPS;
	newargv["-DCPSInfoRepo"] = _DCPSInfoRepo;
	if (_DCPSDebugLevel > 0)
		newargv["-DCPSDebugLevel"] = _DCPSDebugLevel;
	if (_DCPSTransportDebugLevel > 0)
		newargv["-DCPSTransportDebugLevel"] = _DCPSTransportDebugLevel;

	// create our DDS subscriber
	char **theArgv = newargv.argv();
	DDSSubscriber subscriber(newargv.argc(), theArgv);
	if (subscriber.status()) {
		std::cerr << "Unable to create a subscriber, exiting." << std::endl;
		exit(1);
	}

	QApplication app(argc, argv);

	// create the data source reader
	AScopeReader reader(subscriber, _tsTopic);

 	// create the scope
	AScope scope(_refreshHz, _saveDir);
	scope.setWindowTitle(QString(_title.c_str()));
	scope.show();

	// connect the reader to the scope to receive new DDS data
	scope.connect(&reader, SIGNAL(newItem(AScope::TimeSeries)),
			&scope, SLOT(newTSItemSlot(AScope::TimeSeries)));
	// connect the scope to the reader to return used DDS data
	scope.connect(&scope, SIGNAL(returnTSItem(AScope::TimeSeries)),
			&reader, SLOT(returnItemSlot(AScope::TimeSeries)));

	return app.exec();
}
