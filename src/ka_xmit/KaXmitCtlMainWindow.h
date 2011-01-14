/*
 *  Created on: Jan 14, 2011
 *      Author: burghart
 */
#ifndef KAXMITCTLMAINWINDOW_H_
#define KAXMITCTLMAINWINDOW_H_

#include <QMainWindow>
#include "ui_KaXmitCtlMainWindow.h"

class KaXmitCtlMainWindow : public QMainWindow {
public:
    KaXmitCtlMainWindow();
    ~KaXmitCtlMainWindow();
private:
    Ui::KaXmitCtlMainWindow _ui;
};
#endif /*KAXMITCTLMAINWINDOW_H_*/
