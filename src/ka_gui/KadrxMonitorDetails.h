/*
 * KadrxMonitorDetails.h
 *
 *  Created on: May 7, 2018
 *      Author: Chris Burghart <burghart@ucar.edu>
 */

#ifndef KADRXMONITORDETAILS_H_
#define KADRXMONITORDETAILS_H_

#include <QtGui/QDialog>
#include <QtGui/QPixmap>
#include <KadrxStatus.h>
#include "ui_KadrxMonitorDetails.h"

class KadrxMonitorDetails : public QDialog {
    Q_OBJECT
public:
    KadrxMonitorDetails(QWidget * parent);
    virtual ~KadrxMonitorDetails();

    /// @brief Update the widget with information from the given KadrxStatus
    /// @param status current KadrxStatus to be used in populating the widget.
    void update(const KadrxStatus & status);

private:
    Ui::KadrxMonitorDetails _ui;
    QPixmap _redLED;
    QPixmap _redLED_off;
    QPixmap _greenLED;
    QPixmap _greenLED_off;
};

#endif /* KA_GUI_KADRXMONITORDETAILS_H_ */
