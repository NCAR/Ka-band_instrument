/*
 * KaGuiMainWindow.cpp
 *
 *  Created on: May 3, 2018
 *      Author: Chris Burghart <burghart@ucar.edu>
 */
#include "KaGuiMainWindow.h"

#include <sstream>
#include <unistd.h>
#include <logx/Logging.h>

#include <QtCore/QDateTime>

LOGGING("KaGuiMainWindow")


KaGuiMainWindow::KaGuiMainWindow(std::string xmitterHost, int xmitterPort) :
    QMainWindow(),
    _ui(),
    _xmitterFaultDetails(this),
    _xmitClient(xmitterHost, xmitterPort),
    _updateTimer(this),
    _redLED(":/redLED.png"),
    _redLED_off(":/redLED_off.png"),
    _greenLED(":/greenLED.png"),
    _greenLED_off(":/greenLED_off.png"),
    _noXmitd(true) {
    // Set up the UI
    _ui.setupUi(this);

    connect(&_updateTimer, SIGNAL(timeout()), this, SLOT(_update()));
    _updateTimer.start(1000);

    connect(&_xmitterFaultDetails, SIGNAL(faultResetClicked()),
            this, SLOT(resetXmitterFault()));
}

KaGuiMainWindow::~KaGuiMainWindow() {
}

void
KaGuiMainWindow::on_xmitterPowerButton_clicked() {
    if (_xmitdStatus.unitOn()) {
        _xmitClient.powerOff();
    } else {
        _xmitClient.powerOn();
    }
    _update();
}

void
KaGuiMainWindow::resetXmitterFault() {
    _xmitClient.faultReset();
    _update();
}

void
KaGuiMainWindow::on_xmitterStandbyButton_clicked() {
    _xmitClient.standby();
    _update();
}

void
KaGuiMainWindow::on_xmitterOperateButton_clicked() {
    _xmitClient.operate();
    _update();
}

void
KaGuiMainWindow::on_xmitterFaultDetailsButton_clicked() {
    _xmitterFaultDetails.show();
}

QString
KaGuiMainWindow::_ColorText(QString text, QString colorName) {
    return(QString("<font color='" + colorName + "'>" + text + "</font>"));
}
void
KaGuiMainWindow::_update() {
    // Get status from ka_xmitd
    _xmitdStatus = XmitdStatus(); // start with uninitialized status
    if (! _xmitClient.getStatus(_xmitdStatus)) {
        _noXmitDaemon();
        return;
    }

    // If we had been out of contact with ka_xmitd, log the new contact
    if (_noXmitd) {
        // Log new contact with ka_xmitd
	std::ostringstream os;
	os << "Connected to ka_xmitd @ " << _xmitClient.getXmitdHost() << ":" <<
		_xmitClient.getXmitdPort();
	ILOG << os.str();
	_noXmitd = false;
    } 
    if (! _xmitdStatus.serialConnected()) {
        _noXmitterSerial();
        return;
    }
    
    _enableXmitterUi();

    // boolean status values
    _ui.xmitterHvpsRunupIcon->
        setPixmap(_xmitdStatus.hvpsRunup() ? _greenLED : _greenLED_off);
    _ui.xmitterStandbyIcon->
        setPixmap(_xmitdStatus.standby() ? _greenLED : _greenLED_off);
    _ui.xmitterWarmupIcon->
        setPixmap(_xmitdStatus.heaterWarmup() ? _greenLED : _greenLED_off);
    _ui.xmitterCooldownIcon->
        setPixmap(_xmitdStatus.cooldown() ? _greenLED : _greenLED_off);
    _ui.xmitterHvpsOnIcon->
        setPixmap(_xmitdStatus.hvpsOn() ? _greenLED : _greenLED_off);
    
    // Turn on the Fault LED if any transmitter faults are active
    _ui.xmitterFaultIcon->
        setPixmap(_xmitdStatus.faultSummary() ? _redLED : _redLED_off);

    // Populate the fault details dialog
    _xmitterFaultDetails.update(_xmitdStatus);

    // Text displays for voltage, currents, and temperature
    QString txt;
    txt.setNum(_xmitdStatus.hvpsVoltage(), 'f', 1);
    _ui.xmitterHvpsVoltageValue->setText(txt);
    
    txt.setNum(_xmitdStatus.magnetronCurrent(), 'f', 1);
    _ui.xmitterMagCurrentValue->setText(txt);
    
    txt.setNum(_xmitdStatus.hvpsCurrent(), 'f', 1);
    _ui.xmitterHvpsCurrentValue->setText(txt);
    
    txt.setNum(_xmitdStatus.temperature(), 'f', 0);
    _ui.xmitterTemperatureValue->setText(txt);
    
    // "unit on" light
    _ui.xmitterUnitOnIcon->setPixmap(_xmitdStatus.unitOn() ? _greenLED : _greenLED_off);
    
    // If the transmitter is allowing remote control, enable the transmitter
    // control button box on the GUI
    bool remoteEnabled = _xmitdStatus.remoteEnabled();
    if (! remoteEnabled) {
        _ui.xmitterStatusLabel->
            setText(_ColorText("Remote xmitter control is disabled", "darkred"));
    }
    _ui.xmitterControlFrame->setEnabled(remoteEnabled);
    _ui.xmitterPowerButton->setEnabled(remoteEnabled);

    // Set enable/disable state of the Standby and Operate buttons
    if (remoteEnabled && _xmitdStatus.unitOn()) {
        if (_xmitdStatus.faultSummary()) {
            _ui.xmitterStandbyButton->setEnabled(false);
            _ui.xmitterOperateButton->setEnabled(false);
        } else {
            _ui.xmitterStandbyButton->
                setEnabled(_xmitdStatus.hvpsRunup() && ! _xmitdStatus.heaterWarmup());
            _ui.xmitterOperateButton->
                setEnabled(! _xmitdStatus.hvpsRunup() && ! _xmitdStatus.heaterWarmup());
        }
    } else {
        _ui.xmitterStandbyButton->setEnabled(false);
        _ui.xmitterOperateButton->setEnabled(false);
    }

}

