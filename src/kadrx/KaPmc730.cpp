/*
 * KaPmc730.cpp
 *
 *  Created on: Dec 17, 2010
 *      Author: burghart
 */

#include "KaPmc730.h"

KaPmc730 * KaPmc730::_theKaPmc730 = 0;

KaPmc730::KaPmc730() : Pmc730(0) {
}

KaPmc730::~KaPmc730() {
}

KaPmc730 & 
KaPmc730::theKaPmc730() {
    if (! _theKaPmc730) {
        _theKaPmc730 = new KaPmc730();
        // For Ka, DIO lines 0-7 are used for input, 8-15 for output
        _theKaPmc730->setDioDirection0_7(Pmc730::DIO_INPUT);
        _theKaPmc730->setDioDirection8_15(Pmc730::DIO_OUTPUT);
    }
    return(*_theKaPmc730);
}
