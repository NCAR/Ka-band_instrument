/*
 * main.cpp
 *
 * Created Nov 2010
 *      Author: Mike Dixon
 *      Author: Based on code by Charlie Martin
 *      EOL, NCAR, Boulder, CO, USA
 */

#include <QApplication>
#include <QPushButton>

#include <iostream>
#include <boost/program_options.hpp>
#include "QtConfig.h"
#include "AScopeReader.h"
#include "AScope.h"

double _refreshHz;       ///< The scope refresh rate in Hz
std::string _serverHost; ///< The host name for the time series server
int _serverPort;         ///< The port for the time series server
int _debugLevel;
std::string _saveDir;            ///< The image save directory
std::string _title;

namespace po = boost::program_options;

//////////////////////////////////////////////////////////////////////
///
/// get parameters that are specified in the configuration file.
/// These can be overriden by command line specifications.
void getConfigParams()
{

  QtConfig config("TcpScope", "TcpScope");
  
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
  
  _refreshHz    = config.getDouble("RefreshHz",  50.0);
  _serverHost = "localhost";
  _serverPort = 10000;
  _debugLevel = 0;

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
    ("host", po::value<std::string>(&_serverHost), "Set the server host")
    ("port", po::value<int>(&_serverPort), "Set the server port")
    ("debug", po::value<int>(&_debugLevel),
     "Set the debug level: 0, 1, or 2. 0 is the default")
    ;

  po::variables_map vm;
  try {
    po::store(po::parse_command_line(argc, argv, descripts), vm);
  }
  catch(std::exception ex) {
    std::cerr << "ERROR parsing command line" << std::endl;
    std::cerr << descripts << std::endl;
    exit(1);
  }
  po::notify(vm);

//   if (vm.count("help")) {
//     std::cout << descripts << std::endl;
//     exit(1);
//   }
}


int
  main (int argc, char** argv) {


  // get the configuration parameters from the configuration file
  getConfigParams();

  // parse the command line optins, substituting for config params.
  parseOptions(argc, argv);

  QApplication app(argc, argv);

  // create the data source reader

  AScopeReader reader(_serverHost, _serverPort);

  // create the scope

  AScope scope(_refreshHz, _saveDir);
  scope.setWindowTitle(QString(_title.c_str()));
  scope.show();

  // connect the reader to the scope to receive new time series data
  
  scope.connect(&reader, SIGNAL(newItem(AScope::TimeSeries)),
                &scope, SLOT(newTSItemSlot(AScope::TimeSeries)));
  
  // connect the scope to the reader to return used time series data

  scope.connect(&scope, SIGNAL(returnTSItem(AScope::TimeSeries)),
                &reader, SLOT(returnItemSlot(AScope::TimeSeries)));

  return app.exec();
}
