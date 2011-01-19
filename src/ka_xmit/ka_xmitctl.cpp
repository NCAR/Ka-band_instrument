/*
 * ka_xmitctl.cpp
 *
 *  Created on: Jan 6, 2011
 *      Author: burghart
 */
 
#include <cstdlib>
#include <QApplication>

#include <logx/Logging.h>

#include "KaXmitCtlMainWindow.h"

LOGGING("ka_xmitctl")

int
main(int argc, char *argv[]) {
    // Let logx get and strip out its arguments
    logx::ParseLogArgs(argc, argv);
    
    QApplication* app = new QApplication(argc, argv);
    
    if (argc != 3) {
        ELOG << "Usage: " << argv[0] << " <xmitd_host> <xmitd_port>";
        exit(1);
    }

    QMainWindow* mainWindow = new KaXmitCtlMainWindow(argv[1], atoi(argv[2]));
    mainWindow->show();
    
    return app->exec();
}
