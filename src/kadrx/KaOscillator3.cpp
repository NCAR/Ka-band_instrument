/*
 * KaOscillator3.cpp
 *
 *  Created on: Dec 2, 2010
 *      Author: burghart
 */

#include "KaOscillator3.h"
#include "Adf4001.h"
#include "KaPmc730.h"
#include <cassert>
#include <iostream>
#include <logx/Logging.h>

LOGGING("KaOscillator3");

const unsigned int KaOscillator3::OSC3_FREQ_STEP;
const unsigned int KaOscillator3::OSC3_DEFAULT_FREQ;
const unsigned int KaOscillator3::OSC3_MIN_FREQ;
const unsigned int KaOscillator3::OSC3_MAX_FREQ;


KaOscillator3::KaOscillator3(bool testReadback) :
    _kaPmc730(KaPmc730::theKaPmc730()),
    _testReadback(testReadback && ! _kaPmc730.simulating()),
    _nDivider(0),
    _rDivider(0),
    _lastCommandSent(0) {
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
KaOscillator3::_adf4001Bitbang(uint32_t val) {
    if (_pmcSimulated())
        return;
    // Assure that CLOCK and LE lines are low when we start.
    _kaPmc730.setPllClock(0);
    _kaPmc730.setPllLatchEnable(0);
    
    // Buffer to hold echoed bits from the ADF4001
    uint32_t echoedBits = 0;
    
    // Send over each bit, from most to least significant
    for (int bitnum = 23; bitnum >= 0; bitnum--) {
        // First read the muxout bit from the ADF4001 (on DIO channel 7) 
        // and append it to echoedBits. As long as muxout is MUX_SERIAL_DATA, 
        // the bit we read should be the bit we sent out 24 bits ago...
        uint8_t muxoutBit = _kaPmc730.getPllMuxout();
        echoedBits = (echoedBits << 1) | muxoutBit;

        // Send the next bit on the DATA line
        unsigned int bit = (val >> bitnum) & 0x1;
        _kaPmc730.setPllData(bit);
        // CLOCK high then low to clock in the bit
        _kaPmc730.setPllClock(1);
        _kaPmc730.setPllClock(0);
    }
    
    // Finally, set LE high to latch the whole thing
    _kaPmc730.setPllLatchEnable(1);
    // ..and LE back to low to clean up
    _kaPmc730.setPllLatchEnable(0);
    
    // Did we get a successful echo back of the last command we sent?
    DLOG << std::hex << "last command 0x" << _lastCommandSent << 
        ", echo 0x" << echoedBits << std::dec;
        
    if (_testReadback && _lastCommandSent && (_lastCommandSent != echoedBits)) {
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
