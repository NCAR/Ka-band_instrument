/*
 * ka_xmitctl.cpp
 *
 *  Created on: Jan 6, 2011
 *      Author: burghart
 */

#include <logx/Logging.h>

#include <xmlrpc-c/base.hpp>
#include <xmlrpc-c/client_simple.hpp>

LOGGING("ka_xmitctl")

int
main(int argc, char *argv[]) {
    // Let logx get and strip out its arguments
    logx::ParseLogArgs(argc, argv);
    
    try {
        xmlrpc_c::clientSimple client;
        // Get the transmitter status from ka_xmitd
        xmlrpc_c::value result;
        client.call("http://localhost:8080/RPC2", "getStatus", "", &result);

        // cast the xmlrpc_c::value_struct into a map<string,value> dictionary
        std::map<std::string, xmlrpc_c::value> statusDict = xmlrpc_c::value_struct(result);

        // extract a couple of values from the dictionary
        bool hvpsOn = xmlrpc_c::value_boolean(statusDict["hvps_on"]);
        double hvpsCurrent = xmlrpc_c::value_double(statusDict["hvps_current"]);
        ILOG << "HVPS on: " << hvpsOn << ", HVPS current: " << hvpsCurrent;
    } catch (std::exception const& e) {
        ELOG << "xmlrpc++ client threw error: " << e.what();
    }
}
