#include <ArgvParams.h>
#include <QtConfig.h>
#include <XmlRpc.h>
#include <DDSSubscriber.h>
#include <boost/date_time/posix_time/posix_time.hpp>

#include "TsArchiver.h"
#include "svnInfo.h"


// Current write rate, in rays/second
float WriteRate = 0.0;

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


int
main(int argc, char *argv[])
{
    const int DefaultRPCPort = 60003;
    if (argc != 1 && argc != 3)
    {
        std::cerr << "Usage: " << argv[0] << " [<data_dir> <rpc_port>]" << std::endl;
        exit(1);
    }

    std::cout << "tsarchiver rev " << SVNREVISION << " from " <<
        SVNURL << std::endl;
    std::cout << "My PID is " << getpid() << std::endl;

    // Configuration

    // get the configuration
    QtConfig eaConfig("TsArchiver", "TsArchiver");

    // set up the configuration directory path: $KADIR/conf/
    char* e = getenv("KADIR");
    if (!e) {
        std::cerr << "Environment variable KADIR must be set." << std::endl;
        exit(1);
    }
    std::string kaConfigDir = std::string(e) + "/conf/";

    std::string orbConfigFile =
        eaConfig.getString("DDS/ORBConfigFile", kaConfigDir + "ORBSvc.conf");
    std::string dcpsConfigFile =
        eaConfig.getString("DDS/DCPSConfigFile", kaConfigDir + "DDSClient.ini");
    std::string dcpsInfoRepo =
        eaConfig.getString("DDS/DCPSInfoRepo", "iiop://localhost:50000/DCPSInfoRepo");
    std::string productsTopic =
        eaConfig.getString("TopicTS", "KATS");
    std::string dataDir =
        eaConfig.getString("DataDir", "/data_first");
    int rpcPort =
        eaConfig.getInt("RpcPort", DefaultRPCPort);

    // Override dataDir and rpcPort if we got command line args
    if (argc > 1) {
        std::cout << "Using data dir from command line: " << argv[1] << std::endl;
        dataDir = std::string(argv[1]);
        std::cout << "Using RPC port from command line: " << argv[2] << std::endl;
        rpcPort = atoi(argv[2]);
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

    // Instantiate the singleton archiver
    TsArchiver* theArchiver =
        TsArchiver::TheArchiver(subscriber, productsTopic, dataDir);

    // Initialize our RPC server
    RpcSvr.bindAndListen(rpcPort);
    RpcSvr.enableIntrospection(true);

    int prevBytesWritten = theArchiver->bytesWritten();
    boost::posix_time::ptime lastCheckTime =
        boost::posix_time::microsec_clock::universal_time();

    while (1) {
        // Listen for RPC commands for a bit
        RpcSvr.work(5.0);   // 5 seconds

        // Calculate write rate
        boost::posix_time::ptime now =
            boost::posix_time::microsec_clock::universal_time();
        int bytesWritten = theArchiver->bytesWritten();
        int byteDiff = bytesWritten - prevBytesWritten;
        boost::posix_time::time_duration timeDiff = now - lastCheckTime;
        WriteRate = byteDiff / (1.0e-6 * timeDiff.total_microseconds()); // B/s
        WriteRate /= (1024 * 1024);  // MB/s
        std::cerr << "Write rate: " << WriteRate << " MB/s, DDS drops: " << 
        	theArchiver->ddsDrops() << std::endl;;

        // Save current numbers for next go-round
        prevBytesWritten = bytesWritten;
        lastCheckTime = now;
    }

}
