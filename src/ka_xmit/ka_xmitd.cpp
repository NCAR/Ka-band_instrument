/*
 * ka_xmitd.cpp
 *
 *  Created on: Jan 5, 2011
 *      Author: burghart
 */

#include <cassert>
#include <string>
#include <map>
#include <stdexcept>
#include <iostream>
#include <unistd.h>

#include <logx/Logging.h>

#include <xmlrpc-c/base.hpp>
#include <xmlrpc-c/registry.hpp>
#include <xmlrpc-c/server_abyss.hpp>

LOGGING("ka_xmitd")

// logical OR of all of the transmitter fault bits
bool FaultSummary = false;
// HVPS has been commanded ON
bool HvpsRunup = false;
// transmitter ready for HV on
bool Standby = false;
// heater 3-minute warmup is in progress
bool HeaterWarmup = false;
// 3-minute transmitter cooldown period is in progress
bool Cooldown = false;
// transmitter contactor energized and unit not in cooldown
bool UnitOn = false;
// magnetron over-current detected
bool MagnetronCurrentFault = false;
// blower speed detector indicates that the blower is stopped
bool BlowerFault = false;
// all HV power supplies have cleared the preset low limits and HV is on
bool HvpsOn = false;
// the unit is ready to accept remote commands
bool RemoteEnabled = false;
// power transmitter door/cover is open
bool SafetyInterlock = false;
// VSWR greater than 2:1 load mismatch
bool ReversePowerFault = false;
// transmitter input pulse has exceeded the duty cycle or PRF limits
bool PulseInputFault = false;
// HVPS protection has detected over-current
bool HvpsCurrentFault = false;
// waveguide pressure too low
bool WaveguidePressureFault = false;
// HVPS voltage too low
bool HvpsUnderVoltage = false;
// HVPS voltage too high
bool HvpsOverVoltage = false;
// HVPS voltage, kV
double HvpsVoltage = -99.99;
// magnetron average current, mA
double MagnetronCurrent = -99.99;
// HVPS current, mA
double HvpsCurrent = -99.99;

