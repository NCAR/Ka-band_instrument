/*
 * KaXmitCtlMainWindow.cpp
 *
 *  Created on: Jan 14, 2011
 *      Author: burghart
 */
#include "KaXmitCtlMainWindow.h"

#include <sstream>
#include <unistd.h>
#include <logx/Logging.h>

#include <QDateTime>

LOGGING("MainWindow")


KaXmitCtlMainWindow::KaXmitCtlMainWindow(std::string xmitterHost, 
    int xmitterPort) :
    QMainWindow(),
    _ui(),
    _xmitClient(xmitterHost, xmitterPort),
    _updateTimer(this),
    _redLED(":/redLED.png"),
    _greenLED(":/greenLED.png"),
    _greenLED_off(":/greenLED_off.png"),
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

void
KaXmitCtlMainWindow::on_powerButton_clicked() {
    if (_status.unitOn()) {
        _xmitClient.powerOff();
    } else {
        _xmitClient.powerOn();
    }
    _update();
}

void
KaXmitCtlMainWindow::on_faultResetButton_clicked() {
    _xmitClient.faultReset();
    _update();
}

void
KaXmitCtlMainWindow::on_standbyButton_clicked() {
    _xmitClient.standby();
    _update();
}

void
KaXmitCtlMainWindow::_appendXmitdLogMsgs() {
    unsigned int firstIndex = _nextLogIndex;
    std::string msgs;
    _xmitClient.getLogMessages(firstIndex, msgs, _nextLogIndex);
    if (_nextLogIndex != firstIndex) {
        _ui.logArea->appendPlainText(msgs.c_str());
    }
}

void
KaXmitCtlMainWindow::on_operateButton_clicked() {
    _xmitClient.operate();
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
    _status = XmitClient::XmitStatus(); // start with uninitialized status
    if (! _xmitClient.getStatus(_status)) {
        _noDaemon();
        return;
    } else if (! _status.serialConnected()) {
        _noXmitter();
        return;
    }
    
    _enableUi();

    // boolean status values
    _ui.runupLabel->setEnabled(_status.hvpsRunup());
    _ui.standbyLabel->setEnabled(_status.standby());
    _ui.warmupLabel->setEnabled(_status.heaterWarmup());
    _ui.cooldownLabel->setEnabled(_status.cooldown());
    _ui.hvpsOnLabel->setEnabled(_status.hvpsOn());
    _ui.remoteEnabledLabel->setEnabled(_status.remoteEnabled());
    
    // fault lights
    _ui.magCurrFaultIcon->setPixmap(_status.magnetronCurrentFault() ? _redLED : _greenLED);
    _ui.blowerFaultIcon->setPixmap(_status.blowerFault() ? _redLED : _greenLED);
    _ui.interlockFaultIcon->setPixmap(_status.safetyInterlock() ? _redLED : _greenLED);
    _ui.revPowerFaultIcon->setPixmap(_status.reversePowerFault() ? _redLED : _greenLED);
    _ui.pulseInputFaultIcon->setPixmap(_status.pulseInputFault() ? _redLED : _greenLED);
    _ui.hvpsCurrFaultIcon->setPixmap(_status.hvpsCurrentFault() ? _redLED : _greenLED);
    _ui.wgPresFaultIcon->setPixmap(_status.waveguidePressureFault() ? _redLED : _greenLED);
    _ui.hvpsUnderVFaultIcon->setPixmap(_status.hvpsUnderVoltage() ? _redLED : _greenLED);
    _ui.hvpsOverVFaultIcon->setPixmap(_status.hvpsOverVoltage() ? _redLED : _greenLED);
    
    // fault counts
    _ui.magCurrFaultCount->setText(_countLabel(_status.magnetronCurrentFaultCount()));
    _ui.blowerFaultCount->setText(_countLabel(_status.blowerFaultCount()));
    _ui.interlockFaultCount->setText(_countLabel(_status.safetyInterlockCount()));
    _ui.revPowerFaultCount->setText(_countLabel(_status.reversePowerFaultCount()));
    _ui.pulseInputFaultCount->setText(_countLabel(_status.pulseInputFaultCount()));
    _ui.hvpsCurrFaultCount->setText(_countLabel(_status.hvpsCurrentFaultCount()));
    _ui.wgPresFaultCount->setText(_countLabel(_status.waveguidePressureFaultCount()));
    _ui.hvpsUnderVFaultCount->setText(_countLabel(_status.hvpsUnderVoltageCount()));
    _ui.hvpsOverVFaultCount->setText(_countLabel(_status.hvpsOverVoltageCount()));
    
    QString txt;
    txt.setNum(_status.autoPulseFaultResets());
    _ui.autoResetCount->setText(txt);
    
    
    // Text displays for voltage, currents, and temperature
    txt.setNum(_status.hvpsVoltage(), 'f', 1);
    _ui.hvpsVoltageValue->setText(txt);
    
    txt.setNum(_status.magnetronCurrent(), 'f', 1);
    _ui.magCurrentValue->setText(txt);
    
    txt.setNum(_status.hvpsCurrent(), 'f', 1);
    _ui.hvpsCurrentValue->setText(txt);
    
    txt.setNum(_status.temperature(), 'f', 0);
    _ui.temperatureValue->setText(txt);
    
    // "unit on" light
    _ui.unitOnLabel->setPixmap(_status.unitOn() ? _greenLED : _greenLED_off);
    
    // enable/disable buttons
    _ui.powerButton->setEnabled(_status.remoteEnabled());
    if (_status.remoteEnabled() && _status.unitOn()) {
        _ui.faultResetButton->setEnabled(_status.faultSummary());
        if (_status.faultSummary()) {
            _ui.standbyButton->setEnabled(false);
            _ui.operateButton->setEnabled(false);
        } else {
            _ui.standbyButton->setEnabled(_status.hvpsRunup() && ! _status.heaterWarmup());
            _ui.operateButton->setEnabled(! _status.hvpsRunup() && ! _status.heaterWarmup());
        }
    } else {
        _ui.faultResetButton->setEnabled(false);
        _ui.standbyButton->setEnabled(false);
        _ui.operateButton->setEnabled(false);
    }
    
    if (_status.remoteEnabled()) {
        statusBar()->clearMessage();
    } else {
        statusBar()->showMessage("Remote control is currently DISABLED");
    }
}

void
KaXmitCtlMainWindow::_noDaemon() {
    // Note lack of daemon connection in the status bar
    std::ostringstream ss;
    ss << "No connection to ka_xmitd @ " << _xmitClient.getXmitdHost() << ":" <<
            _xmitClient.getXmitdPort();
    statusBar()->showMessage(ss.str().c_str());
    // If we've been in contact with a ka_xmitd, log that we lost contact
    if (_nextLogIndex > 0) {
        ss.seekp(0); // start over in the ostringstream
        ss << "Lost contact with ka_xmitd @ " << _xmitClient.getXmitdHost() <<
                ":" << _xmitClient.getXmitdPort();
        _logMessage(ss.str());
    }
    // If we lose contact with ka_xmitd, reset _nextLogIndex to zero so we
    // start fresh when we connect again
    _nextLogIndex = 0;
    // Disable the UI when we are out of contact
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
            QDateTime::currentDateTime().toUTC().toString("yyyy-MM-dd hh:mm:ss ") + 
            message.c_str());
}
