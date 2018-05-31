/*
 * XmitClient.cpp
 *
 *  Created on: Mar 11, 2011
 *      Author: burghart
 */

#include "XmitClient.h"

#include <cstdlib>
#include <logx/Logging.h>

LOGGING("XmitClient")

XmitClient::XmitClient(std::string xmitdHost, int xmitdPort) :
    XmlRpc::XmlRpcClient(xmitdHost.c_str(), xmitdPort),
    _xmitdHost(xmitdHost),
    _xmitdPort(xmitdPort) {
}

XmitClient::~XmitClient() {
}

bool
XmitClient::_executeXmlRpcCommand(const std::string cmd, 
    const XmlRpc::XmlRpcValue & params, XmlRpc::XmlRpcValue & result) {
    DLOG << "Executing '" << cmd << "()' command";
    if (! execute(cmd.c_str(), params, result)) {
        DLOG << "Error executing " << cmd << "() call to ka_xmitd";
        return(false);
    }
    if (isFault()) {
        ELOG << "XML-RPC fault on " << cmd << "() call";
        abort();
    }
    return true;  
}

void
XmitClient::powerOn() {
    XmlRpc::XmlRpcValue null;
    XmlRpc::XmlRpcValue result;
    _executeXmlRpcCommand("powerOn", null, result);
}

void
XmitClient::powerOff() {
    XmlRpc::XmlRpcValue null;
    XmlRpc::XmlRpcValue result;
    _executeXmlRpcCommand("powerOff", null, result);
}

void
XmitClient::faultReset() {
    XmlRpc::XmlRpcValue null;
    XmlRpc::XmlRpcValue result;
    _executeXmlRpcCommand("faultReset", null, result);
}

void
XmitClient::standby() {
    XmlRpc::XmlRpcValue null;
    XmlRpc::XmlRpcValue result;
    _executeXmlRpcCommand("standby", null, result);
}

void
XmitClient::operate() {
    XmlRpc::XmlRpcValue null;
    XmlRpc::XmlRpcValue result;
    _executeXmlRpcCommand("operate", null, result);
}

bool
XmitClient::getStatus(XmitdStatus & status) {
    XmlRpc::XmlRpcValue null;
    XmlRpc::XmlRpcValue statusDict;
    if (! _executeXmlRpcCommand("getStatus", null, statusDict)) {
        WLOG << __PRETTY_FUNCTION__ << ": getStatus failed!";
        return(false);
    }
    status = XmitdStatus(statusDict);
    return(true);
}

void
XmitClient::getLogMessages(unsigned int firstIndex, std::string & msgs, 
        unsigned int  & nextLogIndex) {
    XmlRpc::XmlRpcValue startIndex = int(firstIndex);
    XmlRpc::XmlRpcValue resultDict;
    if (! _executeXmlRpcCommand("getLogMessages", startIndex, resultDict)) {
        WLOG << __PRETTY_FUNCTION__ << ": getLogMessages failed!";
        return;
    }
    msgs.append(std::string(resultDict["logMessages"]));
    nextLogIndex = (unsigned int)(int(resultDict["nextIndex"]));
}