/// Xmlrpc++ method to get transmitter status from ka_xmitd. The method
/// returns a xmlrpc_c::value_struct, which can be cast into a dictionary
/// of type std::map<std::string, xmlrpc_c::value>. The dictionary
/// will contain:
/// <table border>
///   <tr>
///     <td><b>key</b></td>
///     <td><b>value type</b></td>
///     <td><b>value</b></td>
///   </tr>
///   <tr>
///     <td>fault_summary</td>
///     <td>bool</td>
///     <td>true iff any system fault bits are currently set</td>
///   </tr>
///   <tr>
///     <td>hvps_runup</td>
///     <td>bool</td>
///     <td>true if the HV voltage power supply has been commanded ON</td>
///   </tr>
///   <tr>
///     <td>standby</td>
///     <td>bool</td>
///     <td>transmitter ready for HV on</td>
///   </tr>
///   <tr>
///     <td>heater_warmup</td>
///     <td>bool</td>
///     <td>heater 3-minute warmup is in progress</td>
///   </tr>
///   <tr>
///     <td>cooldown</td>
///     <td>bool</td>
///     <td>3-minute transmitter cooldown is in progress</td>
///   </tr>
///   <tr>
///     <td>unit_on</td>
///     <td>bool</td>
///     <td>indicates transmitter contactor energized and unit not in cooldown</td>
///   </tr>
///   <tr>
///     <td>magnetron_current_fault</td>
///     <td>bool</td>
///     <td>magnetron over-current detected</td>
///   </tr>
///   <tr>
///     <td>blower_fault</td>
///     <td>bool</td>
///     <td>the blower speed detector indicates that blower is stopped</td>
///   </tr>
///   <tr>
///     <td>hvps_on</td>
///     <td>bool</td>
///     <td>high voltage is on</td>
///   </tr>
///   <tr>
///     <td>remote_enabled</td>
///     <td>bool</td>
///     <td>the unit is ready to accept remote commands</td>
///   </tr>
///   <tr>
///     <td>safety_interlock</td>
///     <td>bool</td>
///     <td>power transmitter door/cover is open</td>
///   </tr>
///   <tr>
///     <td>reverse_power_fault</td>
///     <td>bool</td>
///     <td>VSWR greater than 2:1 load mismatch</td>
///   </tr>
///   <tr>
///     <td>pulse_input_fault</td>
///     <td>bool</td>
///     <td>the transmitter input pulse has exceeded the duty cycle or PRF limits</td>
///   </tr>
///   <tr>
///     <td>hvps_current_fault</td>
///     <td>bool</td>
///     <td>HVPS protection has detected over-current</td>
///   </tr>
///   <tr>
///     <td>waveguide_pressure_fault</td>
///     <td>bool</td>
///     <td>waveguide pressure is detected low</td>
///   </tr>
///   <tr>
///     <td>hvps_under_voltage</td>
///     <td>bool</td>
///     <td>HVPS voltage is too low</td>
///   </tr>
///   <tr>
///     <td>hvps_over_voltage</td>
///     <td>bool</td>
///     <td>HVPS voltage is too high</td>
///   </tr>
///   <tr>
///     <td>hvps_voltage</td>
///     <td>double</td>
///     <td>HVPS potential, kV</td>
///   </tr>
///   <tr>
///     <td>magnetron_current</td>
///     <td>double</td>
///     <td>average magnetron current, mA</td>
///   </tr>
///   <tr>
///     <td>hvps_current</td>
///     <td>double</td>
///     <td>HVPS current, mA</td>
///   </tr>
/// </table>
/// Example client usage, where ka_xmitd is running on machine `xmitctl`:
/// @code
///     #include <xmlrpc-c/base.hpp>
///     #include <xmlrpc-c/client.hpp>
///     ...
///
///     // Get the transmitter status from ka_xmitd
///     xmlrpc_c::value_struct xmlVs;
///     client.call("http://xmitctl:8080/RPC2", "getStatus", "", &xmlVs);
///
///     // cast the xmlrpc_c::value_struct into a map<string,value> dictionary
///     std::map<std::string, xmlrpc_c::value> statusDict = xmlVs.cvalue();
///
///     // extract a couple of values from the dictionary
///     bool hvpsOn = statusDict["hvps_on"].cvalue();
///     double hvpsCurrent = statusDict["hvps_current"].cvalue();
/// @endcode
class getStatusMethod : public xmlrpc_c::method {
public:
    getStatusMethod() {
        /// Return a struct 
        this->_signature = "S:";
        // method's result and two arguments are integers
        this->_help = 
            "This method returns a dictionary containing the current transmitter status";
    }
    void
    execute(xmlrpc_c::paramList const& paramList,
            xmlrpc_c::value * const retvalP) {

        paramList.verifyEnd(0);

        std::map<std::string, xmlrpc_c::value> structData;
        structData["fault_summary"] = xmlrpc_c::value_boolean(FaultSummary);
        structData["hvps_runup"] = xmlrpc_c::value_boolean(HvpsRunup);
        structData["standby"] = xmlrpc_c::value_boolean(Standby);
        structData["heater_warmup"] = xmlrpc_c::value_boolean(HeaterWarmup);
        structData["cooldown"] = xmlrpc_c::value_boolean(Cooldown);
        structData["unit_on"] = xmlrpc_c::value_boolean(UnitOn);
        structData["magnetron_current_fault"] = xmlrpc_c::value_boolean(MagnetronCurrentFault);
        structData["blower_fault"] = xmlrpc_c::value_boolean(BlowerFault);
        structData["hvps_on"] = xmlrpc_c::value_boolean(HvpsOn);
        structData["remote_enabled"] = xmlrpc_c::value_boolean(RemoteEnabled);
        structData["safety_interlock"] = xmlrpc_c::value_boolean(SafetyInterlock);
        structData["reverse_power_fault"] = xmlrpc_c::value_boolean(ReversePowerFault);
        structData["pulse_input_fault"] = xmlrpc_c::value_boolean(PulseInputFault);
        structData["hvps_current_fault"] = xmlrpc_c::value_boolean(HvpsCurrentFault);
        structData["waveguide_pressure_fault"] = xmlrpc_c::value_boolean(WaveguidePressureFault);
        structData["hvps_under_voltage"] = xmlrpc_c::value_boolean(HvpsUnderVoltage);
        structData["hvps_over_voltage"] = xmlrpc_c::value_boolean(HvpsOverVoltage);
        structData["hvps_voltage"] = xmlrpc_c::value_double(HvpsVoltage);
        structData["magnetron_current"] = xmlrpc_c::value_double(MagnetronCurrent);
        structData["hvps_current"] = xmlrpc_c::value_double(HvpsCurrent);
        
        *retvalP = xmlrpc_c::value_struct(structData);
    }
};



int
main(int argc, char *argv[]) {
    // Let logx get and strip out its arguments
    logx::ParseLogArgs(argc, argv);
    
    try {
        xmlrpc_c::registry myRegistry;

        xmlrpc_c::methodPtr const getStatusMethodP(new getStatusMethod);

        myRegistry.addMethod("getStatus", getStatusMethodP);

        xmlrpc_c::serverAbyss myAbyssServer(
                xmlrpc_c::serverAbyss::constrOpt()
        .registryP(&myRegistry)
        .portNumber(8080));

        myAbyssServer.run();
        // xmlrpc_c::serverAbyss.run() never returns
        assert(false);
    } catch (std::exception const& e) {
        ELOG << __PRETTY_FUNCTION__ << "Something failed. " << e.what();
    }
    return 0;
} 
