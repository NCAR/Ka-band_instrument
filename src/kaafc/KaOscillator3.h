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
 * them as single-ended signals to oscillator 3's ADF4001 PLL CLOCK, DATA, and 
 * LE input lines.
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
    KaOscillator3(Pmc730 & pmc730);
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
     * Set oscillator 3 frequency. The selected frequency must be a multiple
     * of OSC3_FREQ_STEP, and must be in the range [OSC3_MIN_FREQ, OSC3_MAX_FREQ]
     * @param freq the desired oscillator 3 frequency
     */
    void setFrequency(unsigned int freq);
    
private:
    /**
     * Reference oscillator frequency, Hz
     */
    static const unsigned int OSC3_REF_FREQ = 10000000;         // 10 MHz
    
    /**
     * The PMC730 DIO lines we use to communicate with the ADF4001 chip.
     */
    typedef enum {
        DIO_CLOCK    = 8,
        DIO_CLOCKINV = 9,
        DIO_DATA     = 10,
        DIO_DATAINV  = 11,
        DIO_LE       = 14,
        DIO_LEINV    = 15
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
        
    Pmc730 & _pmc730;
    /**
     * Oscillator 3 output frequency = (_nDivider / _rDivider) * OSC3_REF_FREQ
     */
    uint16_t _nDivider;
    uint16_t _rDivider;
};

#endif /* KAOSCILLATOR3_H_ */