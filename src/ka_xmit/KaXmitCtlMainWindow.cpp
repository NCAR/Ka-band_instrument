/*
 * KaXmitCtlMainWindow.cpp
 *
 *  Created on: Jan 14, 2011
 *      Author: burghart
 */
#include <sstream>
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
    _xmitterHost(xmitterHost),
    _xmitterPort(xmitterPort),
    _updateTimer(this),
    _redLED(":/redLED.png"),
    _greenLED(":/greenLED.png"),
    _greenLED_off(":/greenLED_off.png"),
    _xmlrpcClient(_xmitterHost.c_str(), _xmitterPort),
    _statusDict(),
    _pulseInputFaultTimes(),
    _doAutoFaultReset(true),
    _autoResetCount(0) {
    // Set up the UI
    _ui.setupUi(this);
    
    connect(&_updateTimer, SIGNAL(timeout()), this, SLOT(_update()));
    _updateTimer.start(1000);
}

KaXmitCtlMainWindow::~KaXmitCtlMainWindow() {
}

bool
KaXmitCtlMainWindow::_executeXmlRpcCommand(const std::string cmd, 
    const XmlRpcValue & params, XmlRpcValue & result) {
    DLOG << "Executing '" << cmd << "()' command";
    if (! _xmlrpcClient.execute(cmd.c_str(), params, result)) {
        DLOG << "Error executing " << cmd << "() call to ka_xmitd";
        _noConnection();
        return(false);
    }
    if (_xmlrpcClient.isFault()) {
        ELOG << "XML-RPC fault on " << cmd << "() call";
        abort();
    }
    return true;  
}

void
KaXmitCtlMainWindow::on_powerButton_clicked() {
    XmlRpcValue result;
    _executeXmlRpcCommand((_unitOn() ? "powerOff" : "powerOn"),
        _NULL_XMLRPCVALUE, result);
    _update();
}

void KaXmitCtlMainWindow::_faultReset() {
    XmlRpcValue result;
    _executeXmlRpcCommand("faultReset", _NULL_XMLRPCVALUE, result);
    // When a faultReset command is issued, allow auto-resets on pulse faults
    // again.
    _doAutoFaultReset = true;
}
    
void
KaXmitCtlMainWindow::on_faultResetButton_clicked() {
    _faultReset();
    _update();
}

void
KaXmitCtlMainWindow::on_standbyButton_clicked() {
    XmlRpcValue result;
    _executeXmlRpcCommand("standby", _NULL_XMLRPCVALUE, result);
    _update();
}

void
KaXmitCtlMainWindow::on_operateButton_clicked() {
    XmlRpcValue result;
    _executeXmlRpcCommand("operate", _NULL_XMLRPCVALUE, result);
    _update();
}

void
KaXmitCtlMainWindow::_update() {
    if (! _executeXmlRpcCommand("getStatus", _NULL_XMLRPCVALUE, _statusDict)) {
        return;
    }
    
    // boolean status values
    _ui.runupLabel->setEnabled(_hvpsRunup());
    _ui.standbyLabel->setEnabled(_standby());
    _ui.warmupLabel->setEnabled(_heaterWarmup());
    _ui.cooldownLabel->setEnabled(_cooldown());
    _ui.hvpsOnLabel->setEnabled(_hvpsOn());
    _ui.remoteEnabledLabel->setEnabled(_remoteEnabled());
    
    // fault lights
    _ui.magCurrFaultIcon->setPixmap(_magnetronCurrentFault() ? _redLED : _greenLED);
    _ui.blowerFaultIcon->setPixmap(_blowerFault() ? _redLED : _greenLED);
    _ui.interlockFaultIcon->setPixmap(_safetyInterlock() ? _redLED : _greenLED);
    _ui.revPowerFaultIcon->setPixmap(_reversePowerFault() ? _redLED : _greenLED);
    _ui.pulseInputFaultIcon->setPixmap(_pulseInputFault() ? _redLED : _greenLED);
    _ui.hvpsCurrFaultIcon->setPixmap(_hvpsCurrentFault() ? _redLED : _greenLED);
    _ui.wgPresFaultIcon->setPixmap(_waveguidePressureFault() ? _redLED : _greenLED);
    _ui.hvpsUnderVFaultIcon->setPixmap(_hvpsUnderVoltage() ? _redLED : _greenLED);
    _ui.hvpsOverVFaultIcon->setPixmap(_hvpsOverVoltage() ? _redLED : _greenLED);
    
    // Text displays for voltage, currents, and temperature
    QString txt;
    
    txt.setNum(_hvpsVoltage(), 'f', 1);
    _ui.hvpsVoltageValue->setText(txt);
    
    txt.setNum(_magnetronCurrent(), 'f', 1);
    _ui.magCurrentValue->setText(txt);
    
    txt.setNum(_hvpsCurrent(), 'f', 1);
    _ui.hvpsCurrentValue->setText(txt);
    
    txt.setNum(_temperature(), 'f', 0);
    _ui.temperatureValue->setText(txt);
    
    txt.setNum(_autoResetCount);
    _ui.autoResetValue->setText(txt);
    
    // "unit on" light
    _ui.unitOnLabel->setPixmap(_unitOn() ? _greenLED : _greenLED_off);
    
    // enable/disable buttons
    _ui.powerButton->setEnabled(_remoteEnabled());
    if (_remoteEnabled() && _unitOn()) {
        _ui.faultResetButton->setEnabled(_faultSummary());
        _ui.standbyButton->setEnabled(! _standby());
        _ui.operateButton->setEnabled(! _hvpsRunup());
    } else {
        _ui.faultResetButton->setEnabled(false);
        _ui.standbyButton->setEnabled(false);
        _ui.operateButton->setEnabled(false);
    }
    
    if (_remoteEnabled()) {
        statusBar()->clearMessage();
    } else {
        statusBar()->showMessage("Remote control is currently DISABLED");
    }
    
    // Special handling for pulse input faults
    if (_pulseInputFault()) {
        _handlePulseInputFault();
    }
}

