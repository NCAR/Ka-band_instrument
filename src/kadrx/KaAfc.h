/*
 * KaAfc.h
 *
 *  Created on: Dec 13, 2010
 *      Author: burghart
 */

#ifndef KAAFC_H_
#define KAAFC_H_

#include <sys/types.h>

class KaAfcPrivate;

/// Singleton object to handle Ka-band AFC, controlling three adjustable 
/// oscillators, historically labeled 0, 1, and 3. It merely needs per-pulse 
/// estimates of G0 power and frequency offset, and handles programming of the
/// oscillators based on those values.
class KaAfc {
public:
    // Get a reference to the singleton instance of KaAfc.
    static KaAfc & theAfc() {
        if (! _theAfc) {
            _theAfc = new KaAfc();
        }
        return(*_theAfc);
    }
    
    /// Set the G0 threshold power for reliable calculated frequencies, in dBm
    /// @param thresh G0 threshold power, in dBm
    static void setG0ThresholdDbm(double thresh);
    
    /// Set the AFC fine step in Hz. 
    /// @param step AFC fine step in Hz
    static void setFineStep(unsigned int step);

    /// Set the AFC coarse step in Hz. 
    /// @param step AFC coarse step in Hz
    static void setCoarseStep(unsigned int step);
    
    /// Set the maximum data latency for the burst data, in seconds. After
    /// an oscillator adjustment is applied, the processing thread sleeps for
    /// this amount of time so that the next data allowed in will be from after
    /// the oscillators are at their new frequencies.
    static void setMaxDataLatency(double maxDataLatency);

    /// Accept an incoming set of averaged transmit pulse information comprising
    /// g0 power, and calculated frequency offset. This information will be used
    /// to adjust oscillator frequencies. If this AfcThread is currently in the 
    /// process of a frequency adjustment, the given sample will be ignored.
    /// @param g0Power g0 power, in W
    /// @param freqOffset measured frequency offset, in Hz
    /// @param pulseSeqNum pulse number, counted since transmitter startup
    void newXmitSample(double g0Power, double freqOffset, int64_t pulseSeqNum);
protected:
    /// constructor for the singleton instance
    KaAfc();
    
    /// destructor for the singleton instance
    ~KaAfc();
private:
    /// We hide most everything in a private implementation class
    KaAfcPrivate *_afcPrivate;
    
    /// The singleton instance we expose
    static KaAfc *_theAfc;
};


#endif /* KAAFC_H_ */
