#include <ArgvParams.h>
#include <QtConfig.h>
#include <DDSPublisher.h>
#include <boost/program_options.hpp>

#include "ProductPlayback.h"
#include "ProductWriter.h"

namespace po = boost::program_options;

// Current write rate, in MB/s
float WriteRate = 0.0;

// The Ka configuration directory ($KADIR/conf)
std::string KaConfigDir;
std::string DCPSInfoRepo;
std::string OrbConfigFile;
std::string DCPSConfigFile;
int DCPSDebugLevel = 0;
int DCPSTransportDebugLevel = 0;
float PlaybackSpeed = 1.0;
QStringList FileNames;


void
parseCommandLine(int argc, char* argv[]) {
    std::vector<std::string> files;
    // Options valid from config file or command line
    po::options_description genericOpts("Generic options");
    genericOpts.add_options()
        ("help", "Describe options")
        ("speed", po::value<float>(&PlaybackSpeed), "Playback speed, relative to real-time")
        ("ORB", po::value<std::string>(&OrbConfigFile), "ORB service configuration file (Corba ORBSvcConf arg)")
        ("DCPS", po::value<std::string>(&DCPSConfigFile), "DCPS configuration file (OpenDDS DCPSConfigFile arg)")
        ("DCPSInfoRepo", po::value<std::string>(&DCPSInfoRepo), "DCPSInfoRepo URL")
        ("DCPSDebugLevel", po::value<int>(&DCPSDebugLevel), "DCPS debug level")
        ("DCPSTransportDebugLevel", po::value<int>(&DCPSTransportDebugLevel),
            "DCPS transport debug level");

    // We don't really want people to add "--file <file>" for each file, but 
    // 'positional options' is the canonical way to get the remainder of the
    // line, and that requires having an associated 'real' option.  We just
    // don't display it in the help message...
    po::options_description hiddenOpts;
    hiddenOpts.add_options()
        ("file", po::value<std::vector<std::string> >(&files), "file to play back");
    
    po::options_description cmdlineOpts;
    cmdlineOpts.add(genericOpts).add(hiddenOpts);
    
    po::options_description visibleOpts;
    visibleOpts.add(genericOpts);
    
    po::positional_options_description positionalOpts;
    positionalOpts.add("file", -1);

    po::variables_map vm;
    // Enhance the default style by allowing -<option> in addition to --<option>
    int style = po::command_line_style::default_style | po::command_line_style::allow_long_disguise;
    po::store(po::command_line_parser(argc, argv).
            options(cmdlineOpts).positional(positionalOpts).style(style).run(), vm);
    po::notify(vm);
    
    if (vm.count("help")) {
        std::cout << visibleOpts << std::endl;
        exit(1);
    }
    if (vm.count("file")) {
        for (unsigned int i = 0; i < files.size(); i++) {
            FileNames << files[i].c_str();
        }
    } else {
        std::cerr << "At least one file must be given on the command line!" << std::endl;
        exit(1);
    }
}


int
main(int argc, char *argv[])
{
    // Configuration

    // get the configuration
    QtConfig config("ProductPlayback", "ProductPlayback");

    // set up the configuration directory path: $KADIR/conf/
    char* e = getenv("KADIR");
    if (!e) {
        std::cerr << "Environment variable KADIR must be set." << std::endl;
        exit(1);
    }
    
    DCPSInfoRepo = config.getString("DDS/DCPSInfoRepo", 
            "iiop://localhost:50000/DCPSInfoRepo");
    
    KaConfigDir = std::string(e) + "/conf/";
    OrbConfigFile = config.getString("DDS/ORBConfigFile", 
            KaConfigDir + "ORBSvc.conf");
    DCPSConfigFile = config.getString("DDS/DCPSConfigFile", 
            KaConfigDir + "DDSClient.ini");
    std::string productTopic = config.getString("TopicProduct", "KAPROD");
    
    parseCommandLine(argc, argv);

    // create the publisher
    ArgvParams pubParams(argv[0]);
    pubParams["-ORBSvcConf"] = OrbConfigFile;
    pubParams["-DCPSConfigFile"] = DCPSConfigFile;
    pubParams["-DCPSInfoRepo"] = DCPSInfoRepo;

    DDSPublisher publisher(pubParams.argc(), pubParams.argv());
    int pubStatus = publisher.status();
    if (pubStatus) {
        std::cerr << "Error " << pubStatus << " creating publisher" << std::endl;
        exit(pubStatus);
    }

    // create the DDS writer for outgoing time series
    ProductWriter *productWriter = new ProductWriter(publisher, productTopic);

    ProductPlayback pb(productWriter, FileNames, PlaybackSpeed);
    pb.run();
}


