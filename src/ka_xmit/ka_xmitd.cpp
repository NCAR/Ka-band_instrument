/*
 * ka_xmitd.cpp
 *
 *  Created on: Jan 5, 2011
 *      Author: burghart
 */

#include <string>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <deque>
#include <vector>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>

#include <logx/Logging.h>
#include <log4cpp/SyslogAppender.hh>
#include <RecentHistoryAppender.h>

#include <XmlRpc.h>

#include "KaXmitter.h"

LOGGING("ka_xmitd")

/// Our transmitter
KaXmitter *Xmitter = 0;
/// Current transmitter status
KaXmitStatus XmitStatus;

/// Our RPC server
using namespace XmlRpc;
XmlRpcServer RpcServer;

/// XML-RPC struct (dictionary) with last values received from the transmitter
/// (see documentation for GetStatusMethod below)
XmlRpcValue StatusDict;

/// What was the last time we saw the transmitter in "operate" mode?
time_t LastOperateTime = 0;

/// Are we allowing auto-reset of pulse input faults?
bool DoAutoFaultReset = true;

/// How many auto fault resets have we done?
int AutoResetCount = 0;

// Fault counters
int MagnetronCurrentFaultCount = 0;
int BlowerFaultCount = 0;
int SafetyInterlockFaultCount = 0;
int ReversePowerFaultCount = 0;
int PulseInputFaultCount = 0;
int HvpsCurrentFaultCount = 0;
int WaveguidePressureFaultFaultCount = 0;
int HvpsUnderVoltageCount = 0;
int HvpsOverVoltagetCount = 0;

// log4cpp Appender which keeps around the 50 most recent log messages. 
logx::RecentHistoryAppender RecentLogHistory("RecentHistoryAppender", 50);

/// Xmlrpc++ method to get transmitter status from ka_xmitd. The method
/// returns a XmlRpc::XmlRpcValue struct (dictionary) mapping std::string keys 
/// to XmlRpc::XmlRpcValue values. The dictionary will contain:
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
///   <tr>
///     <td>auto_pulse_fault_resets</td>
///     <td>int</td>
///     <td>count of pulse input faults which have been automatically reset since startup</td>
///   </tr>
///   <tr>
///     <td>magnetron_current_fault_count</td>
///     <td>int</td>
///     <td>count of magnetron current faults since startup</td>
///   </tr>
///   <tr>
///     <td>blower_fault_count</td>
///     <td>int</td>
///     <td>count of blower faults since startup</td>
///   </tr>
///   <tr>
///     <td>safety_interlock_count</td>
///     <td>int</td>
///     <td>count of safety interlock faults since startup</td>
///   </tr>
///   <tr>
///     <td>reverse_power_fault_count</td>
///     <td>int</td>
///     <td>count of reverse power faults since startup</td>
///   </tr>
///   <tr>
///     <td>pulse_input_fault_count</td>
///     <td>int</td>
///     <td>count of pulse input faults since startup</td>
///   </tr>
///   <tr>
///     <td>hvps_current_fault_count</td>
///     <td>int</td>
///     <td>count of HVPS current faults since startup</td>
///   </tr>
///   <tr>
///     <td>waveguide_pressure_fault_count</td>
///     <td>int</td>
///     <td>count of waveguide pressure faults since startup</td>
///   </tr>
///   <tr>
///     <td>hvps_under_voltage_count</td>
///     <td>int</td>
///     <td>count of HVPS under-voltage faults since startup</td>
///   </tr>
///   <tr>
///     <td>hvps_over_voltage_count</td>
///     <td>int</td>
///     <td>count of HVPS over-voltage faults since startup</td>
///   </tr>
/// </table>
/// Example client usage, where ka_xmitd is running on machine `xmitserver`:
/// @code
///     #include <XmlRpc.h>
///     ...
///
///     // Get the transmitter status from ka_xmitd on xmitserver.local.net on port 8080
///     XmlRpc::XmlRpcClient client("xmitserver.local.net", 8080);
///     const XmlRpc::XmlRpcValue nullParams;
///     XmlRpc::XmlRpcValue statusDict;
///     client.execute("getStatus", nullParams, statusDict);
///
///     // extract a couple of values from the dictionary
///     bool hvpsOn = bool(statusDict["hvps_on"]));
///     double hvpsCurrent = double(statusDict["hvps_current"]));
/// @endcode
class GetStatusMethod : public XmlRpcServerMethod {
public:
    GetStatusMethod(XmlRpcServer *s) : XmlRpcServerMethod("getStatus", s) {}
    void execute(XmlRpcValue & paramList, XmlRpcValue & retvalP) {
        retvalP = StatusDict;
    }
} getStatusMethod(&RpcServer);

