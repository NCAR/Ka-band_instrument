/*
 * KaPmc730.h
 *
 *  Created on: Dec 17, 2010
 *      Author: burghart
 */

#ifndef KAPMC730_H_
#define KAPMC730_H_

#include "Pmc730.h"

/// Control a singleton instance of Pmc730 for the one PMC730 card on the 
/// Ka-band DRX machine. At some point, this can actually subclass Pmc730 and 
/// gain specialized methods related to the specific connections to the PMC730 
/// on the Ka-band radar (e.g., PMC730 DIO line 8 is connected to the PLL CLOCK+ 
/// input on the downconverter board).
class KaPmc730 : public Pmc730 {
public:
    /// Return a reference to Ka's singleton Pmc730 instance.
    static KaPmc730 & theKaPmc730();
    
    /**
     * Get the current value in the PMC730 pulse counter.
     */
    static uint32_t getPulseCounter() { return(theKaPmc730()._getPulseCounter()); }
    /**
     * Set the transmitter trigger enable line (DIO channel 14) on or off.
     * @param bool true iff TX trigger should be enabled
     */
    static void setTxTriggerEnable(bool enable) {
        theKaPmc730()._setTxTriggerEnable(enable);
    }
private:
    KaPmc730();
    virtual ~KaPmc730();

    /**
     * Initialize the PMC730 for pulse counting, which uses DIO channel 2.
     */
    void _initPulseCounter();
    /**
     * Get the current value in the PMC730 pulse counter.
     */
    uint32_t _getPulseCounter();
    /**
     * Set the transmitter trigger enable line (DIO channel 14) on or off.
     * @param bool true iff TX trigger should be enabled
     */
    void _setTxTriggerEnable(bool enable);
    
    static KaPmc730 * _theKaPmc730;
};

#endif /* KAPMC730_H_ */
