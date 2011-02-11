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
    // Send a "faultReset" command to the transmitter
    void _faultReset();
    // Disable the UI
    void _disableUi();
    // Send an "operate" command to the transmitter
    void _operate();
    // Send a "status" command, leaving the results in _statusDict.
    void _getStatus();
    // Disable the UI when no connection exists to the ka_xmitd.
    void _noDaemon();
    // Disable the UI if the daemon is not talking to the transmitter
    void _noXmitter();
    // Special handling for pulse input faults
    void _handlePulseInputFault();
    // Log a message
    void _logMessage(std::string message);
    
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
    double _hvpsVoltage() { return(_statusDouble("hvps_voltage")); }
    double _magnetronCurrent() { return(_statusDouble("magnetron_current")); }
    double _hvpsCurrent() { return(_statusDouble("hvps_current")); }
    double _temperature() { return(_statusDouble("temperature")); }
    
    bool _statusBool(std::string key);
    double _statusDouble(std::string key);
    
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
    // What was the last time we saw the transmitter in "operate" mode?
    time_t _lastOperateTime;
    // Pulse input fault times
    std::deque<time_t> _pulseInputFaultTimes;
    // Are we allowing auto-reset of pulse input faults?
    bool _doAutoFaultReset;
    // How many auto fault resets have we done?
    int _autoResetCount;
};
#endif /*KAXMITCTLMAINWINDOW_H_*/
