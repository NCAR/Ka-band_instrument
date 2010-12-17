/*
 * KaPmc730.cpp
 *
 *  Created on: Dec 17, 2010
 *      Author: burghart
 */

#include "KaPmc730.h"

Pmc730 * KaPmc730::_thePmc730 = 0;

Pmc730 & 
KaPmc730::thePmc730() {
    if (! _thePmc730) {
        _thePmc730 = new Pmc730(0);
        // For Ka, DIO lines 0-7 are used for input, 8-15 for output
        _thePmc730->setDioDirection0_7(Pmc730::DIO_INPUT);
        _thePmc730->setDioDirection8_15(Pmc730::DIO_OUTPUT);
    }
    return(*_thePmc730);
}