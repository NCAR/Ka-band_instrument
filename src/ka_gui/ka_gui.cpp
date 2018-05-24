/*
 * ka_gui.cpp
 *
 *  Created on: Jan 6, 2011
 *      Author: burghart
 */
 
#include <QApplication>
#include "KadrxStatusThread.h"
#include "KaGuiMainWindow.h"

#include <logx/Logging.h>


LOGGING("ka_gui")

int
main(int argc, char *argv[]) {
    // Let logx get and strip out its arguments
    logx::ParseLogArgs(argc, argv);

    QApplication* app = new QApplication(argc, argv);

    if (argc != 4) {
        ELOG << "Usage: " << argv[0] << " <host> <xmitd_port> <kadrx_port>";
        exit(1);
    }

    // Create and start the XmitdStatusThread
    XmitdStatusThread xmitdStatusThread(argv[1], atoi(argv[2]));
    xmitdStatusThread.start();

    // Create and start the KadrxStatusThread
    KadrxStatusThread kadrxStatusThread(argv[1], atoi(argv[3]));
    kadrxStatusThread.start();
    
    // Create our main window
    QMainWindow* mainWindow =
            new KaGuiMainWindow(xmitdStatusThread, kadrxStatusThread);
    mainWindow->show();

    return(app->exec());
}
