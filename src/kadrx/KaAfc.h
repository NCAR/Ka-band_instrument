/*
 * KaAfc.h
 *
 *  Created on: Dec 13, 2010
 *      Author: burghart
 */

#ifndef KAAFC_H_
#define KAAFC_H_

class KaAfcPrivate;

/// Singleton object to handle Ka-band AFC, controlling three adjustable 
/// oscillators. It merely needs pulse-by-pulse estimates of G0 power and
/// frequency offset, and handles programming of three adjustable 
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

    /// Accept an incoming set of averaged transmit pulse information comprising
    /// relative g0 power, and calculated frequency offset. This information 
    /// will be used to adjust oscillator frequencies.
    /// If this AfcThread is currently in the process of a frequency adjustment,
    /// the given sample will be ignored. @see adjustmentInProgress()
    /// @param g0Mag relative g0 power magnitude, in range [0.0,1.0]
    /// @param freqOffset measured frequency offset, in Hz
    void newXmitSample(double g0Mag, double freqOffset);
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
