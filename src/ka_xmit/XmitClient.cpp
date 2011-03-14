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
XmitClient::getStatus(XmitStatus & status) {
    XmlRpc::XmlRpcValue null;
    XmlRpc::XmlRpcValue statusDict;
    if (! _executeXmlRpcCommand("getStatus", null, statusDict)) {
        WLOG << __PRETTY_FUNCTION__ << ": getStatus failed!";
        return(false);
    }
    status = XmitStatus(statusDict);
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

// Default constructor; fill with bad values.
XmitClient::XmitStatus::XmitStatus() {
    _serialConnected = false;
    _faultSummary = false;
    _hvpsRunup = false;
    _standby = false;
    _heaterWarmup = false;
    _cooldown = false;
    _unitOn = false;
    _magnetronCurrentFault = false;
    _blowerFault = false;
    _hvpsOn = false;
    _remoteEnabled = false;
    _safetyInterlock = false;
    _reversePowerFault = false;
    _pulseInputFault = false;
    _hvpsCurrentFault = false;
    _waveguidePressureFault = false;
    _hvpsUnderVoltage = false;
    _hvpsOverVoltage = false;
    
    _magnetronCurrentFaultCount = -1;
    _blowerFaultCount = -1;
    _safetyInterlockCount = -1;
    _reversePowerFaultCount = -1;
    _pulseInputFaultCount = -1;
    _hvpsCurrentFaultCount = -1;
    _waveguidePressureFaultCount = -1;
    _hvpsUnderVoltageCount = -1;
    _hvpsOverVoltageCount = -1;
    _autoPulseFaultResets = -1;
    
    _hvpsVoltage = -9999.9;
    _magnetronCurrent = -9999.9;
    _hvpsCurrent = -9999.9;
    _temperature = -9999.9;

}

XmitClient::XmitStatus::XmitStatus(XmlRpc::XmlRpcValue & statusDict) {
    // Unpack all of the status values from the XmlRpc::XmlRpcValue dictionary
    // into local member variables
    _serialConnected = _StatusBool(statusDict, "serial_connected");
    _faultSummary = _StatusBool(statusDict, "fault_summary");
    _hvpsRunup = _StatusBool(statusDict, "hvps_runup");
    _standby = _StatusBool(statusDict, "standby");
    _heaterWarmup = _StatusBool(statusDict, "heater_warmup");
    _cooldown = _StatusBool(statusDict, "cooldown");
    _unitOn = _StatusBool(statusDict, "unit_on");
    _magnetronCurrentFault = _StatusBool(statusDict, "magnetron_current_fault");
    _blowerFault = _StatusBool(statusDict, "blower_fault");
    _hvpsOn = _StatusBool(statusDict, "hvps_on");
    _remoteEnabled = _StatusBool(statusDict, "remote_enabled");
    _safetyInterlock = _StatusBool(statusDict, "safety_interlock");
    _reversePowerFault = _StatusBool(statusDict, "reverse_power_fault");
    _pulseInputFault = _StatusBool(statusDict, "pulse_input_fault");
    _hvpsCurrentFault = _StatusBool(statusDict, "hvps_current_fault");
    _waveguidePressureFault = _StatusBool(statusDict, "waveguide_pressure_fault");
    _hvpsUnderVoltage = _StatusBool(statusDict, "hvps_under_voltage");
    _hvpsOverVoltage = _StatusBool(statusDict, "hvps_over_voltage");
    
    _magnetronCurrentFaultCount = _StatusInt(statusDict, "magnetron_current_fault_count");
    _blowerFaultCount = _StatusInt(statusDict, "blower_fault_count");
    _safetyInterlockCount = _StatusInt(statusDict, "safety_interlock_count");
    _reversePowerFaultCount = _StatusInt(statusDict, "reverse_power_fault_count");
    _pulseInputFaultCount = _StatusInt(statusDict, "pulse_input_fault_count");
    _hvpsCurrentFaultCount = _StatusInt(statusDict, "hvps_current_fault_count");
    _waveguidePressureFaultCount = _StatusInt(statusDict, "waveguide_pressure_fault_count");
    _hvpsUnderVoltageCount = _StatusInt(statusDict, "hvps_under_voltage_count");
    _hvpsOverVoltageCount = _StatusInt(statusDict, "hvps_over_voltage_count");
    _autoPulseFaultResets = _StatusInt(statusDict, "auto_pulse_fault_resets");
    
    _hvpsVoltage = _StatusDouble(statusDict, "hvps_voltage");
    _magnetronCurrent = _StatusDouble(statusDict, "magnetron_current");
    _hvpsCurrent = _StatusDouble(statusDict, "hvps_current");
    _temperature = _StatusDouble(statusDict, "temperature");
    
}

XmitClient::XmitStatus::~XmitStatus() {
}

bool
XmitClient::XmitStatus::_StatusBool(XmlRpc::XmlRpcValue statusDict, std::string key) {
    if (! statusDict.hasMember(key)) {
        ELOG << "Status dictionary does not contain requested key '" << key <<
            "'!";
        abort();
    } else {
        return(bool(statusDict[key]));
    }
}

int
XmitClient::XmitStatus::_StatusInt(XmlRpc::XmlRpcValue statusDict, std::string key) {
    if (! statusDict.hasMember(key)) {
        ELOG << "Status dictionary does not contain requested key '" << key <<
            "'!";
        abort();
    } else {
        return(int(statusDict[key]));
    }
}

double
XmitClient::XmitStatus::_StatusDouble(XmlRpc::XmlRpcValue statusDict, std::string key) {
    if (! statusDict.hasMember(key)) {
        ELOG << "Status dictionary does not contain requested key '" << key <<
            "'!";
        abort();
    } else {
        return(double(statusDict[key]));
    }
}
