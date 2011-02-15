/*
 * KaOscillator3.h
 *
 *  Created on: Dec 2, 2010
 *      Author: burghart
 */

#ifndef KAOSCILLATOR3_H_
#define KAOSCILLATOR3_H_

#include <stdint.h>

class Pmc730;

/**
 * @brief Class for programming Ka oscillator #3 (nominally 107.5 MHz)
 * 
 * Six DIO lines from the DRX computer's Acromag PMC730 multi-IO card are used 
 * to send three differential signals to the Ka downconversion card which relays 
 * them as single-ended signals to CLOCK, DATA, and LE lines of oscillator 3's 
 * Analog Devices ADF4001 PLL. Programming this chip controls the oscillator.
 * <table border>
 *   <tr>
 *     <td><b>PMC730 DIO line</b></td>
 *     <td><b>Signal</b></td>
 *   </tr>
 *   <tr>
 *     <td>8</td>
 *     <td>CLOCK+</td>
 *   </tr>
 *   <tr>
 *     <td>9</td>
 *     <td>CLOCK-</td>
 *   </tr>
 *   <tr>
 *     <td>10</td>
 *     <td>DATA+</td>
 *   </tr>
 *   <tr>
 *     <td>11</td>
 *     <td>DATA-</td>
 *   </tr>
 *   <tr>
 *     <td>14</td>
 *     <td>LE+</td>
 *   </tr>
 *   <tr>
 *     <td>15</td>
 *     <td>LE-</td>
 *   </tr>
 * </table>
 * 
 * @see Adf4001
 */
class KaOscillator3 {
public:
    /**
     * Constructor for a real oscillator 3, with communication via the
     * given PMC-730 card.
     * @param pmc730 Reference to the PMC730 card through which oscillator 3
     *      is connected. Set to SIM_PMC730 to simulate existence of a
     *      PMC-730 card.
     * @param testReadback If true, echoed bits are read back from the PLL
     *      chip to validate oscillator programming.
     */
    KaOscillator3(Pmc730 & pmc730, bool testReadback = true);
    virtual ~KaOscillator3();
    
    /**
     * Step value for output frequencies.
     */
    // Note that this frequency *must* divide evenly into OSC3_REF_FREQ!
    static const unsigned int OSC3_FREQ_STEP = 50000;           // 50 kHz
    /**
     * Default output frequency
     */
    static const unsigned int OSC3_DEFAULT_FREQ = 107500000;    // 107.5 MHz
    /**
     * Minimum output frequency
     */
    static const unsigned int OSC3_MIN_FREQ = 107000000;
    /**
     * Maximum output frequency
     */
    static const unsigned int OSC3_MAX_FREQ = 108000000;
    /**
     * "Simulated" Pmc730.
     */
    static Pmc730 & SIM_PMC730;
    
    /**
     * Set oscillator 3 frequency. The selected frequency must be a multiple
     * of OSC3_FREQ_STEP, and must be in the range [OSC3_MIN_FREQ, OSC3_MAX_FREQ]
     * @param freq the desired oscillator 3 frequency, in Hz
     */
    void setFrequency(unsigned int freq);
    
    /**
     * Set oscillator 3 frequency in units of its frequency step.
     * @param scaledFreq the desired oscillator 3 frequency, in units of the
     *     frequency step
     */
    void setScaledFreq(unsigned int scaledFreq) { setFrequency(scaledFreq * OSC3_FREQ_STEP); }
    
    /**
     * Get oscillator 3 frequency in Hz.
     */
    unsigned int getFrequency() const {
        return((OSC3_REF_FREQ / _rDivider) * _nDivider);
    }
    
    /**
     * Get oscillator 3 frequency in units of the frequency step.
     */
    unsigned int getScaledFreq() const {
        return(getFrequency() / OSC3_FREQ_STEP);
    }
    
    /**
     * Get oscillator 3 frequency step, in Hz.
     */
    static unsigned int getFreqStep() { return(OSC3_FREQ_STEP); }
    