/// Xmlrpc++ method to turn transmitter on. Note that this merely turns on
/// power to the transmitter unit, and does not enable the high voltage (i.e.,
/// actual transmission).
class PowerOnMethod : public XmlRpcServerMethod {
public:
    PowerOnMethod(XmlRpcServer *s) : XmlRpcServerMethod("powerOn", s) {}
    void execute(XmlRpcValue & paramList, XmlRpcValue & retvalP) {
        DLOG << "power ON command received";
        Xmitter->powerOn();
    }
} powerOnMethod(&RpcServer);

/// Xmlrpc++ method to turn transmitter off. It takes three minutes
/// of cooldown time before the transmitter turns off completely.
class PowerOffMethod : public XmlRpcServerMethod {
public:
    PowerOffMethod(XmlRpcServer *s) : XmlRpcServerMethod("powerOff", s) {}
    void execute(XmlRpcValue & paramList, XmlRpcValue & retvalP) {
        DLOG << "power OFF command received";
        Xmitter->powerOff();
    }
} powerOffMethod(&RpcServer);

/// Xmlrpc++ method to reset transmitter faults.
class FaultResetMethod : public XmlRpcServerMethod {
public:
    FaultResetMethod(XmlRpcServer *s) : XmlRpcServerMethod("faultReset", s) {}
    void execute(XmlRpcValue & paramList, XmlRpcValue & retvalP) {
        DLOG << "fault reset command received";
        Xmitter->faultReset();
        // Re-enable auto pulse fault resets when the user explicitly clears 
        // faults
        DoAutoFaultReset = true;
    }
} faultResetMethod(&RpcServer);

/// Xmlrpc++ method to enter "standby" state (warmed up, but high voltage off)
class StandbyMethod : public XmlRpcServerMethod {
public:
    StandbyMethod(XmlRpcServer *s) : XmlRpcServerMethod("standby", s) {}
    void execute(XmlRpcValue & paramList, XmlRpcValue & retvalP) {
        DLOG << "standby command received";
        Xmitter->standby();
    }
} standbyMethod(&RpcServer);

/// Xmlrpc++ method to enter "operate" state (high voltage on, ready to transmit)
class OperateMethod : public XmlRpcServerMethod {
public:
    OperateMethod(XmlRpcServer *s) : XmlRpcServerMethod("operate", s) {}
    void execute(XmlRpcValue & paramList, XmlRpcValue & retvalP) {
        DLOG << "operate command received";
        Xmitter->operate();
    }
} operateMethod(&RpcServer);

