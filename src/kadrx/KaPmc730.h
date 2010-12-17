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
class KaPmc730 {
public:
    /// Return a reference to Ka's singleton Pmc730 instance.
    static Pmc730 & thePmc730();
protected:
    static Pmc730 * _thePmc730;
};

#endif /* KAPMC730_H_ */
