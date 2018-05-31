/*
 * XmitdStatus.cpp
 *
 *  Created on: May 10, 2018
 *      Author: Chris Burghart <burghart@ucar.edu>
 */

#include "XmitdStatus.h"
#include <logx/Logging.h>

LOGGING("XmitdStatus")

XmitdStatus::XmitdStatus() {
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

    _magnetronCurrentFaultTime = -1;
    _blowerFaultTime = -1;
    _safetyInterlockTime = -1;
    _reversePowerFaultTime = -1;
    _pulseInputFaultTime = -1;
    _hvpsCurrentFaultTime = -1;
    _waveguidePressureFaultTime = -1;
    _hvpsUnderVoltageTime = -1;
    _hvpsOverVoltageTime = -1;

    _hvpsVoltage = -9999.9;
    _magnetronCurrent = -9999.9;
    _hvpsCurrent = -9999.9;
    _temperature = -9999.9;

}

XmitdStatus::XmitdStatus(XmlRpc::XmlRpcValue & statusDict) {
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

    _magnetronCurrentFaultTime = _StatusInt(statusDict, "magnetron_current_fault_time");
    _blowerFaultTime = _StatusInt(statusDict, "blower_fault_time");
    _safetyInterlockTime = _StatusInt(statusDict, "safety_interlock_time");
    _reversePowerFaultTime = _StatusInt(statusDict, "reverse_power_fault_time");
    _pulseInputFaultTime = _StatusInt(statusDict, "pulse_input_fault_time");
    _hvpsCurrentFaultTime = _StatusInt(statusDict, "hvps_current_fault_time");
    _waveguidePressureFaultTime = _StatusInt(statusDict, "waveguide_pressure_fault_time");
    _hvpsUnderVoltageTime = _StatusInt(statusDict, "hvps_under_voltage_time");
    _hvpsOverVoltageTime = _StatusInt(statusDict, "hvps_over_voltage_time");

    _hvpsVoltage = _StatusDouble(statusDict, "hvps_voltage");
    _magnetronCurrent = _StatusDouble(statusDict, "magnetron_current");
    _hvpsCurrent = _StatusDouble(statusDict, "hvps_current");
    _temperature = _StatusDouble(statusDict, "temperature");

}

XmitdStatus::~XmitdStatus() {
}

bool
XmitdStatus::_StatusBool(XmlRpc::XmlRpcValue statusDict, std::string key) {
    if (! statusDict.hasMember(key)) {
        ELOG << "Status dictionary does not contain requested key '" << key <<
            "'!";
        abort();
    } else {
        return(bool(statusDict[key]));
    }
}

int
XmitdStatus::_StatusInt(XmlRpc::XmlRpcValue statusDict, std::string key) {
    if (! statusDict.hasMember(key)) {
        ELOG << "Status dictionary does not contain requested key '" << key <<
            "'!";
        abort();
    } else {
        return(int(statusDict[key]));
    }
}

double
XmitdStatus::_StatusDouble(XmlRpc::XmlRpcValue statusDict, std::string key) {
    if (! statusDict.hasMember(key)) {
        ELOG << "Status dictionary does not contain requested key '" << key <<
            "'!";
        abort();
    } else {
        return(double(statusDict[key]));
    }
}
