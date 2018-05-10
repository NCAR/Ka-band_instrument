/*
 *  Created on: Jan 14, 2011
 *      Author: burghart
 */
#ifndef KAXMITCTLMAINWINDOW_H_
#define KAXMITCTLMAINWINDOW_H_

#include <string>
#include <map>
#include <deque>
#include <ctime>

#include <QMainWindow>
#include <QPixmap>
#include <QTimer>

#include <XmitClient.h>

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
    void _update();
private:
    // Disable the UI
    void _disableUi();
    // Enable the UI
    void _enableUi();
    // Append latest messages from ka_xmitd to our logging area
    void _appendXmitdLogMsgs();
    // Disable the UI when no connection exists to the ka_xmitd.
    void _noDaemon();
    // Disable the UI if the daemon is not talking to the transmitter
    void _noXmitter();
    // Log a message
    void _logMessage(std::string message);
    /**
     *  Return "-" if the count is zero, otherwise a text representation of 
     *  the count.
     *  @param count the count to be represented
     *  @return "-" if the count is zero, otherwise a text representation of 
     *      the count.
     */
    static QString _countLabel(int count);
    
    /**
     *  Return an empty string if the time is -1, otherwise a brief text 
     *  representation of the time.
     *  @param time the time_t time to be represented
     *  @return an empty string if the time is -1, otherwise a brief text 
     *  representation of the time.
     */
    static QString _faultTimeLabel(time_t time);

    Ui::KaXmitCtlMainWindow _ui;
    XmitClient _xmitClient;
    QTimer _updateTimer;
    QPixmap _redLED;
    QPixmap _greenLED;
    QPixmap _greenLED_off;
    // Last status read
    XmitdStatus _status;
    
    // next log index to get from ka_xmitd
    unsigned int _nextLogIndex;
    bool _noXmitd;
};
#endif /*KAXMITCTLMAINWINDOW_H_*/
