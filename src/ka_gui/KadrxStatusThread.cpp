// *=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*
// ** Copyright UCAR (c) 1990 - 2018
// ** University Corporation for Atmospheric Research (UCAR)
// ** National Center for Atmospheric Research (NCAR)
// ** Boulder, Colorado, USA
// ** BSD licence applies - redistribution and use in source and binary
// ** forms, with or without modification, are permitted provided that
// ** the following conditions are met:
// ** 1) If the software is modified to produce derivative works,
// ** such modified software should be clearly marked, so as not
// ** to confuse it with the version available from UCAR.
// ** 2) Redistributions of source code must retain the above copyright
// ** notice, this list of conditions and the following disclaimer.
// ** 3) Redistributions in binary form must reproduce the above copyright
// ** notice, this list of conditions and the following disclaimer in the
// ** documentation and/or other materials provided with the distribution.
// ** 4) Neither the name of UCAR nor the names of its contributors,
// ** if any, may be used to endorse or promote products derived from
// ** this software without specific prior written permission.
// ** DISCLAIMER: THIS SOFTWARE IS PROVIDED "AS IS" AND WITHOUT ANY EXPRESS
// ** OR IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
// ** WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
// *=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*

// KadrxStatusThread.cpp
//
//  Created on: May 10, 2018
//      Author: Chris Burghart <burghart@ucar.edu>

#include "KadrxStatusThread.h"
#include <logx/Logging.h>
#include <QtCore/QMetaType>
#include <QtCore/QTimer>

LOGGING("KadrxStatusThread")

KadrxStatusThread::KadrxStatusThread(QString kadrxHost, int kadrxPort) :
    _responsive(false),
    _kadrxHost(kadrxHost),
    _kadrxPort(kadrxPort),
    _client(0) {
    // We need to register KadrxStatus as a metatype, since we'll be passing it
    // as an argument in a signal.
    qRegisterMetaType<KadrxStatus>("KadrxStatus");
    // Set thread affinity to self, so that signals connected to our slot(s)
    // will execute the slots in this thread, and not our parent's.
    moveToThread(this);
}

KadrxStatusThread::~KadrxStatusThread() {
    DLOG << "In ~KadrxStatusThread()";
    quit();
    wait();
}

void
KadrxStatusThread::run() {
    _client = new KadrxRpcClient(_kadrxHost.toStdString(), _kadrxPort);

    // Set up a 1 second timer to call _getStatus()
    QTimer timer;
    connect(&timer, SIGNAL(timeout()), this, SLOT(_getStatus()));
    timer.start(1000);
    // Start the event loop
    exec();
    return;
}

void
KadrxStatusThread::_getStatus() {
    KadrxStatus status;
    if (_client->getStatus(status)) {
        if (! _responsive) {
            _responsive = true;
            emit serverResponsive(true);
        }
        emit newStatus(status);
    } else {
        if (_responsive) {
            _responsive = false;
            emit serverResponsive(false);
        }
    }
}
