/*
 * ka_xmitctl.cpp
 *
 *  Created on: Jan 6, 2011
 *      Author: burghart
 */

#include <logx/Logging.h>

#include <XmlRpc.h>
using namespace XmlRpc;

LOGGING("ka_xmitctl")

int
main(int argc, char *argv[]) {
    // Let logx get and strip out its arguments
    logx::ParseLogArgs(argc, argv);
    
    XmlRpcClient client("localhost", 8080);
    
    try {
        XmlRpcValue noArgs;
        XmlRpcValue statusDict;
        client.execute("getStatus", noArgs, statusDict);
        if (client.isFault()) {
            WLOG << "Server fault on getStatus";
        }

        // extract a couple of values from the dictionary
        bool hvpsOn = statusDict["hvps_on"];
        double hvpsCurrent = statusDict["hvps_current"];
        ILOG << "HVPS on: " << hvpsOn << ", HVPS current: " << hvpsCurrent;
    } catch (XmlRpcException const& e) {
        ELOG << "xmlrpc++ client threw exception: " << e.getMessage();
    }
}
