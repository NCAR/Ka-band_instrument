#include <iostream>
#include <csignal>

#include <ArgvParams.h>
#include <QtConfig.h>
#include <XmlRpc.h>
#include <DDSSubscriber.h>
#include <boost/date_time/posix_time/posix_time.hpp>

#include "svnInfo.h"
#include "ProductArchiver.h"


// Current write rate, in rays/second
float WriteRate = 0.0;

// The ka configuration directory ($KADIR/conf)
std::string KaConfigDir;


// Our RPC server
using namespace XmlRpc;
XmlRpcServer RpcSvr;

// Start()
// Null method, provided for convenience.
class StartMethod : public XmlRpcServerMethod {
public:
    StartMethod(XmlRpcServer* s) : XmlRpcServerMethod("Start", s) {}

    void execute(XmlRpcValue& params, XmlRpcValue& result)
    {
        result = 0;
        std::cout << "got start command" << std::endl;
    }
} startMethod(&RpcSvr);

// Stop()
// Null method, provided for convenience.
class StopMethod : public XmlRpcServerMethod {
public:
    StopMethod(XmlRpcServer* s) : XmlRpcServerMethod("Stop", s) {}

    void execute(XmlRpcValue& params, XmlRpcValue& result)
    {
        result = 0;
        std::cout << "got stop command" << std::endl;
    }
} stopMethod(&RpcSvr);

// status()
// Return a map of status values
class StatusMethod : public XmlRpcServerMethod {
public:
    StatusMethod(XmlRpcServer* s) : XmlRpcServerMethod("status", s) {}

    void execute(XmlRpcValue& params, XmlRpcValue& result)
    {
        XmlRpc::XmlRpcValue retval;
        retval["rate"] = WriteRate;
        // Return the map
        result = retval;
    }
} statusMethod(&RpcSvr);


// Time to quit?
bool Quit = false;

// Signal handler for SIGINT provides a clean shutdown on ^C
void
signalHandler(int signum) {
    Quit = 1;
}

int
main(int argc, char *argv[])
{
    const int DefaultRPCPort = 60003;
    if (argc != 1 && argc != 3)
    {
        std::cerr << "Usage: " << argv[0] << " [<data_dir> <rpc_port>]" << std::endl;
        exit(1);
    }

    std::cout << "productarchiver rev " << SVNREVISION << " from " <<
        SVNURL << std::endl;
    std::cout << "My PID is " << getpid() << std::endl;

    // Configuration

    // get the configuration
    QtConfig eaConfig("ProductArchiver", "ProductArchiver");

    // set up the configuration directory path: $KADIR/conf/
    char* e = getenv("KADIR");
    if (!e) {
        std::cerr << "Environment variable KADIR must be set." << std::endl;
        exit(1);
    }
    KaConfigDir = std::string(e) + "/conf/";

    std::string orbConfigFile =
        eaConfig.getString("DDS/ORBConfigFile", KaConfigDir + "ORBSvc.conf");
    std::string dcpsConfigFile =
        eaConfig.getString("DDS/DCPSConfigFile", KaConfigDir + "DDSClient.ini");
    std::string dcpsInfoRepo =
        eaConfig.getString("DDS/DCPSInfoRepo", "iiop://localhost:50000/DCPSInfoRepo");
    std::string productsTopic =
        eaConfig.getString("TopicProduct", "KAPROD");
    std::string dataDir =
        eaConfig.getString("DataDir", "/data_first");
    int rpcPort = 
        eaConfig.getInt("RpcPort", DefaultRPCPort);
    uint raysPerFile = 
        uint(eaConfig.getInt("RaysPerFile", 10000));
    std::cout << "Breaking files every " << raysPerFile << " rays" <<
        std::endl;

    // Override dataDir and rpcPort if we got command line args
    if (argc > 1) {
        std::cout << "Using data dir from command line: " << argv[1] << std::endl;
        dataDir = std::string(argv[1]);
        std::cout << "Using RPC port from command line: " << argv[2] << std::endl;
        rpcPort = atoi(argv[2]);
    }
    
    // Call signalHandler() if we get SIGINT (^C from the keyboard) or
    // SIGTERM (e.g., default for the 'kill' command)
    struct sigaction sa;
    sa.sa_handler = signalHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGINT, &sa, 0) != 0) {
        perror("Failed to change action for SIGINT");
        exit(1);
    }    
    if (sigaction(SIGTERM, &sa, 0) != 0) {
        perror("Failed to change action for SIGTERM");
        exit(1);
    }    
    
    // create the subscriber
    ArgvParams subParams(argv[0]);
    subParams["-ORBSvcConf"] = orbConfigFile;
    subParams["-DCPSConfigFile"] = dcpsConfigFile;
    subParams["-DCPSInfoRepo"] = dcpsInfoRepo;

    DDSSubscriber subscriber(subParams.argc(), subParams.argv());
    int subStatus = subscriber.status();
    if (subStatus) {
        std::cerr << "Error " << subStatus << " creating subscriber" << std::endl;
        exit(subStatus);
    }

    // Instantiate our netCDF CFRadial archiver
    ProductArchiver* archiver = new ProductArchiver(subscriber, 
            productsTopic, dataDir, raysPerFile, RadxFile::FILE_FORMAT_CFRADIAL);

    // Initialize our RPC server
    RpcSvr.bindAndListen(rpcPort);
    RpcSvr.enableIntrospection(true);

    while (! Quit) {
        // How much have we written?
        std::cerr << archiver->raysRead() << " rays read, " <<
            archiver->raysWritten() << " rays written, " << 
            std::setprecision(2) << 
            float(archiver->bytesWritten()) / (1024*1024) << " MB written" << 
            std::endl;
        
        // Listen for RPC commands for a bit
        RpcSvr.work(5.0);   // 5 seconds
    }
    RpcSvr.shutdown();
    
    // Delete our archiver, which will write out any unwritten data.
    delete(archiver);
    
    // XXX KLUGE! Just call exit(0) now to avoid a call to our DDSSubscriber's
    // destructor.  It causes a segfault, and there's no time at the moment to 
    // track it down...  (cb 4 Jun 2010)
    exit(0);
}