/// Xmlrpc++ method to get a string containing all available xmitd log 
/// messages at or later than the given index. If the index is zero, all 
/// available messages are returned. The method returns a XmlRpc::XmlRpcValue 
/// struct (dictionary) mapping std::string keys to XmlRpc::XmlRpcValue values.
/// The dictionary will contain:
/// <table border>
///   <tr>
///     <td><b>key</b></td>
///     <td><b>value type</b></td>
///     <td><b>value</b></td>
///   </tr>
///   <tr>
///     <td>logMessages</td>
///     <td>string</td>
///     <td>A string holding all of the requested log messages</td>
///   </tr>
///   <tr>
///     <td>nextIndex</td>
///     <td>int</td>
///     <td>The index of the next log message following the returned messages</td>
///   </tr>
class GetLogMessagesMethod : public XmlRpcServerMethod {
public:
    GetLogMessagesMethod(XmlRpcServer *s) : XmlRpcServerMethod("getLogMessages", s) {}
    void execute(XmlRpcValue & paramList, XmlRpcValue & retvalP) {
        DLOG << "getLogMessages command received";
        if (paramList[0].getType() != XmlRpcValue::TypeInt) {
            ELOG << "getLogMessages got non-integer param[0]!";
        }
        unsigned int startIndex = int(paramList[0]);
        // Note that the parameter to getMessages() is in/out, and on return
        // it will contain the next index to get.
        std::vector<std::string> msgs = RecentLogHistory.getMessages(startIndex);
        std::string appendedMsgs("");
        for (unsigned int i = 0; i < msgs.size(); i++) {
            appendedMsgs.append(msgs[i]);
        }
        // Trim a trailing newline
        if (appendedMsgs[appendedMsgs.size() - 1] == '\n')
            appendedMsgs.resize(appendedMsgs.size() - 1);
        
        XmlRpcValue dict;
        dict["logMessages"] = appendedMsgs;
        dict["nextIndex"] = int(startIndex);
        retvalP = dict;
    }
} getLogMessagesMethod(&RpcServer);

