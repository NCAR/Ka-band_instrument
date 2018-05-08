/*
 * XmitterFaultDetails.h
 *
 *  Created on: May 7, 2018
 *      Author: Chris Burghart <burghart@ucar.edu>
 */

#ifndef KA_GUI_XMITFAULTDETAILS_H_
#define KA_GUI_XMITFAULTDETAILS_H_

#include <QtGui/QDialog>
#include <QtGui/QPixmap>
#include <XmitClient.h>
#include "ui_XmitterFaultDetails.h"

class XmitterFaultDetails : public QDialog {
public:
    XmitterFaultDetails(QWidget * parent);
    virtual ~XmitterFaultDetails();

    /// @brief Update the widget with information from the given XmitStatus
    /// @param status current XmitStatus to be used in populating the widget.
    void update(const XmitClient::XmitStatus & status);

    /// @brief Clear the widget
    void clearWidget();
private:
    /// @brief Return a fault count label string: "-" for no faults or an
    /// integer label with the fault count.
    /// @param count the fault count to be represented
    /// @return a fault count label string: "-" for no faults or an
    /// integer label with the fault count.
    QString _countLabel(int count);

    /// Return an empty string if the time is -1, otherwise a brief text 
    /// representation of the time.
    /// @param time the time_t time to be represented
    /// @return an empty string if the time is -1, otherwise a brief text 
    /// representation of the time.
    static QString _faultTimeLabel(time_t time);

    Ui::XmitterFaultDetails _ui;
    QPixmap _redLED;
    QPixmap _redLED_off;
    QPixmap _greenLED;
    QPixmap _greenLED_off;
};

#endif /* KA_GUI_XMITFAULTDETAILS_H_ */
