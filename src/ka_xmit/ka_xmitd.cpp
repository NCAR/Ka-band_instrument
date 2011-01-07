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
        // Construct an XML-RPC <struct> (more accurately a dictionary)
        // containing all of the transmitter status values.
        XmlRpcValue statusDict;
        statusDict["fault_summary"] = XmlRpcValue(xmitter.getFaultSummary());
        statusDict["hvps_runup"] = XmlRpcValue(xmitter.getHvpsRunup());
        statusDict["standby"] = XmlRpcValue(xmitter.getStandby());
        statusDict["heater_warmup"] = XmlRpcValue(xmitter.getHeaterWarmup());
        statusDict["cooldown"] = XmlRpcValue(xmitter.getCooldown());
        statusDict["unit_on"] = XmlRpcValue(xmitter.getUnitOn());
        statusDict["magnetron_current_fault"] = XmlRpcValue(xmitter.getMagnetronCurrentFault());
        statusDict["blower_fault"] = XmlRpcValue(xmitter.getBlowerFault());
        statusDict["hvps_on"] = XmlRpcValue(xmitter.getHvpsOn());
        statusDict["remote_enabled"] = XmlRpcValue(xmitter.getRemoteEnabled());
        statusDict["safety_interlock"] = XmlRpcValue(xmitter.getSafetyInterlock());
        statusDict["reverse_power_fault"] = XmlRpcValue(xmitter.getReversePowerFault());
        statusDict["pulse_input_fault"] = XmlRpcValue(xmitter.getPulseInputFault());
        statusDict["hvps_current_fault"] = XmlRpcValue(xmitter.getHvpsCurrentFault());
        statusDict["waveguide_pressure_fault"] = XmlRpcValue(xmitter.getWaveguidePressureFault());
        statusDict["hvps_under_voltage"] = XmlRpcValue(xmitter.getHvpsUnderVoltage());
        statusDict["hvps_over_voltage"] = XmlRpcValue(xmitter.getHvpsOverVoltage());
        statusDict["hvps_voltage"] = XmlRpcValue(xmitter.getHvpsVoltage());
        statusDict["magnetron_current"] = XmlRpcValue(xmitter.getMagnetronCurrent());
        statusDict["hvps_current"] = XmlRpcValue(xmitter.getHvpsCurrent());
        
        retvalP = statusDict;
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
