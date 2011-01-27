/*
 * KaOscillator3.cpp
 *
 *  Created on: Dec 2, 2010
 *      Author: burghart
 */

#include "KaOscillator3.h"
#include "Adf4001.h"
#include <Pmc730.h>
#include <cassert>
#include <iostream>
#include <logx/Logging.h>

LOGGING("KaOscillator3");

KaOscillator3::KaOscillator3(Pmc730 & pmc730) :
    _pmc730(pmc730),
    _nDivider(0),
    _rDivider(0),
    _lastCommandSent(0) {
    // Verify that the DIO lines we use to program the PLL are all set to output
    if (_pmc730.getDioDirection(DIO_CLOCK) != Pmc730::DIO_OUTPUT ||
            _pmc730.getDioDirection(DIO_CLOCKINV) != Pmc730::DIO_OUTPUT ||
            _pmc730.getDioDirection(DIO_DATA) != Pmc730::DIO_OUTPUT ||
            _pmc730.getDioDirection(DIO_DATAINV) != Pmc730::DIO_OUTPUT ||
            _pmc730.getDioDirection(DIO_LE) != Pmc730::DIO_OUTPUT ||
            _pmc730.getDioDirection(DIO_LEINV) != Pmc730::DIO_OUTPUT) {
        ELOG << __PRETTY_FUNCTION__ << ": PMC-730 DIO lines " << 
                DIO_CLOCK << ", " << DIO_CLOCKINV << ", " << 
                DIO_DATA << ", " << DIO_DATAINV << ", " <<
                DIO_LE << ", and " << DIO_LEINV << 
                " are not all set for output!";
        abort();
    }
    // Our frequency step must divide evenly into the reference frequency
    assert((OSC3_REF_FREQ % OSC3_FREQ_STEP) == 0);
    // Determine our fixed R divider value
    _rDivider = OSC3_REF_FREQ / OSC3_FREQ_STEP;
    assert(_rDivider <= 16383); // ADF4001 requirement
    // Program for our default frequency (this sets _nDivider)
    setFrequency(OSC3_DEFAULT_FREQ);
}

KaOscillator3::~KaOscillator3() {
}

void
KaOscillator3::_sendDifferentialSignal(bool signalHigh, DIOLine_t dioPosLine,
        DIOLine_t dioNegLine) {
    // as written, this only works for DIO lines in the range 8-15!
    assert(dioPosLine >=8 && dioPosLine <= 15 &&
            dioNegLine >= 8 && dioNegLine <= 15);
    // start from the current state of lines 8-15
    uint8_t new8_15 = _pmc730.getDio8_15();
    // change the two lines we care about
    if (signalHigh) {
        new8_15 = _turnBitOn(new8_15, dioPosLine - 8);
        new8_15 = _turnBitOff(new8_15, dioNegLine - 8);
    } else {
        new8_15 = _turnBitOff(new8_15, dioPosLine - 8);
        new8_15 = _turnBitOn(new8_15, dioNegLine - 8);
    }
    // ship out the new state
    _pmc730.setDio8_15(new8_15);
}

void
KaOscillator3::_setClock(bool signalHigh) {
    _sendDifferentialSignal(signalHigh, DIO_CLOCK, DIO_CLOCKINV);
}

void
KaOscillator3::_setLE(bool signalHigh) {
    _sendDifferentialSignal(signalHigh, DIO_LE, DIO_LEINV);
}

void
KaOscillator3::_setData(bool signalHigh) {
    _sendDifferentialSignal(signalHigh, DIO_DATA, DIO_DATAINV);
}

void
KaOscillator3::_adf4001Bitbang(uint32_t val) {
    // Assure that CLOCK and LE lines are low when we start.
    _setClock(0);
    _setLE(0);
    
    // Buffer to hold echoed bits from the ADF4001
    uint32_t echoedBits = 0;
    
    // Send over each bit, from most to least significant
    for (int bitnum = 23; bitnum >= 0; bitnum--) {
        // First read the muxout bit from the ADF4001 (on DIO channel 7) 
        // and append it to echoedBits. As long as muxout is MUX_SERIAL_DATA, 
        // the bit we read should be the bit we sent out 24 bits ago...
        uint8_t muxoutBit = (_pmc730.getDio0_7() >> 7) & 0x1;
        echoedBits = (echoedBits << 1) | muxoutBit;

        // Send the next bit on the DATA line
        unsigned int bit = (val >> bitnum) & 0x1;
        _setData(bit);
        // CLOCK high then low to clock in the bit
        _setClock(1);
        _setClock(0);
    }
    
    // Finally, set LE high to latch the whole thing
    _setLE(1);
    // ..and LE back to low to clean up
    _setLE(0);
    
    // Did we get a successful echo back of the last command we sent?
    DLOG << std::hex << "last command 0x" << _lastCommandSent << 
        ", echo 0x" << echoedBits << std::dec;
        
    if (_lastCommandSent && (_lastCommandSent != echoedBits)) {
        ELOG << __PRETTY_FUNCTION__ << std::hex << "Last command sent (0x" << 
            _lastCommandSent << ") != echoed bits (0x" << echoedBits << ")" <<
            std::dec;
        abort();
    }
    _lastCommandSent = val;
}

void
KaOscillator3::setFrequency(unsigned int freq) {
    // validate that the requested frequency meets the required criteria
    if ((freq % OSC3_FREQ_STEP) != 0) {
        ELOG << __PRETTY_FUNCTION__ <<  ": requested frequency " <<  freq << 
                " Hz is not a multiple of OSC3_FREQ_STEP " << OSC3_FREQ_STEP <<
                " Hz";
        abort();
    }
    if (freq > OSC3_MAX_FREQ) {
        ELOG << __PRETTY_FUNCTION__ << ": requested frequency " << freq <<
                " Hz is greater than OSC3_MAX_FREQ " << OSC3_MAX_FREQ << " Hz";
        abort();
    }
    if (freq < OSC3_MIN_FREQ) {
        ELOG << __PRETTY_FUNCTION__ << ": requested frequency " << freq <<
                " Hz is less than OSC3_MIN_FREQ " << OSC3_MIN_FREQ << " Hz";
        abort();
    }
    
    ILOG << "Setting O3 frequency to " << freq << " Hz, (N value " <<
        freq / OSC3_FREQ_STEP << ")";
    
    // Calculate our N divider value
    _nDivider = freq / OSC3_FREQ_STEP;
    assert(_nDivider <= 8191);  // ADF4001 requirement
    
    // Set the PLL frequency using the ADF4001 "Initialization Latch Method":
    // send initialization latch, then R latch, then N latch, 
    // See the Analog Devices ADF4001 Data Sheet for details.
    uint32_t latch = Adf4001::initializationLatch(
            Adf4001::POWERDOWN_NORMAL_OPERATION,
            Adf4001::CPCURRENT2_X8, Adf4001::CPCURRENT1_X8, 
            Adf4001::TCTIMEOUT_63, Adf4001::FASTLOCK_DISABLED,
            Adf4001::CPTHREESTATE_DISABLE, Adf4001::PPOLARITY_POSITIVE,
            Adf4001::MUX_SERIAL_DATA, Adf4001::COUNTER_RESET_DISABLED);
    _adf4001Bitbang(latch);
    
    latch = Adf4001::rLatch(Adf4001::LDP_3CYCLES, Adf4001::ABP_2_9NS, _rDivider);
    _adf4001Bitbang(latch);
    
    latch = Adf4001::nLatch(Adf4001::CPG_DISABLE_CURRENT2, _nDivider);
    _adf4001Bitbang(latch);
}
