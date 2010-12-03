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
private:
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
    
    Pmc730 & _pmc730;
};

#endif /* KAOSCILLATOR3_H_ */
