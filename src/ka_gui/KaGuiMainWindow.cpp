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


KaGuiMainWindow::KaGuiMainWindow(const XmitdStatusThread & xmitdStatusThread,
                                 const KadrxStatusThread & kadrxStatusThread) :
    QMainWindow(),
    _ui(),
    _xmitterFaultDetails(this),
    _xmitdStatusThread(xmitdStatusThread),
    _xmitdStatus(),
    _xmitdResponsive(false),
    _kadrxMonitorDetails(this),
    _kadrxStatusThread(kadrxStatusThread),
    _kadrxStatus(),
    _kadrxResponsive(false),
    _redLED(":/redLED.png"),
    _redLED_off(":/redLED_off.png"),
    _greenLED(":/greenLED.png"),
    _greenLED_off(":/greenLED_off.png")
{
    // Initialize the UI object
    _ui.setupUi(this);

    // Connect signals from our XmitdStatusThread and start the thread
    connect(&_xmitdStatusThread, SIGNAL(newStatus(XmitdStatus)),
            this, SLOT(_updateXmitdStatus(XmitdStatus)));
    connect(&_xmitdStatusThread, SIGNAL(serverResponsive(bool)),
            this, SLOT(_setXmitdResponsiveness(bool)));

    connect(&_xmitterFaultDetails, SIGNAL(faultResetClicked()),
            this, SLOT(resetXmitterFault()));

    // Connect signals from the KadrxStatusThread
    QObject::connect(&kadrxStatusThread, SIGNAL(newStatus(KadrxStatus)),
                     this, SLOT(_updateKadrxStatus(KadrxStatus)));
    QObject::connect(&kadrxStatusThread, SIGNAL(serverResponsive(bool)),
                     this, SLOT(_setKadrxResponsiveness(bool)));

    // Populate the GUI
    _updateGui();
}

KaGuiMainWindow::~KaGuiMainWindow() {
}

void
KaGuiMainWindow::on_kadrxMoreButton_clicked() {
    _kadrxMonitorDetails.show();
}

void
KaGuiMainWindow::on_xmitterPowerButton_clicked() {
    if (_xmitdStatus.unitOn()) {
        _xmitdRpcClient().powerOff();
    } else {
        _xmitdRpcClient().powerOn();
    }
}

void
KaGuiMainWindow::resetXmitterFault() {
    _xmitdRpcClient().faultReset();
}

void
KaGuiMainWindow::on_xmitterStandbyButton_clicked() {
    _xmitdRpcClient().standby();
}

void
KaGuiMainWindow::on_xmitterOperateButton_clicked() {
    _xmitdRpcClient().operate();
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
KaGuiMainWindow::_updateGui() {
    _updateXmitdBox();
    _updateKadrxBox();
}

void
KaGuiMainWindow::_updateXmitdBox() {
    // Empty the display and disable controls if ka_xmitd is not alive or if
    // ka_xmitd has no connection to the transmitter.
    if (! _xmitdResponsive) {
        // Log lack of xmitd connection in the status label
        std::ostringstream os;
        os << "No ka_xmitd @ " <<
              _xmitdRpcClient().getXmitdHost() << ":" <<
              _xmitdRpcClient().getXmitdPort();
        _ui.xmitterStatusLabel->setText(_ColorText(os.str().c_str(), "darkred"));
        _disableXmitterUi();
        return;
    } else if (! _xmitdStatus.serialConnected()) {
        std::ostringstream os;
        os << "No serial connection from ka_xmitd to xmitter!";
        _ui.xmitterStatusLabel->setText(_ColorText(os.str().c_str(), "darkred"));
        _disableXmitterUi();  
        return;
    }

    // If we get here, we have real status from ka_xmitd
    _ui.xmitterStatusLabel->setText("ka_xmitd is alive");
    _ui.xmitterControlAndStateFrame->setEnabled(_xmitdResponsive);
    _ui.xmitterValuesFrame->setEnabled(_xmitdResponsive);
    _xmitterFaultDetails.setEnabled(_xmitdResponsive);

    // update boolean status values
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
KaGuiMainWindow::_updateKadrxBox() {
    // Enable/disable the main blocks based on kadrx responsiveness
    _ui.kadrxXmitDisableBox->setEnabled(_kadrxResponsive);
    _ui.kadrxMonitorBox->setEnabled(_kadrxResponsive);
    _kadrxMonitorDetails.setEnabled(_kadrxResponsive);

    // Empty the display and disable controls if kadrx is not responsive
    if (! _kadrxResponsive) {
        // Log lack of kadrx connection in the status label
        std::ostringstream os;
        os << "No kadrx @ " << _kadrxStatusThread.kadrxHost().toStdString() <<
              ":" << _kadrxStatusThread.kadrxPort();
        _ui.kadrxStatusLabel->setText(_ColorText(os.str().c_str(), "darkred"));
        return;
    }

    // If we get here, we have real status from kadrx
    _ui.kadrxStatusLabel->setText("kadrx is alive");
    
    // transmit disable status
    NoXmitBitmap noXmitBits = _kadrxStatus.noXmitBitmap();
    _ui.disabledByN2PresIcon->
        setPixmap(noXmitBits.bitIsSet(NoXmitBitmap::N2_PRESSURE_LOW) ?
                  _redLED : _redLED_off);
    _ui.disabledForBlankingIcon->
        setPixmap(noXmitBits.bitIsSet(NoXmitBitmap::IN_BLANKING_SECTOR) ?
                  _redLED : _redLED_off);
    _ui.disabledViaXmlrpcIcon->
        setPixmap(noXmitBits.bitIsSet(NoXmitBitmap::XMLRPC_REQUEST) ?
                  _redLED : _redLED_off);
    _ui.disabledByHupIcon->
        setPixmap(noXmitBits.bitIsSet(NoXmitBitmap::HUP_SIGNAL_RECEIVED) ?
                  _redLED : _redLED_off);

    // boolean monitor values
    _ui.unlockedTimeserverIcon->
        setPixmap(_kadrxStatus.gpsTimeServerGood() ? _redLED_off : _redLED);
    _ui.unlocked100MHzIcon->
        setPixmap(_kadrxStatus.locked100MHz() ? _redLED_off : _redLED);

    // other details
    _kadrxMonitorDetails.update(_kadrxStatus);
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
KaGuiMainWindow::_updateKadrxStatus(KadrxStatus kadrxStatus) {
    _setKadrxResponsiveness(true);
    _kadrxStatus = kadrxStatus;
    _updateGui();
}

void
KaGuiMainWindow::_updateXmitdStatus(XmitdStatus xmitdStatus) {
    _setXmitdResponsiveness(true);
    _xmitdStatus = xmitdStatus;
    _updateGui();
}

void
KaGuiMainWindow::_setKadrxResponsiveness(bool responsive) {
    // If no change, just return
    if (responsive == _kadrxResponsive) {
        return;
    }

    if (responsive) {
        ILOG << "kadrx is now responding";
    } else {
        _kadrxStatus = KadrxStatus();
        WLOG << "kadrx is no longer responding";
    }
    _kadrxResponsive = responsive;
    _updateGui();
}

void
KaGuiMainWindow::_setXmitdResponsiveness(bool responsive) {
    // If no change, just return
    if (responsive == _xmitdResponsive) {
        return;
    }

    if (responsive) {
        ILOG << "ka_xmitd is now responding";
    } else {
        WLOG << "ka_xmitd is no longer responding";
    }
    _xmitdResponsive = responsive;
    _updateGui();
}