    /**
     * Get oscillator 3 max frequency in units of the frequency step.
     */
    static unsigned int getScaledMaxFreq() {
        return(OSC3_MAX_FREQ / OSC3_FREQ_STEP);
    }
    
    /**
     * Get oscillator 3 min frequency in units of the frequency step.
     */
    static unsigned int getScaledMinFreq() {
        return(OSC3_MIN_FREQ / OSC3_FREQ_STEP);
    }
    
private:
    /**
     * Reference oscillator frequency, Hz
     */
    static const unsigned int OSC3_REF_FREQ = 10000000;         // 10 MHz
    
    /**
     * The PMC730 DIO lines we use to communicate with the ADF4001 chip.
     */
    typedef enum {
        DIO_MUXOUT   = 7,
        DIO_CLOCK    = 8,
        DIO_CLOCKINV = 9,
        DIO_DATA     = 10,
        DIO_DATAINV  = 11,
        DIO_LE       = 12,
        DIO_LEINV    = 13
    } DIOLine_t;
    
    /**
     * Set the selected bit in the source byte and return the result.
     * @param src the source byte
     * @param bitnum the number of the bit to set; 0 for least significant bit
     * @return a byte copied from src, but with the selected bit set
     */
    static uint8_t 
    _turnBitOn(uint8_t src, unsigned int bitnum) {
        uint8_t mask = (1 << bitnum);
        return(src | mask);
    }

    /**
     * Unset the selected bit in the source byte and return the result.
     * @param src the source byte
     * @param bitnum the number of the bit to unset; 0 for least significant bit
     * @return a byte copied from src, but with the selected bit unset
     */
    static uint8_t
    _turnBitOff(uint8_t src, unsigned int bitnum) {
        uint8_t mask = (1 << bitnum);
        return(src & ~mask);
    }

    /**
     * Send a differential signal over two selected PMC730 DIO lines. 
     * Line dioPosLine will get the positive signal, and line dioNegLine will 
     * get the inverted signal.
     * @param signalHigh if true, a high signal will be sent, otherwise low
     * @param dioPosLine the DIO line for the positive signal
     * @param dioNegLine the DIO line for the inverted signal
     */
    void _sendDifferentialSignal(bool signalHigh, DIOLine_t dioPosLine, 
            DIOLine_t dioNegLine);
    
    /**
     * Set the state of the ADF4001 clock line.
     * @param signalHigh if true, the clock line will be set high, otherwise low
     */
    void _setClock(bool signalHigh);
    
    /**
     * Set the state of the ADF4001 LE (latch enable) line.
     * @param signalHigh if true, the LE will be set high, otherwise low
     */
    void _setLE(bool signalHigh);
    
    /**
     * Set the state of the ADF4001 data line.
     * @param signalHigh if true, the data line will be set high, otherwise low
     */
    void _setData(bool signalHigh);
    
    /**
     * Send out a 24-bit latch command to the ADF4001 PLL chip
     * @param val the 24-bit value to send to the PLL (only the low order 24 
     *      bits of val are sent to the PLL).
     */
    void _adf4001Bitbang(uint32_t val);
    /**
     * Is our PMC-730 simulated? (I.e., is it a reference to NULL?)
     */
    bool _pmcSimulated() {
        return(&_pmc730 == 0);
    }
        
    Pmc730 & _pmc730;
    /**
     * If _testReadback is true, echoed bits from the PLL chip are verified
     * against previously sent commands.
     */
    bool _testReadback;
    /**
     * Oscillator 3 output frequency = (_nDivider / _rDivider) * OSC3_REF_FREQ
     */
    uint16_t _nDivider;
    uint16_t _rDivider;
    /**
     * Keep the last command bitbanged to the ADF4001.
     */
    uint32_t _lastCommandSent;
};

#endif /* KAOSCILLATOR3_H_ */