/// Get current status from the transmitter and update the XML-RPC status
/// dictionary.
void
updateStatus() {
    static KaXmitStatus PrevXmitStatus;
    PrevXmitStatus = XmitStatus;
    XmitStatus = Xmitter->getStatus();
    
    // Increment fault counters
    if (XmitStatus.magnetronCurrentFault && ! PrevXmitStatus.magnetronCurrentFault)
        MagnetronCurrentFaultCount++;
    if (XmitStatus.blowerFault && ! PrevXmitStatus.blowerFault)
        BlowerFaultCount++;
    if (XmitStatus.safetyInterlock && ! PrevXmitStatus.safetyInterlock)
        SafetyInterlockFaultCount++;
    if (XmitStatus.reversePowerFault && ! PrevXmitStatus.reversePowerFault)
        ReversePowerFaultCount++;
    if (XmitStatus.pulseInputFault && ! PrevXmitStatus.pulseInputFault)
        PulseInputFaultCount++;
    if (XmitStatus.hvpsCurrentFault && ! PrevXmitStatus.hvpsCurrentFault)
        HvpsCurrentFaultCount++;
    if (XmitStatus.waveguidePressureFault && ! PrevXmitStatus.waveguidePressureFault)
        WaveguidePressureFaultFaultCount++;
    if (XmitStatus.hvpsUnderVoltage && ! PrevXmitStatus.hvpsUnderVoltage)
        HvpsUnderVoltageCount++;
    if (XmitStatus.hvpsOverVoltage && ! PrevXmitStatus.hvpsOverVoltage)
        HvpsOverVoltagetCount++;
    
    // Unpack the status from the transmitter into our XML-RPC StatusDict
    StatusDict["serial_connected"] = XmlRpcValue(XmitStatus.serialConnected);
    StatusDict["fault_summary"] = XmlRpcValue(XmitStatus.faultSummary);
    StatusDict["hvps_runup"] = XmlRpcValue(XmitStatus.hvpsRunup);
    StatusDict["standby"] = XmlRpcValue(XmitStatus.standby);
    StatusDict["heater_warmup"] = XmlRpcValue(XmitStatus.heaterWarmup);
    StatusDict["cooldown"] = XmlRpcValue(XmitStatus.cooldown);
    StatusDict["unit_on"] = XmlRpcValue(XmitStatus.unitOn);
    StatusDict["magnetron_current_fault"] = 
            XmlRpcValue(XmitStatus.magnetronCurrentFault);
    StatusDict["blower_fault"] = XmlRpcValue(XmitStatus.blowerFault);
    StatusDict["hvps_on"] = XmlRpcValue(XmitStatus.hvpsOn);
    StatusDict["remote_enabled"] = XmlRpcValue(XmitStatus.remoteEnabled);
    StatusDict["safety_interlock"] = XmlRpcValue(XmitStatus.safetyInterlock);
    StatusDict["reverse_power_fault"] = XmlRpcValue(XmitStatus.reversePowerFault);
    StatusDict["pulse_input_fault"] = XmlRpcValue(XmitStatus.pulseInputFault);
    StatusDict["hvps_current_fault"] = XmlRpcValue(XmitStatus.hvpsCurrentFault);
    StatusDict["waveguide_pressure_fault"] = 
            XmlRpcValue(XmitStatus.waveguidePressureFault);
    StatusDict["hvps_under_voltage"] = XmlRpcValue(XmitStatus.hvpsUnderVoltage);
    StatusDict["hvps_over_voltage"] = XmlRpcValue(XmitStatus.hvpsOverVoltage);
    StatusDict["hvps_voltage"] = XmlRpcValue(XmitStatus.hvpsVoltage);
    StatusDict["magnetron_current"] = XmlRpcValue(XmitStatus.magnetronCurrent);
    StatusDict["hvps_current"] = XmlRpcValue(XmitStatus.hvpsCurrent);
    StatusDict["temperature"] = XmlRpcValue(XmitStatus.temperature);
    
    // And add our fault history counts
    StatusDict["auto_pulse_fault_resets"] = XmlRpcValue(AutoResetCount);
    StatusDict["magnetron_current_fault_count"] = 
            XmlRpcValue(MagnetronCurrentFaultCount);
    StatusDict["blower_fault_count"] = XmlRpcValue(BlowerFaultCount);
    StatusDict["safety_interlock_count"] = XmlRpcValue(SafetyInterlockFaultCount);
    StatusDict["reverse_power_fault_count"] = XmlRpcValue(ReversePowerFaultCount);
    StatusDict["pulse_input_fault_count"] = XmlRpcValue(PulseInputFaultCount);
    StatusDict["hvps_current_fault_count"] = XmlRpcValue(HvpsCurrentFaultCount);
    StatusDict["waveguide_pressure_fault_count"] = 
            XmlRpcValue(WaveguidePressureFaultFaultCount);
    StatusDict["hvps_under_voltage_count"] = XmlRpcValue(HvpsUnderVoltageCount);
    StatusDict["hvps_over_voltage_count"] = XmlRpcValue(HvpsOverVoltagetCount);
    
    // If we're operating (hvps_runup is true), update LastOperateTime to now
    if (XmitStatus.hvpsRunup)
        LastOperateTime = time(0);
}