bool
KaXmitCtlMainWindow::_statusBool(std::string key) {
    if (! _statusDict.hasMember(key)) {
        ELOG << "Status dictionary does not contain requested key '" << key <<
            "'!";
        abort();
    } else {
        return(bool(_statusDict[key]));
    }
}

double
KaXmitCtlMainWindow::_statusDouble(std::string  key) {
    if (! _statusDict.hasMember(key)) {
        ELOG << "Status dictionary does not contain requested key '" << key <<
            "'!";
        abort();
    } else {
        return(double(_statusDict[key]));
    }
}
 
void
KaXmitCtlMainWindow::_noConnection() {
    std::ostringstream ss;
    ss << "No connection to ka_xmitd @ " << _xmitterHost << ":" << _xmitterPort;
    statusBar()->showMessage(ss.str().c_str());
    
    _ui.runupLabel->setEnabled(false);
    _ui.standbyLabel->setEnabled(false);
    _ui.warmupLabel->setEnabled(false);
    _ui.cooldownLabel->setEnabled(false);
    _ui.hvpsOnLabel->setEnabled(false);
    _ui.remoteEnabledLabel->setEnabled(false);
    
    _ui.unitOnLabel->setPixmap(_greenLED_off);
    
    _ui.magCurrFaultIcon->setPixmap(_greenLED_off);
    _ui.blowerFaultIcon->setPixmap(_greenLED_off);
    _ui.interlockFaultIcon->setPixmap(_greenLED_off);
    _ui.revPowerFaultIcon->setPixmap(_greenLED_off);
    _ui.pulseInputFaultIcon->setPixmap(_greenLED_off);
    _ui.hvpsCurrFaultIcon->setPixmap(_greenLED_off);
    _ui.wgPresFaultIcon->setPixmap(_greenLED_off);
    _ui.hvpsUnderVFaultIcon->setPixmap(_greenLED_off);
    _ui.hvpsOverVFaultIcon->setPixmap(_greenLED_off);
    
    _ui.hvpsVoltageValue->setText("0.0");
    _ui.magCurrentValue->setText("0.0");
    _ui.hvpsCurrentValue->setText("0.0");
    _ui.temperatureValue->setText("0");
    _ui.autoResetValue->setText("0");
    
    _ui.powerButton->setEnabled(false);
    _ui.faultResetButton->setEnabled(false);
    _ui.standbyButton->setEnabled(false);
    _ui.operateButton->setEnabled(false);
}

void
KaXmitCtlMainWindow::_handlePulseInputFault() {
    // If auto-resets are disabled, just return now
    if (! _doAutoFaultReset)
        return;

    // Push the time of this fault on to our deque
    _pulseInputFaultTimes.push_back(time(0));
    // Pop the earliest entry when we hit MAX_ENTRIES+1 entries.
    const unsigned int MAX_ENTRIES = 10;
    if (_pulseInputFaultTimes.size() == (MAX_ENTRIES + 1))
        _pulseInputFaultTimes.pop_front();
    // Issue a "faultReset" command unless the time span for the last 
    // MAX_ENTRIES faults is less than 100 seconds.
    if (_pulseInputFaultTimes.size() < MAX_ENTRIES || 
        ((_pulseInputFaultTimes[MAX_ENTRIES - 1] - _pulseInputFaultTimes[0]) > 100)) {
        _faultReset();
        _autoResetCount++;
        statusBar()->showMessage("Pulse input fault auto reset");
    } else {
        std::ostringstream ss;
        ss << "Auto fault reset disabled after 10 faults in " << 
            (_pulseInputFaultTimes[MAX_ENTRIES - 1] - _pulseInputFaultTimes[0]) << 
            " seconds!";
        statusBar()->showMessage(ss.str().c_str());
        // Disable auto fault resets. They will be reenabled if the user
        // pushes the "Fault Reset" button.
        _doAutoFaultReset = false;
    }
    return;     
}
