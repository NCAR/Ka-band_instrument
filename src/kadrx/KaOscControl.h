/*
 * KaOscControl.h
 *
 *  Created on: Dec 13, 2010
 *      Author: burghart
 */

#ifndef KAOSCCONTROL_H_
#define KAOSCCONTROL_H_

#include <sys/types.h>

#include "KaDrxConfig.h"

class KaOscControlPriv;

/// Singleton object to handle Ka-band AFC, controlling three adjustable 
/// oscillators, historically labeled 0, 1, and 3. It merely needs per-pulse 
/// estimates of G0 power and frequency offset, and handles programming of the
/// oscillators based on those values.
class KaOscControl {
public:
	/// Instantiate the singleton
    /// @param config the KaDrxConfig in use, which will provide AFC parameters
    /// @param maxDataLatency maximum data latency time expected for burst data,
    ///     in seconds
	static void createTheControl(const KaDrxConfig & config, double maxDataLatency);
	
    /// Get a reference to the singleton instance of KaOscControl. It must have
	/// first been created by a call to createTheControl().
    static KaOscControl & theControl();
    
    /// Accept an incoming set of averaged transmit pulse information comprising
    /// g0 power, and calculated frequency offset. This information will be used
    /// to adjust oscillator frequencies. If this KaOscControl is currently in the 
    /// process of a frequency adjustment, the given sample will be ignored.
    /// @param g0Power g0 power, in W
    /// @param freqOffset measured frequency offset, in Hz
    /// @param pulseSeqNum pulse number, counted since transmitter startup
    void newXmitSample(double g0Power, double freqOffset, int64_t pulseSeqNum);
protected:
    /// constructor
    /// @param config the KaDrxConfig in use, which will provide AFC parameters
    /// @param maxDataLatency maximum data latency time expected for burst data,
    ///     in seconds
    KaOscControl(const KaDrxConfig & config, double maxDataLatency);
    
    /// destructor for the singleton instance
    ~KaOscControl();
private:
    /// We hide most everything in a private implementation class
    KaOscControlPriv *_privImpl;
    
    /// The singleton instance we expose
    static KaOscControl *_theControl;
};


#endif /* KAOSCCONTROL_H_ */