/// Handle a pulse input fault
void
handlePulseInputFault() {
    /// List of pulse input fault times, and the max number of them we keep around
    static std::deque<time_t> PulseFaultTimes;
    static const unsigned int MAX_PF_ENTRIES = 10;

    // If auto-resets are disabled, just return now
    if (! DoAutoFaultReset)
        return;

    // Get current time
    time_t now = time(0);

    // Push the time of this fault on to our deque
    PulseFaultTimes.push_back(now);
    
    // Keep no more than the last MAX_ENTRIES fault times.
    if (PulseFaultTimes.size() == (MAX_PF_ENTRIES + 1))
        PulseFaultTimes.pop_front();
    
    // Issue a "faultReset" command unless the time span for the last 
    // MAX_ENTRIES faults is less than 100 seconds.
    if (PulseFaultTimes.size() < MAX_PF_ENTRIES || 
        ((PulseFaultTimes[MAX_PF_ENTRIES - 1] - PulseFaultTimes[0]) > 100)) {
        Xmitter->faultReset();
        AutoResetCount++;
        ILOG << "Pulse input fault auto reset";
    } else {
        std::ostringstream ss;
        WLOG << "Pulse input fault auto reset disabled after 10 faults in " << 
            (PulseFaultTimes[MAX_PF_ENTRIES - 1] - PulseFaultTimes[0]) << 
            " seconds!";
        // Disable auto fault resets. They will be re-enabled if the user
        // pushes the "Fault Reset" button.
        DoAutoFaultReset = false;
        return;
    }
    // The pulse input fault puts the radar into "standby" mode. 
    // If the radar was *very* recently in "operate" mode, wait until the fault
    // clears, then return to "operate" mode.
    if ((now - LastOperateTime) < 2) {
        int tries;
        const int MAXTRIES = 10;
        for (tries = 0; tries < MAXTRIES; tries++) {
            // Sleep a moment and get updated status
            usleep(100000);
            updateStatus();
            // If the fault was actually cleared, go back to "operate" mode.
            if (! XmitStatus.faultSummary) {
                ILOG << "Returning to 'operate' after pulse input fault reset.";
                Xmitter->operate();
                break;
            }
        }
        if (tries == MAXTRIES) {
            WLOG << "Too many tries waiting for fault to clear!";
            // Disable auto fault resets. They will be re-enabled if the user
            // pushes the "Fault Reset" button.
            DoAutoFaultReset = false;
        }
    }
    return;     
}



int
main(int argc, char *argv[]) {
    // Let logx get and strip out its arguments
    logx::ParseLogArgs(argc, argv);
    
    // Append our log to local syslog
    log4cpp::SyslogAppender syslogAppender("SyslogAppender", "ka_xmitd", LOG_DAEMON);
    log4cpp::Category::getRoot().addAppender(syslogAppender);
    
    // Append our log to the recent history
    log4cpp::Category::getRoot().addAppender(RecentLogHistory);

    // Check the remaining args
    if (argc != 3 && argc != 4) {
        std::cerr << "Usage: " << argv[0] << 
            " <xmitter_ttydev> <server_port> [f] [<logx_arg> ...]" << std::endl;
        std::cerr << std::endl;
        std::cerr << "Use ""SimulatedKaXmitter"" as <xmitter_ttydev> to " << 
                "simulate a transmitter" << std::endl;
        std::cerr << "If 'f' is given after the port number, the program will" <<
            std::endl;
        std::cerr << "run in foreground, otherwise it will run as a daemon in" <<
            std::endl;
        std::cerr << "background." << std::endl;
        exit(1);
    }
    
    // If we didn't get a third arg, run in background
    if (argc == 3) {
        pid_t pid = fork();
        if (pid < 0) {
            ELOG << "Error forking: " << strerror(errno);
            exit(1);
        } else if (pid > 0) {
            exit(0);    // parent process exits now, leaving the child in background
        }
    }
    
    time_t now = time(0);
    char timestring[40];
    strftime(timestring, sizeof(timestring) - 1, "%F %T", gmtime(&now));
    ILOG << "ka_xmitd (" << getpid() << ") started " << timestring;

    // start with all-zero status
    memset(&XmitStatus, 0, sizeof(XmitStatus));
    
    // Instantiate our transmitter, communicating over the given serial port
    Xmitter = new KaXmitter(argv[1]);
    
    // Initialize our RPC server
    RpcServer.bindAndListen(atoi(argv[2]));
    RpcServer.enableIntrospection(true);
    
    /*
     * Listen for XML-RPC commands for a while. Repeat.
     */
    while (true) {
        updateStatus();
        if (XmitStatus.pulseInputFault)
            handlePulseInputFault();
        // Note that work() mostly goes for 2x the given time, but sometimes
        // goes for 1x the given time. Who knows why?
        RpcServer.work(0.2);
    }
    
    delete(Xmitter);
    return 0;
} 
