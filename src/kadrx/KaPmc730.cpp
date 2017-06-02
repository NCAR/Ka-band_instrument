/*
 * KaPmc730.cpp
 *
 *  Created on: Dec 17, 2010
 *      Author: burghart
 */

#include "KaPmc730.h"
#include <sys/time.h>
/*
 * Include low-level Acromag headers.
 */
extern "C" {
#  include <Acromag/pmccommon/pmcmulticommon.h>
#  include <Acromag/pmc730/pmc730.h>
}

#include <logx/Logging.h>

LOGGING("KaPmc730");

// Static to tell whether the singleton is created as a simulated PMC-730
bool KaPmc730::_DoSimulate = false;

// Our singleton instance
KaPmc730 * KaPmc730::_theKaPmc730 = 0;

KaPmc730::KaPmc730() : Pmc730(_DoSimulate ? -1 : 0) {
	// Get the time of day of instantiation (in seconds). This allows us to
    // return a reasonable pulse count if we're simulating.
	struct timeval startTv;
	gettimeofday(&startTv, NULL);
	_startTimeOfDay = startTv.tv_sec + 1.0e-6 * startTv.tv_usec;

    // For Ka, DIO lines 0-7 are used for input, 8-15 for output
    setDioDirection0_7(DIO_INPUT);
    setDioDirection8_15(DIO_OUTPUT);
    // Verify that our defined input DIO lines are all actually set for input
    if (getDioDirection(_KA_DIN_COUNTER) != DIO_INPUT ||
            getDioDirection(_KA_DIN_GPSCLOCK_ALM) != DIO_INPUT ||
            getDioDirection(_KA_DIN_WGPRES_OK) != DIO_INPUT ||
            getDioDirection(_KA_DIN_OSC3) != DIO_INPUT) {
        ELOG << __PRETTY_FUNCTION__ << ": Ka PMC-730 DIO lines " <<
                _KA_DIN_COUNTER << ", " << _KA_DIN_GPSCLOCK_ALM << ", " <<
                _KA_DIN_WGPRES_OK << ", and " << _KA_DIN_OSC3 <<
                " are not all set for input!";
        abort();
    }
    // Verify that our defined output DIO lines are all actually set for output
    if (getDioDirection(_KA_DOUT_CLK_P) != DIO_OUTPUT ||
            getDioDirection(_KA_DOUT_CLK_N) != DIO_OUTPUT ||
            getDioDirection(_KA_DOUT_DATA_P) != DIO_OUTPUT ||
            getDioDirection(_KA_DOUT_DATA_N) != DIO_OUTPUT ||
            getDioDirection(_KA_DOUT_LE_P) != DIO_OUTPUT ||
            getDioDirection(_KA_DOUT_LE_N) != DIO_OUTPUT ||
            getDioDirection(_KA_DOUT_TXENABLE) != DIO_OUTPUT) {
        ELOG << __PRETTY_FUNCTION__ << ": Ka PMC-730 DIO lines " <<
                _KA_DOUT_CLK_P << ", " << _KA_DOUT_CLK_N << ", " <<
                _KA_DOUT_DATA_P << ", " << _KA_DOUT_DATA_N << ", " <<
                _KA_DOUT_LE_P << ", " << _KA_DOUT_LE_N << ", and " <<
                _KA_DOUT_TXENABLE << " are not all set for output!";
        abort();
    }
    // Ka uses the PMC730 pulse counting function, which uses DIO channel 2
    _initPulseCounter();
}

KaPmc730::~KaPmc730() {
}

void
KaPmc730::doSimulate(bool simulate) {
	if (_theKaPmc730) {
		ELOG << __PRETTY_FUNCTION__ <<
			" has no effect after the KaPmc730 has been created!";
		return;
	}
	DLOG << "Singleton KaPmc730 will be created with simulate set to " <<
		(simulate ? "TRUE" : "FALSE");
	_DoSimulate = simulate;
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
	if (_simulate)
		return;

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
	// If simulating, just return a count of milliseconds since we were
	// instantiated.
	if (_simulate) {
		struct timeval tv;
		gettimeofday(&tv, NULL);
		double diff = (tv.tv_sec + 1.0e-6 * tv.tv_usec) - _startTimeOfDay;
		if (diff < 0.0)
			diff += 86400; // seconds in a day
		return(uint32_t(1000 * diff));
	}
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
KaPmc730::_sendDifferentialSignal(bool signalHigh, DoutLine_t dioPosLine,
        DoutLine_t dioNegLine) {
    // as written, this only works for DIO lines in the range 8-15!
    assert(dioPosLine >=8 && dioPosLine <= 15 &&
            dioNegLine >= 8 && dioNegLine <= 15);
    // start from the current state of lines 8-15
    uint8_t new8_15 = getDio8_15();
    // change the two lines we care about
    if (signalHigh) {
        new8_15 = _turnBitOn(new8_15, dioPosLine - 8);
        new8_15 = _turnBitOff(new8_15, dioNegLine - 8);
    } else {
        new8_15 = _turnBitOff(new8_15, dioPosLine - 8);
        new8_15 = _turnBitOn(new8_15, dioNegLine - 8);
    }
    // ship out the new state
    setDio8_15(new8_15);
}
