/*
 * KaPmc730.cpp
 *
 *  Created on: Dec 17, 2010
 *      Author: burghart
 */

#include "KaPmc730.h"
/*
 * Include Acromag headers.
 */
extern "C" {
#  include "pmccommon/pmcmulticommon.h"
#  include "pmc730/pmc730.h"
}

#include <logx/Logging.h>

LOGGING("KaPmc730");

// Our singleton instance
KaPmc730 * KaPmc730::_theKaPmc730 = 0;

KaPmc730::KaPmc730() : Pmc730(0) {
    // For Ka, DIO lines 0-7 are used for input, 8-15 for output
    setDioDirection0_7(Pmc730::DIO_INPUT);
    setDioDirection8_15(Pmc730::DIO_OUTPUT);
    // Ka uses the PMC730 pulse counting function, which uses DIO channel 2
    _initPulseCounter();
}

KaPmc730::~KaPmc730() {
}

KaPmc730 & 
KaPmc730::theKaPmc730() {
    if (! _theKaPmc730) {
        _theKaPmc730 = new KaPmc730();
    }
    return(*_theKaPmc730);
}

void
KaPmc730::_initPulseCounter() {
    // Initialize the PMC730 counter/timer for:
    // - pulse counting
    // - input polarity high
    // - software trigger to start counting
    // - disable counter interrupts
    
    // set up as an event (pulse) counter
    if (SetMode(&_card, InEvent) != Success) {
        ELOG << __PRETTY_FUNCTION__ << ": setting PMC730 to count pulses";
        abort();
    }
    // set input polarity to high
    if (SetInputPolarity(&_card, InPolHi) != Success) {
        ELOG << __PRETTY_FUNCTION__ << ": setting PMC730 input pulse polarity";
        abort();
    }
    // Use internal (software) trigger to start the timer
    if (SetTriggerSource(&_card, InTrig) != Success) {
        ELOG << __PRETTY_FUNCTION__ << ": setting trigger source";
        abort();
    }
    // disable counter/timer interrupts
    if (SetInterruptEnable(&_card, IntDisable) != Success) {
        ELOG << __PRETTY_FUNCTION__ << ": disabling PMC730 counter/timer interrupts";
        abort();
    }
    // Set counter constant 0 (maximum count value) to 2^32-1, so we can count 
    // up to the full 32 bits. This must be non-zero, or you'll never see any
    // pulses counted!
    if (SetCounterConstant(&_card, 0, 0xFFFFFFFFul) != Success) {
        ELOG << __PRETTY_FUNCTION__ << ": setting counter constant 0";
        abort();
    }
    // Now enable this configuration and then start the counter.
    ConfigureCounterTimer(&_card);
    // Send the software trigger to start the counter.
    StartCounter(&_card);    
}

uint32_t
KaPmc730::_getPulseCounter() {
    // Read the current count value from the counter readback register.
    int32_t signedCount = input_long(_card.nHandle, 
        (long*)&_card.brd_ptr->CounterReadBack);
    // We get a signed 32-bit value from input_long, but the counter readback
    // register is actually unsigned. Reinterpret the result to get the full
    // (unsigned) resolution.
    uint32_t* countPtr = reinterpret_cast<uint32_t*>(&signedCount);
    return(*countPtr);
}

void
KaPmc730::_setTxTriggerEnable(bool enable) {
    // Set the transmitter trigger enable signal, which is on DIO line 14
    setDioLine(14, enable ? 1 : 0);
}