void
KaGuiMainWindow::_noXmitDaemon() {
    // Log lack of xmitd connection in the status bar
    std::ostringstream os;
    os << "No ka_xmitd @ " <<
          _xmitClient.getXmitdHost() << ":" << _xmitClient.getXmitdPort();
    _ui.xmitterStatusLabel->setText(_ColorText(os.str().c_str(), "darkred"));

    // If we were previously in contact with ka_xmitd, log that we lost contact
    if (_noXmitd == false) {
        ELOG << os.str();
    }

    // Disable the transmitter part of the UI when we are out of contact
    _noXmitd = true;
    _disableXmitterUi();
}

void
KaGuiMainWindow::_noXmitterSerial() {
    std::ostringstream os;
    os << "No serial connection from ka_xmitd to xmitter!";
    _ui.xmitterStatusLabel->setText(_ColorText(os.str().c_str(), "darkred"));
    _disableXmitterUi();
}

void
KaGuiMainWindow::_disableXmitterUi() {
    _ui.xmitterControlAndStateFrame->setEnabled(false);
    _ui.xmitterValuesFrame->setEnabled(false);

    // Transmitter fault details
    _xmitterFaultDetails.setEnabled(false);
    _xmitterFaultDetails.clearWidget();

    _ui.xmitterUnitOnIcon->setPixmap(_greenLED_off);
    _ui.xmitterWarmupIcon->setPixmap(_greenLED_off);
    _ui.xmitterStandbyIcon->setPixmap(_greenLED_off);
    _ui.xmitterHvpsRunupIcon->setPixmap(_greenLED_off);
    _ui.xmitterHvpsOnIcon->setPixmap(_greenLED_off);
    _ui.xmitterCooldownIcon->setPixmap(_greenLED_off);
    _ui.xmitterFaultIcon->setPixmap(_redLED_off);

    _ui.xmitterHvpsVoltageValue->setText("");
    _ui.xmitterMagCurrentValue->setText("");
    _ui.xmitterHvpsCurrentValue->setText("");
    _ui.xmitterTemperatureValue->setText("");
}

void
KaGuiMainWindow::_enableXmitterUi() {
    std::ostringstream os;
    _ui.xmitterStatusLabel->setText("ka_xmitd is alive");
    _ui.xmitterControlAndStateFrame->setEnabled(true);
    _ui.xmitterValuesFrame->setEnabled(true);
    _xmitterFaultDetails.setEnabled(true);
}

void
KaGuiMainWindow::_updateXmitdStatus(const XmitdStatus & xmitdStatus) {
    _xmitdStatus = xmitdStatus;
    _update();
}
