/*
 * ka_xmitd.cpp
 *
 *  Created on: Jan 5, 2011
 *      Author: burghart
 */

#include <string>
#include <cassert>
#include <unistd.h>

#include <logx/Logging.h>

#include <XmlRpc.h>

#include "KaXmitter.h"

LOGGING("ka_xmitd")

KaXmitter xmitter("/dev/ttyS0");

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

// Our RPC server
using namespace XmlRpc;
XmlRpcServer RpcServer;

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
class GetStatusMethod : public XmlRpcServerMethod {
public:
    GetStatusMethod(XmlRpcServer *s) : XmlRpcServerMethod("getStatus", s) {}
    void execute(XmlRpcValue & paramList, XmlRpcValue & retvalP) {

        assert(paramList.size() == 0);
        ILOG << "in getStatus()";
        XmlRpcValue structData;
        structData["fault_summary"] = XmlRpcValue(xmitter.getFaultSummary());
        structData["hvps_runup"] = XmlRpcValue(xmitter.getHvpsRunup());
        structData["standby"] = XmlRpcValue(xmitter.getStandby());
        structData["heater_warmup"] = XmlRpcValue(xmitter.getHeaterWarmup());
        structData["cooldown"] = XmlRpcValue(xmitter.getCooldown());
        structData["unit_on"] = XmlRpcValue(xmitter.getUnitOn());
        structData["magnetron_current_fault"] = XmlRpcValue(xmitter.getMagnetronCurrentFault());
        structData["blower_fault"] = XmlRpcValue(xmitter.getBlowerFault());
        structData["hvps_on"] = XmlRpcValue(xmitter.getHvpsOn());
        structData["remote_enabled"] = XmlRpcValue(xmitter.getRemoteEnabled());
        structData["safety_interlock"] = XmlRpcValue(xmitter.getSafetyInterlock());
        structData["reverse_power_fault"] = XmlRpcValue(xmitter.getReversePowerFault());
        structData["pulse_input_fault"] = XmlRpcValue(xmitter.getPulseInputFault());
        structData["hvps_current_fault"] = XmlRpcValue(xmitter.getHvpsCurrentFault());
        structData["waveguide_pressure_fault"] = XmlRpcValue(xmitter.getWaveguidePressureFault());
        structData["hvps_under_voltage"] = XmlRpcValue(xmitter.getHvpsUnderVoltage());
        structData["hvps_over_voltage"] = XmlRpcValue(xmitter.getHvpsOverVoltage());
        structData["hvps_voltage"] = XmlRpcValue(xmitter.getHvpsVoltage());
        structData["magnetron_current"] = XmlRpcValue(xmitter.getMagnetronCurrent());
        structData["hvps_current"] = XmlRpcValue(xmitter.getHvpsCurrent());
        
        retvalP = structData;
    }
} getStatusMethod(&RpcServer);



int
main(int argc, char *argv[]) {
    // Let logx get and strip out its arguments
    logx::ParseLogArgs(argc, argv);

    // Initialize our RPC server
    RpcServer.bindAndListen(8080);
    RpcServer.enableIntrospection(true);

    while (true) {
        RpcServer.work(1.0);
    }
    return 0;
} 
