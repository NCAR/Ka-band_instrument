/*
 * KaXmitCtlMainWindow.cpp
 *
 *  Created on: Jan 14, 2011
 *      Author: burghart
 */
#include <logx/Logging.h>

#include "KaXmitCtlMainWindow.h"

#include <QTimer>

LOGGING("MainWindow")

using namespace XmlRpc;

const XmlRpcValue KaXmitCtlMainWindow::_NULL_XMLRPCVALUE;

KaXmitCtlMainWindow::KaXmitCtlMainWindow(std::string xmitterHost, 
    int xmitterPort) :
    QMainWindow(),
    _ui(),
    _updateTimer(this),
    _redLED(":/redLED.png"),
    _greenLED(":/greenLED.png"),
    _greenLED_off(":/greenLED_off.png"),
    _xmlrpcClient(xmitterHost.c_str(), xmitterPort) {
    // Set up the UI
    _ui.setupUi(this);
    
    connect(&_updateTimer, SIGNAL(timeout()), this, SLOT(_updateStatus()));
    _updateTimer.start(1000);
}

KaXmitCtlMainWindow::~KaXmitCtlMainWindow() {
}

void
KaXmitCtlMainWindow::on_powerButton_clicked() {
    XmlRpcValue result;
    std::string cmd = _unitOn() ? "powerOff" : "powerOn";
    DLOG << "Executing '" << cmd << "'";
    _xmlrpcClient.execute(cmd.c_str(), _NULL_XMLRPCVALUE, result);
    
    _updateStatus();
}

void
KaXmitCtlMainWindow::on_faultResetButton_clicked() {
    XmlRpcValue result;
    DLOG << "Executing 'faultReset'";
    _xmlrpcClient.execute("faultReset", _NULL_XMLRPCVALUE, result);
    
    _updateStatus();
}

void
KaXmitCtlMainWindow::on_standbyButton_clicked() {
    XmlRpcValue result;
    DLOG << "Executing 'standby'";
    _xmlrpcClient.execute("standby", _NULL_XMLRPCVALUE, result);
    
    _updateStatus();
}

void
KaXmitCtlMainWindow::on_operateButton_clicked() {
    XmlRpcValue result;
    DLOG << "Executing 'operate'";
    _xmlrpcClient.execute("operate", _NULL_XMLRPCVALUE, result);
    
    _updateStatus();
}

void
KaXmitCtlMainWindow::_updateStatus() {
    _xmlrpcClient.execute("getStatus", _NULL_XMLRPCVALUE, _statusDict);
    if (_xmlrpcClient.isFault()) {
        WLOG << __FUNCTION__ << ": XML-RPC fault on getStatus";
    }
    _ui.runupLabel->setEnabled(_hvpsRunup());
    _ui.standbyLabel->setEnabled(_standby());
    _ui.warmupLabel->setEnabled(_heaterWarmup());
    _ui.cooldownLabel->setEnabled(_cooldown());
    _ui.hvpsOnLabel->setEnabled(_hvpsOn());
    _ui.remoteEnabledLabel->setEnabled(_remoteEnabled());
    
    _ui.magCurrFaultIcon->setPixmap(_magnetronCurrentFault() ? _redLED : _greenLED);
    _ui.blowerFaultIcon->setPixmap(_blowerFault() ? _redLED : _greenLED);
    _ui.interlockFaultIcon->setPixmap(_safetyInterlock() ? _redLED : _greenLED);
    _ui.revPowerFaultIcon->setPixmap(_reversePowerFault() ? _redLED : _greenLED);
    _ui.pulseInputFaultIcon->setPixmap(_pulseInputFault() ? _redLED : _greenLED);
    _ui.hvpsCurrFaultIcon->setPixmap(_hvpsCurrentFault() ? _redLED : _greenLED);
    _ui.wgPresFaultIcon->setPixmap(_waveguidePressureFault() ? _redLED : _greenLED);
    _ui.hvpsUnderVFaultIcon->setPixmap(_hvpsUnderVoltage() ? _redLED : _greenLED);
    _ui.hvpsOverVFaultIcon->setPixmap(_hvpsOverVoltage() ? _redLED : _greenLED);
    
    QString txt;
    
    txt.setNum(_hvpsVoltage(), 'f', 1);
    _ui.hvpsVoltageValue->setText(txt);
    
    txt.setNum(_magnetronCurrent(), 'f', 1);
    _ui.magCurrentValue->setText(txt);
    
    txt.setNum(_hvpsCurrent(), 'f', 1);
    _ui.hvpsCurrentValue->setText(txt);
    
    txt.setNum(_temperature(), 'f', 0);
    _ui.temperatureValue->setText(txt);
    
    _ui.unitOnLabel->setPixmap(_unitOn() ? _greenLED : _greenLED_off);
    _ui.powerButton->setEnabled(_remoteEnabled());
    if (_unitOn() && _remoteEnabled()) {
        _ui.faultResetButton->setEnabled(_faultSummary());
        _ui.standbyButton->setEnabled(! _standby());
        _ui.operateButton->setEnabled(! _hvpsRunup());
    } else {
        _ui.faultResetButton->setEnabled(false);
        _ui.standbyButton->setEnabled(false);
        _ui.operateButton->setEnabled(false);
    }
}
