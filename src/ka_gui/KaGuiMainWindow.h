/*
 *  Created on: May 3, 2018
 *      Author: Chris Burghart <burghart@ucar.edu>
 */
#ifndef KAGUIMAINWINDOW_H_
#define KAGUIMAINWINDOW_H_

#include <string>
#include <map>
#include <deque>
#include <ctime>

#include <QtCore/QTimer>
#include <QtGui/QMainWindow>
#include <QtGui/QPixmap>

#include <KadrxStatus.h>
#include <XmitClient.h>

#include "ui_KaGuiMainWindow.h"
#include "KadrxMonitorDetails.h"
#include "XmitterFaultDetails.h"
#include "KadrxStatusThread.h"
#include "XmitdStatusThread.h"

class KaGuiMainWindow : public QMainWindow {
    Q_OBJECT
public:
    KaGuiMainWindow(const XmitdStatusThread & xmitdStatusThread,
                    const KadrxStatusThread & kadrxStatusThread);
    virtual ~KaGuiMainWindow();

private slots:
    void on_kadrxMoreButton_clicked();
    void on_xmitterPowerButton_clicked();
    void on_xmitterStandbyButton_clicked();
    void on_xmitterOperateButton_clicked();
    void on_xmitterFaultDetailsButton_clicked();
    void resetXmitterFault();

    /// @brief Update latest ka_xmitd status
    /// @param xmitdStatus the new status
    void _updateXmitdStatus(XmitdStatus xmitdStatus);

    /// @brief Update latest kadrx status
    /// @param kadrxStatus the new status
    void _updateKadrxStatus(KadrxStatus kadrxStatus);

    /// @brief Slot called to set kadrx responsiveness
    /// @param responsive true iff kadrx is responsive
    void _setKadrxResponsiveness(bool responsive);

    /// @brief Slot called to set ka_xmitd responsiveness
    /// @param responsive true iff ka_xmitd is responsive
    void _setXmitdResponsiveness(bool responsive);

private:
    /// @brief Update contents of the GUI
    void _updateGui();

    /// @brief Update GUI content related to ka_xmitd
    void _updateXmitdBox();

    /// @brief Update GUI content related to kadrx
    void _updateKadrxBox();

    /// @brief Disable the transmitter portion of the GUI
    void _disableXmitterUi();

    /// @brief Enable the transmitter portion of the GUI
    void _enableXmitterUi();

    /// @brief Disable the transmitter GUI when no connection exists to
    /// ka_xmitd
    void _noXmitDaemon();

    /// @brief Disable the transmitter GUI when ka_xmitd is not talking to
    /// the transmitter
    void _noXmitterSerial();

    /// @brief Return a reference to the XmitClient instance from our
    /// XmitdStatusThread
    /// @return a reference to the XmitClient instance from our
    /// XmitdStatusThread
    XmitClient & _xmitdRpcClient() { return(_xmitdStatusThread.xmlRpcClient()); }

    /// @brief Return a string which surrounds the given text in a <font>
    /// block using the given color name
    /// @param text the text to be colored
    /// @param the name of the color to use
    static QString _ColorText(QString text, QString colorName);

    Ui::KaGuiMainWindow _ui;
    
    XmitterFaultDetails _xmitterFaultDetails;
    const XmitdStatusThread & _xmitdStatusThread;
    // Last XmitdStatus received
    XmitdStatus _xmitdStatus;
    /// Is ka_xmitd currently responsive?
    bool _xmitdResponsive;

    KadrxMonitorDetails _kadrxMonitorDetails;
    const KadrxStatusThread & _kadrxStatusThread;
    /// Last KadrxStatus received
    KadrxStatus _kadrxStatus;
    /// Is kadrx currently responsive?
    bool _kadrxResponsive;
    
    QPixmap _redLED;
    QPixmap _redLED_off;
    QPixmap _greenLED;
    QPixmap _greenLED_off;

};
#endif /*KAGUIMAINWINDOW_H_*/
