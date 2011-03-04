/*
 *  Created on: Jan 14, 2011
 *      Author: burghart
 */
#ifndef KAXMITCTLMAINWINDOW_H_
#define KAXMITCTLMAINWINDOW_H_

#include <string>
#include <map>
#include <deque>
#include <ctime>

#include <QMainWindow>
#include <QPixmap>
#include <QTimer>

#include <XmlRpc.h>

#include "ui_KaXmitCtlMainWindow.h"

class KaXmitCtlMainWindow : public QMainWindow {
    Q_OBJECT
public:
    KaXmitCtlMainWindow(std::string xmitterHost, int xmitterPort);
    ~KaXmitCtlMainWindow();
private slots:
    void on_powerButton_clicked();
    void on_faultResetButton_clicked();
    void on_standbyButton_clicked();
    void on_operateButton_clicked();
    void _update();
private:
    static const XmlRpc::XmlRpcValue _NULL_XMLRPCVALUE;
    // Execute an XML-RPC command
    bool _executeXmlRpcCommand(const std::string cmd, 
        const XmlRpc::XmlRpcValue & params, XmlRpc::XmlRpcValue & result);
    // Disable the UI
    void _disableUi();
    // Enable the UI
    void _enableUi();
    // Send an "operate" command to the transmitter
    void _operate();
    // Send a "status" command, leaving the results in _statusDict.
    void _getStatus();
    // Append latest messages from ka_xmitd to our logging area
    void _appendXmitdLogMsgs();
    // Disable the UI when no connection exists to the ka_xmitd.
    void _noDaemon();
    // Disable the UI if the daemon is not talking to the transmitter
    void _noXmitter();
    // Log a message
    void _logMessage(std::string message);
    /**
     *  Return "-" if the count is zero, otherwise a text representation of 
     *  the count.
     *  @param count the count to be represented
     *  @return "-" if the count is zero, otherwise a text representation of 
     *      the count.
     */
    QString _countLabel(int count) {
        if (count == 0)
            return QString("-");
        
        QString txt;
        txt.setNum(count);
        return txt;
    }
    
    bool _statusBool(std::string key);
    int _statusInt(std::string key);
    double _statusDouble(std::string key);
    
    bool _serialConnected() { return(_statusBool("serial_connected")); }
    bool _faultSummary() { return(_statusBool("fault_summary")); }
    bool _hvpsRunup() { return(_statusBool("hvps_runup")); }
    bool _standby() { return(_statusBool("standby")); }
    bool _heaterWarmup() { return(_statusBool("heater_warmup")); }
    bool _cooldown() { return(_statusBool("cooldown")); }
    bool _unitOn() { return(_statusBool("unit_on")); }
    bool _magnetronCurrentFault() { return(_statusBool("magnetron_current_fault")); }
    bool _blowerFault() { return(_statusBool("blower_fault")); }
    bool _hvpsOn() { return(_statusBool("hvps_on")); }
    bool _remoteEnabled() { return(_statusBool("remote_enabled")); }
    bool _safetyInterlock() { return(_statusBool("safety_interlock")); }
    bool _reversePowerFault() { return(_statusBool("reverse_power_fault")); }
    bool _pulseInputFault() { return(_statusBool("pulse_input_fault")); }
    bool _hvpsCurrentFault() { return(_statusBool("hvps_current_fault")); }
    bool _waveguidePressureFault() { return(_statusBool("waveguide_pressure_fault")); }
    bool _hvpsUnderVoltage() { return(_statusBool("hvps_under_voltage")); }
    bool _hvpsOverVoltage() { return(_statusBool("hvps_over_voltage")); }
    
    int _magnetronCurrentFaultCount() { return(_statusInt("magnetron_current_fault_count")); }
    int _blowerFaultCount() { return(_statusInt("blower_fault_count")); }
    int _safetyInterlockCount() { return(_statusInt("safety_interlock_count")); }
    int _reversePowerFaultCount() { return(_statusInt("reverse_power_fault_count")); }
    int _pulseInputFaultCount() { return(_statusInt("pulse_input_fault_count")); }
    int _hvpsCurrentFaultCount() { return(_statusInt("hvps_current_fault_count")); }
    int _waveguidePressureFaultCount() { return(_statusInt("waveguide_pressure_fault_count")); }
    int _hvpsUnderVoltageCount() { return(_statusInt("hvps_under_voltage_count")); }
    int _hvpsOverVoltageCount() { return(_statusInt("hvps_over_voltage_count")); }
    int _autoPulseFaultResets() { return(_statusInt("auto_pulse_fault_resets")); }
    
    double _hvpsVoltage() { return(_statusDouble("hvps_voltage")); }
    double _magnetronCurrent() { return(_statusDouble("magnetron_current")); }
    double _hvpsCurrent() { return(_statusDouble("hvps_current")); }
    double _temperature() { return(_statusDouble("temperature")); }
    
    Ui::KaXmitCtlMainWindow _ui;
    std::string _xmitterHost;
    int _xmitterPort;
    QTimer _updateTimer;
    QPixmap _redLED;
    QPixmap _greenLED;
    QPixmap _greenLED_off;
    XmlRpc::XmlRpcClient _xmlrpcClient;
    // Last status read
    XmlRpc::XmlRpcValue _statusDict;
    
    // next log index to get from ka_xmitd
    unsigned int _nextLogIndex;
};
#endif /*KAXMITCTLMAINWINDOW_H_*/
