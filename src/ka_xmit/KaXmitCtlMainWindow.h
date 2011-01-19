/*
 *  Created on: Jan 14, 2011
 *      Author: burghart
 */
#ifndef KAXMITCTLMAINWINDOW_H_
#define KAXMITCTLMAINWINDOW_H_

#include <string>
#include <map>

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
    void _updateStatus();
private:
    static const XmlRpc::XmlRpcValue _NULL_XMLRPCVALUE;
    
    bool _faultSummary() { return(bool(_statusDict["fault_summary"])); }
    bool _hvpsRunup() { return(bool(_statusDict["hvps_runup"])); }
    bool _standby() { return(bool(_statusDict["standby"])); }
    bool _heaterWarmup() { return(bool(_statusDict["heater_warmup"])); }
    bool _cooldown() { return(bool(_statusDict["cooldown"])); }
    bool _unitOn() { return(bool(_statusDict["unit_on"])); }
    bool _magnetronCurrentFault() { return(bool(_statusDict["magnetron_current_fault"])); }
    bool _blowerFault() { return(bool(_statusDict["blower_fault"])); }
    bool _hvpsOn() { return(bool(_statusDict["hvpsOn"])); }
    bool _remoteEnabled() { return(bool(_statusDict["remote_enabled"])); }
    bool _safetyInterlock() { return(bool(_statusDict["safety_interlock"])); }
    bool _reversePowerFault() { return(bool(_statusDict["reverse_power_fault"])); }
    bool _pulseInputFault() { return(bool(_statusDict["pulse_input_fault"])); }
    bool _hvpsCurrentFault() { return(bool(_statusDict["hvps_current_fault"])); }
    bool _waveguidePressureFault() { return(bool(_statusDict["waveguide_pressure_fault"])); }
    bool _hvpsUnderVoltage() { return(bool(_statusDict["hvps_under_voltage"])); }
    bool _hvpsOverVoltage() { return(bool(_statusDict["hvps_over_voltage"])); }
    double _hvpsVoltage() { return(_statusDict["hvps_voltage"]); }
    double _magnetronCurrent() { return(_statusDict["magnetron_current"]); }
    double _hvpsCurrent() { return(_statusDict["hvps_current"]); }
    double _temperature() { return(_statusDict["temperature"]); }
    
    Ui::KaXmitCtlMainWindow _ui;
    QTimer _updateTimer;
    QPixmap _redLED;
    QPixmap _greenLED;
    QPixmap _greenLED_off;
    XmlRpc::XmlRpcClient _xmlrpcClient;
    // Last status read
    XmlRpc::XmlRpcValue _statusDict;
};
#endif /*KAXMITCTLMAINWINDOW_H_*/
