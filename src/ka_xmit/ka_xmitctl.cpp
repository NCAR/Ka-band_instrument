/*
 * ka_xmitctl.cpp
 *
 *  Created on: Jan 6, 2011
 *      Author: burghart
 */

#include <logx/Logging.h>

#include <QApplication>

#include "KaXmitCtlMainWindow.h"

LOGGING("ka_xmitctl")

int
main(int argc, char *argv[]) {
    // Let logx get and strip out its arguments
    logx::ParseLogArgs(argc, argv);
    
    QApplication* app = new QApplication(argc, argv);

    QMainWindow* mainWindow = new KaXmitCtlMainWindow("localhost", 8080);
    mainWindow->show();
    
    return app->exec();
}
