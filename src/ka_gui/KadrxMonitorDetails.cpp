/*
 * KadrxMonitorDetails.cpp
 *
 *  Created on: May 7, 2018
 *      Author: Chris Burghart <burghart@ucar.edu>
 */

#include "KadrxMonitorDetails.h"
#include <QtCore/QDateTime>

KadrxMonitorDetails::KadrxMonitorDetails(QWidget * parent) :
    _ui(),
    _redLED(":/redLED.png"),
    _redLED_off(":/redLED_off.png"),
    _greenLED(":/greenLED.png"),
    _greenLED_off(":/greenLED_off.png")
{
    _ui.setupUi(this);
}

KadrxMonitorDetails::~KadrxMonitorDetails() {
}

void
KadrxMonitorDetails::update(const KadrxStatus & status) {
    // AFC
    _ui.afcEnabledIcon->
        setPixmap(status.afcEnabled() ? _greenLED : _greenLED_off);
    _ui.oscillatorFrame->setEnabled(status.afcEnabled());
    _ui.osc0FreqValue->
        setText(QString::number(1.0e-9 * status.osc0Frequency(), 'f', 4));
    _ui.osc1FreqValue->
        setText(QString::number(1.0e-6 * status.osc1Frequency(), 'f', 2));
    _ui.osc2FreqValue->
        setText(QString::number(1.0e-9 * status.osc2Frequency(), 'f', 3));
    _ui.osc3FreqValue->
        setText(QString::number(1.0e-6 * status.osc3Frequency(), 'f', 2));
    _ui.derivedTxFreqValue->
        setText(QString::number(1.0e-9 * status.derivedTxFrequency(), 'f', 3));

    // powers
    _ui.hTxPowerValue->setText(QString::number(status.hTxPower(), 'f', 1));
    _ui.vTxPowerValue->setText(QString::number(status.vTxPower(), 'f', 1));
    _ui.testPulsePowerValue->
        setText(QString::number(status.testPulsePower(), 'f', 1));

    // temperatures
    _ui.drxComputerTempValue->
        setText(QString::number(status.procDrxTemp(), 'f', 1));
    _ui.procBoxTempValue->
        setText(QString::number(status.procEnclosureTemp(), 'f', 1));
    _ui.rxBackTempValue->
        setText(QString::number(status.rxBackTemp(), 'f', 1));
    _ui.rxBoxFrontValue->
        setText(QString::number(status.rxFrontTemp(), 'f', 1));
    _ui.rxBoxTopValue->
        setText(QString::number(status.rxTopTemp(), 'f', 1));
    _ui.txBoxTempValue->
        setText(QString::number(status.txEnclosureTemp(), 'f', 1));

    // PS voltage
    _ui.psVoltageValue->
        setText(QString::number(status.psVoltage(), 'f', 2));
}
