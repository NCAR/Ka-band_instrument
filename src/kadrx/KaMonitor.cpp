/*
 * KaMonitor.cpp
 *
 *  Created on: Oct 29, 2010
 *      Author: burghart
 */

#include "KaMonitor.h"
#include "KaPmc730.h"

#include <QThread>
#include <QTimer>
#include <QMutex>

#include <vector>

#include <logx/Logging.h>

LOGGING("KaMonitor")

// Pointer to our singleton instance
KaMonitor * KaMonitor::_theMonitor = 0;

/// KaMonitorPriv is the private implementation class for KaMonitor, subclassed 
/// from QThread. The object polls various temperatures and rx video powers 
/// on a regular (1 Hz) basis, and provides access to the status via XML-RPC
/// methods.
class KaMonitorPriv : public QThread {
public:
    KaMonitorPriv();
    
    ~KaMonitorPriv();
    
    void run();
    
private:
    /// Thread access mutex
    QMutex _mutex;
    /// Temperatures
    float _procEnclosureTemp;       // C
    float _procDrTemp;              // C
    float _txEnclosureTemp;         // C
    float _rxTopTemp;               // C
    float _rxBackTemp;              // C
    float _rxFrontTemp;             // C
    /// Video power monitors
    float _hTxPowerVideo;           // H tx pulse power, dBm
    float _vTxPowerVideo;           // V tx pulse power, dBm
    float _testTargetPowerVideo;    // test target power, dBm
};

KaMonitor::KaMonitor() {
    // Instantiate our private thread and start it.
    _privImpl = new KaMonitorPriv();
    _privImpl->start();
}

KaMonitor::~KaMonitor() {
    _privImpl->terminate();
    if (! _privImpl->wait(5000)) {
        ELOG << "KaMonitorPriv thread failed to stop in 5 seconds. Exiting anyway.";
    }
}

KaMonitorPriv::KaMonitorPriv() :
    QThread(),
    _mutex(QMutex::Recursive),
    _procEnclosureTemp(-999),
    _procDrTemp(-999),
    _txEnclosureTemp(-999),
    _rxTopTemp(-999),
    _rxBackTemp(-999),
    _rxFrontTemp(-999),
    _hTxPowerVideo(-999),
    _vTxPowerVideo(-999),
    _testTargetPowerVideo(-999) {
}

KaMonitorPriv::~KaMonitorPriv() {
}

void
KaMonitorPriv::run() {
    // Since we have no event loop, allow thread termination via the terminate()
    // method.
    setTerminationEnabled(true);
  
    while (true) {
        std::vector<float> analogData = KaPmc730::thePmc730().readAnalogChannels(0, 9);
        ILOG << "Analog 8: " << analogData[8] << ", 9: " << analogData[9];
        usleep(1000000); // 1 s
    }
}
