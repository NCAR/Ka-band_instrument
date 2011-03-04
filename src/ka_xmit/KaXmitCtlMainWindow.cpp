/*
 * KaXmitCtlMainWindow.cpp
 *
 *  Created on: Jan 14, 2011
 *      Author: burghart
 */
#include <sstream>
#include <unistd.h>
#include <logx/Logging.h>

#include "KaXmitCtlMainWindow.h"

#include <QDateTime>

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
    _nextLogIndex(0) {
    // Set up the UI
    _ui.setupUi(this);
    // Limit the log area to 1000 messages
    _ui.logArea->setMaximumBlockCount(1000);
    _logMessage("ka_xmitctl started");
    
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
        _noDaemon();
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

void
KaXmitCtlMainWindow::on_faultResetButton_clicked() {
    XmlRpcValue result;
    _executeXmlRpcCommand("faultReset", _NULL_XMLRPCVALUE, result);
    _update();
}

void
KaXmitCtlMainWindow::on_standbyButton_clicked() {
    XmlRpcValue result;
    _executeXmlRpcCommand("standby", _NULL_XMLRPCVALUE, result);
    _update();
}

void
KaXmitCtlMainWindow::_operate() {
    XmlRpcValue result;
    _executeXmlRpcCommand("operate", _NULL_XMLRPCVALUE, result);
}

void
KaXmitCtlMainWindow::_getStatus() {
    if (! _executeXmlRpcCommand("getStatus", _NULL_XMLRPCVALUE, _statusDict))
        _logMessage("getStatus failed!");
}

void
KaXmitCtlMainWindow::_appendXmitdLogMsgs() {
    XmlRpcValue startIndex = int(_nextLogIndex);
    XmlRpcValue result;
    if (! _executeXmlRpcCommand("getLogMessages", startIndex, result))
        _logMessage("getLogMsgs failed!");
    int nextIndex = int(result["nextIndex"]);
    if (nextIndex != int(_nextLogIndex)) {
        std::string msgs = std::string(result["logMessages"]);
        // Append the returned messages to our log area
        _ui.logArea->appendPlainText(std::string(result["logMessages"]).c_str());
        // update _nextLogIndex
        _nextLogIndex = (unsigned int)(nextIndex);
    }
}

void
KaXmitCtlMainWindow::on_operateButton_clicked() {
    _operate();
    _update();
}

void
KaXmitCtlMainWindow::_update() {
    // Update the current time string
    char timestring[32];
    time_t now = time(0);
    strftime(timestring, sizeof(timestring) - 1, "%F %T", gmtime(&now));
    _ui.clockLabel->setText(timestring);
    
    // Append new log messages from ka_xmitd
    _appendXmitdLogMsgs();
    
    // Get status from ka_xmitd
    _getStatus();

    if (! _serialConnected()) {
        _noXmitter();
        return;
    }
    
    _enableUi();

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
    
    // fault counts
    _ui.magCurrFaultCount->setText(_countLabel(_magnetronCurrentFaultCount()));
    _ui.blowerFaultCount->setText(_countLabel(_blowerFaultCount()));
    _ui.interlockFaultCount->setText(_countLabel(_safetyInterlockCount()));
    _ui.revPowerFaultCount->setText(_countLabel(_reversePowerFaultCount()));
    _ui.pulseInputFaultCount->setText(_countLabel(_pulseInputFaultCount()));
    _ui.hvpsCurrFaultCount->setText(_countLabel(_hvpsCurrentFaultCount()));
    _ui.wgPresFaultCount->setText(_countLabel(_waveguidePressureFaultCount()));
    _ui.hvpsUnderVFaultCount->setText(_countLabel(_hvpsUnderVoltageCount()));
    _ui.hvpsOverVFaultCount->setText(_countLabel(_hvpsOverVoltageCount()));
    
    QString txt;
    txt.setNum(_autoPulseFaultResets());
    _ui.autoResetCount->setText(txt);
    
    
    // Text displays for voltage, currents, and temperature
    txt.setNum(_hvpsVoltage(), 'f', 1);
    _ui.hvpsVoltageValue->setText(txt);
    
    txt.setNum(_magnetronCurrent(), 'f', 1);
    _ui.magCurrentValue->setText(txt);
    
    txt.setNum(_hvpsCurrent(), 'f', 1);
    _ui.hvpsCurrentValue->setText(txt);
    
    txt.setNum(_temperature(), 'f', 0);
    _ui.temperatureValue->setText(txt);
    
    // "unit on" light
    _ui.unitOnLabel->setPixmap(_unitOn() ? _greenLED : _greenLED_off);
    
    // enable/disable buttons
    _ui.powerButton->setEnabled(_remoteEnabled());
    if (_remoteEnabled() && _unitOn()) {
        _ui.faultResetButton->setEnabled(_faultSummary());
        if (_faultSummary()) {
            _ui.standbyButton->setEnabled(false);
            _ui.operateButton->setEnabled(false);
        } else {
            _ui.standbyButton->setEnabled(_hvpsRunup() && ! _heaterWarmup());
            _ui.operateButton->setEnabled(! _hvpsRunup() && ! _heaterWarmup());
        }
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

int
KaXmitCtlMainWindow::_statusInt(std::string key) {
    if (! _statusDict.hasMember(key)) {
        ELOG << "Status dictionary does not contain requested key '" << key <<
            "'!";
        abort();
    } else {
        return(int(_statusDict[key]));
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
KaXmitCtlMainWindow::_noDaemon() {
    std::ostringstream ss;
    ss << "No connection to ka_xmitd @ " << _xmitterHost << ":" << _xmitterPort;
    statusBar()->showMessage(ss.str().c_str());
    _disableUi();
}

void
KaXmitCtlMainWindow::_noXmitter() {
    statusBar()->showMessage("No serial connection from ka_xmitd to xmitter!");
    _disableUi();
}

void
KaXmitCtlMainWindow::_disableUi() {
    _ui.statusBox->setEnabled(false);
    _ui.faultStatusBox->setEnabled(false);

    _ui.unitOnLabel->setPixmap(_greenLED_off);
    
    _ui.magCurrFaultIcon->setPixmap(_greenLED_off);
    _ui.magCurrFaultCount->setText("");
    _ui.blowerFaultIcon->setPixmap(_greenLED_off);
    _ui.blowerFaultCount->setText("");
    _ui.interlockFaultIcon->setPixmap(_greenLED_off);
    _ui.interlockFaultCount->setText("");
    _ui.revPowerFaultIcon->setPixmap(_greenLED_off);
    _ui.revPowerFaultCount->setText("");
    _ui.pulseInputFaultIcon->setPixmap(_greenLED_off);
    _ui.pulseInputFaultCount->setText("");
    _ui.hvpsCurrFaultIcon->setPixmap(_greenLED_off);
    _ui.hvpsCurrFaultCount->setText("");
    _ui.wgPresFaultIcon->setPixmap(_greenLED_off);
    _ui.wgPresFaultCount->setText("");
    _ui.hvpsUnderVFaultIcon->setPixmap(_greenLED_off);
    _ui.hvpsUnderVFaultCount->setText("");
    _ui.hvpsOverVFaultIcon->setPixmap(_greenLED_off);
    _ui.hvpsOverVFaultCount->setText("");
    
    _ui.hvpsVoltageValue->setText("0.0");
    _ui.magCurrentValue->setText("0.0");
    _ui.hvpsCurrentValue->setText("0.0");
    _ui.temperatureValue->setText("0");
    
    _ui.autoResetCount->setText("");
}

void
KaXmitCtlMainWindow::_enableUi() {
    _ui.faultStatusBox->setEnabled(true);
    _ui.statusBox->setEnabled(true);
}

void
KaXmitCtlMainWindow::_logMessage(std::string message) {
    _ui.logArea->appendPlainText(
            QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss ") + 
            message.c_str());
}
