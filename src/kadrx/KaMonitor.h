/*
 * KaMonitor.h
 *
 *  Created on: Dec 13, 2010
 *      Author: burghart
 */

#ifndef KAMONITOR_H_
#define KAMONITOR_H_

class KaMonitorPriv;

/// Singleton object to handle Ka monitoring.
class KaMonitor {
public:
    // Get a reference to the singleton instance of KaMonitor.
    static KaMonitor & theMonitor() {
        if (! _theMonitor) {
            _theMonitor = new KaMonitor();
        }
        return(*_theMonitor);
    }
protected:
    /// constructor for the singleton instance
    KaMonitor();
    
    /// destructor for the singleton instance
    ~KaMonitor();
private:
    /// We hide most everything in a private implementation class
    KaMonitorPriv *_privImpl;
    
    /// The singleton instance we expose
    static KaMonitor *_theMonitor;
};


#endif /* KAMONITOR_H_ */
