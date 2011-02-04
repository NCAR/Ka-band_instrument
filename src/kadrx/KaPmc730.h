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
private:
    KaPmc730();
    virtual ~KaPmc730();
    
    static KaPmc730 * _theKaPmc730;
};

#endif /* KAPMC730_H_ */
