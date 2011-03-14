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
    
    /**
     * Get the current value in the PMC730 pulse counter.
     */
    static uint32_t getPulseCounter() { return(theKaPmc730()._getPulseCounter()); }

    /**
     * Set the transmitter trigger enable line (DIO channel 14) on or off.
     * @param bool true iff TX trigger should be enabled
     */
    static void setTxTriggerEnable(bool enable) {
        theKaPmc730().setDioLine(_KA_DOUT_TXENABLE, enable ? 1 : 0);
    }

    /**
     * Set the transmitter's serial port reset line.
     * @param bool if true, the transmitter's serial line reset will be enabled
     */
    static void setTxSerialReset(bool enable) {
        theKaPmc730().setDioLine(_KA_DOUT_TXSERIALRESET, enable ? 1 : 0);
    }
    
    /**
     * Set the state of oscillator3's ADF4001 PLL chip clock line.
     * (differential signal sent on DIO lines 8 and 9)
     * @param signalHigh if true, the clock line will be set high, otherwise low
     */
    static void setPllClock(bool signalHigh) {
        theKaPmc730()._sendDifferentialSignal(signalHigh, _KA_DOUT_CLK_P, _KA_DOUT_CLK_N);
    }

    /**
     * Set the state of oscillator3's ADF4001 PLL chip LE (latch enable) line.
     * (differential signal sent on DIO lines 12 and 13)
     * @param signalHigh if true, the LE will be set high, otherwise low
     */
    static void setPllLatchEnable(bool signalHigh) {
        theKaPmc730()._sendDifferentialSignal(signalHigh, _KA_DOUT_LE_P, _KA_DOUT_LE_N);
    }

    /**
     * Set the state of oscillator3's ADF4001 PLL chip data line.
     * (differential signal sent on DIO lines 10 and 11)
     * @param signalHigh if true, the data line will be set high, otherwise low
     */
    static void setPllData(bool signalHigh) {
        theKaPmc730()._sendDifferentialSignal(signalHigh, _KA_DOUT_DATA_P, _KA_DOUT_DATA_N);
    }
    
    /**
     * Get the state of oscillator3's ADF4001 PLL muxout line.
     * @return 0 if the line is high or zero if the line is low
     */
    static uint8_t getPllMuxout() {
        return(theKaPmc730().getDioLine(_KA_DIN_OSC3));
    }

    /**
     * Return true iff waveguide pressure between the Ka transmitter and the 
     * antenna is OK.
     * @return true iff waveguide pressure between the Ka transmitter and the
     * antenna is OK.
     */
    static bool wgPressureValid() {
        return(theKaPmc730().getDioLine(_KA_DIN_WGPRES_OK));
    }

    /**
     * Get the state of the 100 MHz oscillator alarm signal. When high (true),
     * the oscillator is operating properly.
     * @return true iff the 100 MHz oscillator is operating properly
     */
    static bool oscillator100MhzLocked() {
        return(theKaPmc730().getDioLine(_KA_DIN_100MHZ_ALM));
    }

    /**
     * Get the state of the GPS clock alarm signal (DIO channel 4)
     * @return true iff the GPS clock is reporting an alarm signal (generally 
     *      as a result of unlocked GPS)    
     */
    static bool gpsClockAlarm() {
        return(theKaPmc730().getDioLine(_KA_DIN_GPSCLOCK_ALM));
    }
    
private:
    KaPmc730();
    virtual ~KaPmc730();

    /**
     * Input DIO lines
     */
    typedef enum {
        _KA_DIN_UNUSED0 = 0,
        _KA_DIN_UNUSED1 = 1,
        _KA_DIN_COUNTER = 2,        // pulse counter input
        _KA_DIN_UNUSED3 = 3,
        _KA_DIN_GPSCLOCK_ALM = 4,   // GPS clock alarm
        _KA_DIN_WGPRES_OK = 5,      // waveguide pressure OK
        _KA_DIN_100MHZ_ALM = 6,     // 100 MHz oscillator alarm
        _KA_DIN_OSC3 = 7            // muxout line from oscillator 3 PLL chip
    } DinLine_t;
    
    /**
     * Output DIO lines
     */
    typedef enum {
        _KA_DOUT_CLK_P = 8,      // CLK+ to oscillator 3 PLL
        _KA_DOUT_CLK_N = 9,      // CLK- to oscillator 3 PLL
        _KA_DOUT_DATA_P = 10,    // DATA+ to oscillator 3 PLL
        _KA_DOUT_DATA_N = 11,    // DATA- to oscillator 3 PLL
        _KA_DOUT_LE_P = 12,      // LE+ to oscillator 3 PLL
        _KA_DOUT_LE_N = 13,      // LE- to oscillator 3 PLL
        _KA_DOUT_TXENABLE = 14,  // transmitter trigger enable
        _KA_DOUT_TXSERIALRESET = 15    // transmitter serial line reset
    } DoutLine_t;
    
    /**
     * Initialize the PMC730 for pulse counting, which uses DIO channel 2.
     */
    void _initPulseCounter();
    
    /**
     * Get the current value in the PMC730 pulse counter.
     */
    uint32_t _getPulseCounter();
    
    /**
     * Send a differential signal over two selected PMC730 DIO lines. 
     * Line dioPosLine will get the positive signal, and line dioNegLine will 
     * get the inverted signal.
     * @param signalHigh if true, a high signal will be sent, otherwise low
     * @param dioPosLine the output line for the positive signal
     * @param dioNegLine the output line for the inverted signal
     */
    void _sendDifferentialSignal(bool signalHigh, DoutLine_t dioPosLine, 
            DoutLine_t dioNegLine);
    
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
    
    static KaPmc730 * _theKaPmc730;
};

#endif /* KAPMC730_H_ */
