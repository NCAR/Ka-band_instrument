/*
 * XmitterFaultDetails.cpp
 *
 *  Created on: May 7, 2018
 *      Author: Chris Burghart <burghart@ucar.edu>
 */

#include "XmitterFaultDetails.h"
#include <QtCore/QDateTime>

XmitterFaultDetails::XmitterFaultDetails(QWidget * parent) :
    _ui(),
    _redLED(":/redLED.png"),
    _redLED_off(":/redLED_off.png"),
    _greenLED(":/greenLED.png"),
    _greenLED_off(":/greenLED_off.png")
{
    _ui.setupUi(this);
}

XmitterFaultDetails::~XmitterFaultDetails() {
}

QString
XmitterFaultDetails::_countLabel(int count) {
    if (count == 0)
        return QString("-");

    QString txt;
    txt.setNum(count);
    return txt;
}

QString
XmitterFaultDetails::_faultTimeLabel(time_t time) {
    if (time == -1)
        return QString("");
    
    QDateTime nowQDT = QDateTime::currentDateTime().toUTC();
    QDateTime faultQDT = QDateTime::fromTime_t(time).toUTC();
    if (faultQDT.date() == nowQDT.date()) {
        return(faultQDT.toString("hh:mm:ss"));
    } else {
        return(faultQDT.toString("MM/dd hh:mm:ss"));
    }
}

void
XmitterFaultDetails::update(const XmitClient::XmitStatus & status) {
    // fault lights
    _ui.magCurrFaultIcon->
        setPixmap(status.magnetronCurrentFault() ? _redLED : _redLED_off);
    _ui.blowerFaultIcon->
        setPixmap(status.blowerFault() ? _redLED : _redLED_off);
    _ui.interlockFaultIcon->
        setPixmap(status.safetyInterlock() ? _redLED : _redLED_off);
    _ui.revPowerFaultIcon->
        setPixmap(status.reversePowerFault() ? _redLED : _redLED_off);
    _ui.pulseInputFaultIcon->
        setPixmap(status.pulseInputFault() ? _redLED : _redLED_off);
    _ui.hvpsCurrFaultIcon->
        setPixmap(status.hvpsCurrentFault() ? _redLED : _redLED_off);
    _ui.wgPresFaultIcon->
        setPixmap(status.waveguidePressureFault() ? _redLED : _redLED_off);
    _ui.hvpsUnderVFaultIcon->
        setPixmap(status.hvpsUnderVoltage() ? _redLED : _redLED_off);
    _ui.hvpsOverVFaultIcon->
        setPixmap(status.hvpsOverVoltage() ? _redLED : _redLED_off);
        
    // fault counts
    _ui.magCurrFaultCount->
        setText(_countLabel(status.magnetronCurrentFaultCount()));
    _ui.blowerFaultCount->
        setText(_countLabel(status.blowerFaultCount()));
    _ui.interlockFaultCount->
        setText(_countLabel(status.safetyInterlockCount()));
    _ui.revPowerFaultCount->
        setText(_countLabel(status.reversePowerFaultCount()));
    _ui.pulseInputFaultCount->
        setText(_countLabel(status.pulseInputFaultCount()));
    _ui.autoResetCount->setText(QString::number(status.autoPulseFaultResets()));
    _ui.hvpsCurrFaultCount->
        setText(_countLabel(status.hvpsCurrentFaultCount()));
    _ui.wgPresFaultCount->
        setText(_countLabel(status.waveguidePressureFaultCount()));
    _ui.hvpsUnderVFaultCount->
        setText(_countLabel(status.hvpsUnderVoltageCount()));
    _ui.hvpsOverVFaultCount->
        setText(_countLabel(status.hvpsOverVoltageCount()));
    
    // latest fault times
    _ui.magCurrFaultTime->
        setText(_faultTimeLabel(status.magnetronCurrentFaultTime()));
    _ui.blowerFaultTime->
        setText(_faultTimeLabel(status.blowerFaultTime()));
    _ui.interlockFaultTime->
        setText(_faultTimeLabel(status.safetyInterlockTime()));
    _ui.revPowerFaultTime->
        setText(_faultTimeLabel(status.reversePowerFaultTime()));
    _ui.pulseInputFaultTime->
        setText(_faultTimeLabel(status.pulseInputFaultTime()));
    _ui.hvpsCurrFaultTime->
        setText(_faultTimeLabel(status.hvpsCurrentFaultTime()));
    _ui.wgPresFaultTime->
        setText(_faultTimeLabel(status.waveguidePressureFaultTime()));
    _ui.hvpsUnderVFaultTime->
        setText(_faultTimeLabel(status.hvpsUnderVoltageTime()));
    _ui.hvpsOverVFaultTime->
        setText(_faultTimeLabel(status.hvpsOverVoltageTime()));

    // If we have a fault and remote control is allowed, enable the
    // "Fault Reset" button, otherwise disable it.
    _ui.faultResetButton->
        setEnabled(status.faultSummary() && status.unitOn() && status.remoteEnabled());
}

void
XmitterFaultDetails::clearWidget() {
    // fault lights
    _ui.magCurrFaultIcon->setPixmap(_redLED_off);
    _ui.blowerFaultIcon->setPixmap(_redLED_off);
    _ui.interlockFaultIcon->setPixmap(_redLED_off);
    _ui.revPowerFaultIcon->setPixmap(_redLED_off);
    _ui.pulseInputFaultIcon->setPixmap(_redLED_off);
    _ui.hvpsCurrFaultIcon->setPixmap(_redLED_off);
    _ui.wgPresFaultIcon->setPixmap(_redLED_off);
    _ui.hvpsUnderVFaultIcon->setPixmap(_redLED_off);
    _ui.hvpsOverVFaultIcon->setPixmap(_redLED_off);
        
    // fault counts
    _ui.magCurrFaultCount->setText("");
    _ui.blowerFaultCount->setText("");
    _ui.interlockFaultCount->setText("");
    _ui.revPowerFaultCount->setText("");
    _ui.pulseInputFaultCount->setText("");
    _ui.autoResetCount->setText("");
    _ui.hvpsCurrFaultCount->setText("");
    _ui.wgPresFaultCount->setText("");
    _ui.hvpsUnderVFaultCount->setText("");
    _ui.hvpsOverVFaultCount->setText("");
    
    // latest fault times
    _ui.magCurrFaultTime->setText("");
    _ui.blowerFaultTime->setText("");
    _ui.interlockFaultTime->setText("");
    _ui.revPowerFaultTime->setText("");
    _ui.pulseInputFaultTime->setText("");
    _ui.hvpsCurrFaultTime->setText("");
    _ui.wgPresFaultTime->setText("");
    _ui.hvpsUnderVFaultTime->setText("");
    _ui.hvpsOverVFaultTime->setText("");

    // If we have a fault and remote control is allowed, enable the
    // "Fault Reset" button, otherwise disable it.
    _ui.faultResetButton->setEnabled(false);
}
