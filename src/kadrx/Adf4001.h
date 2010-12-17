/*
 * Adf4001.h
 *
 *  Created on: Dec 1, 2010
 *      Author: burghart
 */

#ifndef ADF4001_H_
#define ADF4001_H_

#include <stdint.h>

/**
 * @brief This class provides functions and types for use in controlling an 
 * Analog Devices ADF4001 PLL chip.
 * @see http://www.analog.com/static/imported-files/data_sheets/ADF4001.pdf
 */
class Adf4001 {
public:
    /**
     * Counter reset selections. This type is used to set bit 2 of the
     * Function (or Initialization) latch.
     */
    typedef enum {
        COUNTER_RESET_DISABLED = 0x0 << 2,
        COUNTER_RESET_ENABLED  = 0x1 << 2,
    } CounterReset_t;

    /**
     * Power-down selection. This type is used to set bits 3 and 21 of the 
     * Function (or Initialization) latch.
     */
    typedef enum {
        POWERDOWN_NORMAL_OPERATION = 0x0 << 21 | 0x0 << 3,
        POWERDOWN_ASYNCHRONOUS     = 0x0 << 21 | 0x1 << 3,
        POWERDOWN_SYNCHRONOUS      = 0x1 << 21 | 0x1 << 3
    } Powerdown_t;

    /**
     * Muxout selection. This type is used to set bits 4-6 of the Function (or 
     * Initialization) latch.
     */
    typedef enum {
        MUX_THREESTATE   = 0x0 << 4,
        MUX_DLOCK_DETECT = 0x1 << 4,
        MUX_NDIVIDER     = 0x2 << 4,
        MUX_AVDD         = 0x3 << 4,
        MUX_RDIVIDER     = 0x4 << 4,
        MUX_NLOCK_DETECT = 0x5 << 4,
        MUX_SERIAL_DATA  = 0x6 << 4,
        MUX_DGND         = 0x7 << 4
    } Muxout_t;

    /**
     * Phase detector polarity selection. This type is used to set bit 7 of the
     * Function (or Initialization) latch.
     */
    typedef enum {
        PPOLARITY_NEGATIVE = 0x0 << 7,
        PPOLARITY_POSITIVE = 0x1 << 7
    } PPolarity_t;
    
    /**
     * Charge pump output three-state enable/disable. This type is used to set
     * bit 8 of the Function (or Initialization) latch.
     */
    typedef enum {
        CPTHREESTATE_DISABLE = 0x0 << 8,
        CPTHREESTATE_ENABLE  = 0x1 << 8
    } CPThreestate_t;
    
    /**
     * Fastlock mode selection. This type is used to set bits 9-10 of the 
     * Function (or Initialization) latch.
     */
    typedef enum { 
        FASTLOCK_DISABLED = 0x0 << 9, 
        FASTLOCK_MODE1 =    0x1 << 9,
        FASTLOCK_MODE2 =    0x3 << 9
    } Fastlock_t;

    /**
     * Timer counter timeout selection. This type is used to set bits 11-14 of
     * the Function (or Initialization) latch. The values for timeout range from
     * 3 to 63 phase/frequency detector cycles. Note that this selection is 
     * ignored unless FASTLOCK_MODE2 is being used. See the ADF4001 Data Sheet 
     * for more information.
     */
    typedef enum {
        TCTIMEOUT_3 =  0x0 << 11,
        TCTIMEOUT_7 =  0x1 << 11,
        TCTIMEOUT_11 = 0x2 << 11,
        TCTIMEOUT_15 = 0x3 << 11,
        TCTIMEOUT_19 = 0x4 << 11,
        TCTIMEOUT_23 = 0x5 << 11,
        TCTIMEOUT_27 = 0x6 << 11,
        TCTIMEOUT_31 = 0x7 << 11,
        TCTIMEOUT_35 = 0x8 << 11,
        TCTIMEOUT_39 = 0x9 << 11,
        TCTIMEOUT_43 = 0xa << 11,
        TCTIMEOUT_47 = 0xb << 11,
        TCTIMEOUT_51 = 0xc << 11,
        TCTIMEOUT_55 = 0xd << 11,
        TCTIMEOUT_59 = 0xe << 11,
        TCTIMEOUT_63 = 0xf << 11
    } TCTimeout_t;

    /**
     * Charge pump current setting 1 selection. This type is used to set bits 
     * 15-17 of the Function (or Initialization) latch. CPCURRENT1_X1 is the 
     * lowest charge pump current, CPCURRENT1_X2 is two times this value,
     * CPCURRENT1_X3 is three times this value, etc. Actual current depends on 
     * the impedance of the loop filter used. See the ADF4001 Data Sheet for 
     * more information.
     */
    typedef enum {
        CPCURRENT1_X1 = 0x0 << 15,
        CPCURRENT1_X2 = 0x1 << 15,
        CPCURRENT1_X3 = 0x2 << 15,
        CPCURRENT1_X4 = 0x3 << 15,
        CPCURRENT1_X5 = 0x4 << 15,
        CPCURRENT1_X6 = 0x5 << 15,
        CPCURRENT1_X7 = 0x6 << 15,
        CPCURRENT1_X8 = 0x7 << 15,                
    } CPCurrent1_t;
    
    /**
     * Charge pump current setting 2 selection. This type is used to set bits 
     * 18-20 of the Function (or Initialization) latch. CPCURRENT2_X1 is the 
     * lowest charge pump current, CPCURRENT2_X2 is two times this value,
     * CPCURRENT2_X3 is three times this value, etc. Actual current depends on 
     * the impedance of the loop filter used. See the ADF4001 Data Sheet for 
     * more information.
     */
    typedef enum {
        CPCURRENT2_X1 = 0x0 << 18,
        CPCURRENT2_X2 = 0x1 << 18,
        CPCURRENT2_X3 = 0x2 << 18,
        CPCURRENT2_X4 = 0x3 << 18,
        CPCURRENT2_X5 = 0x4 << 18,
        CPCURRENT2_X6 = 0x5 << 18,
        CPCURRENT2_X7 = 0x6 << 18,
        CPCURRENT2_X8 = 0x7 << 18,                
    } CPCurrent2_t;

    /**
     * Lock detect precision selection. This type is used to set bit 20 of the
     * R-counter latch.
     */
    typedef enum {
        LDP_3CYCLES = 0x0 << 20,
        LDP_5CYCLES = 0x1 << 20
    } LDP_t;
    
    /**
     * Anti-backlash pulse width selection. This type is used to set bits 16-17
     * of the R-counter latch. The allowed values correspond to pulse widths of
     * 2.9, 1.3, and 6.0 ns.
     */
    typedef enum {
        ABP_2_9NS = 0x0 << 16,
        ABP_1_3NS = 0x1 << 16,
        ABP_6_0NS = 0x2 << 16
    } ABP_t;

    /**
     * Charge pump gain selection. This type is used to set bit 21 of the 
     * N-counter latch. See the ADF4001 Data Sheet for details of its 
     * effect.
     */
    typedef enum {
        CPG_DISABLE_CURRENT2 = 0x0 << 21,
        CPG_ENABLE_CURRENT2  = 0x1 << 21
    } CPGain_t;
    
    /**
     * Construct and return a 24-bit ADF4001 Function latch in the 24 low-order
     * bits of a uint32_t.
     * @param powerdown The Powerdown_t powerdown state to be used. This sets
     *      the values for both bit 21 and bit 3 of the function latch.
     * @param current2 The CPCurrent2_t value for current setting 2.
     * @param current1 The CPCurrent1_t value for current setting 1.
     * @param tctimeout The TCTimeout_t value for timer counter timeout. Note 
     *      that this value is only relevant if fastlock mode 2 is being used.
     * @param fastlock The Fastlock_t mode to be used.
     * @param threestate The CPThreestate_t value for charge pump three-state
     *      output.
     * @param ppolarity The PPolarity_t value for phase detector polarity.
     * @param muxout The Muxout_t value selecting the desired output on the
     *      ADF4001 muxout pin.
     * @param counterreset The CounterReset_t value for counter reset.
     * @return a 24-bit Function latch in the 24 low-order bits of a uint32_t.
     */
    static uint32_t functionLatch(Powerdown_t powerdown, CPCurrent2_t current2,
            CPCurrent1_t current1, TCTimeout_t tctimeout, Fastlock_t fastlock,
            CPThreestate_t threestate, PPolarity_t ppolarity, Muxout_t muxout,
            CounterReset_t counterreset);
    
    /**
     * Construct and return a 24-bit ADF4001 Initialization latch in the 24 
     * low-order bits of a uint32_t.
     * @param powerdown The Powerdown_t powerdown state to be used. This sets
     *      the values for both bit 21 and bit 3 of the function latch.
     * @param current2 The CPCurrent2_t value for current setting 2. Note that 
     *      this value is only relevant if fastlock mode 2 is being used.
     * @param current1 The CPCurrent1_t value for current setting 1.
     * @param tctimeout The TCTimeout_t value for timer counter timeout. Note 
     *      that this value is only relevant if fastlock mode 2 is being used.
     * @param fastlock The Fastlock_t mode to be used.
     * @param threestate The CPThreestate_t value for charge pump three-state
     *      output.
     * @param ppolarity The PPolarity_t value for phase detector polarity.
     * @param muxout The Muxout_t value selecting the desired output on the
     *      ADF4001 muxout pin.
     * @param counterreset The CounterReset_t value for counter reset.
     * @return a 24-bit Initialization latch in the 24 low-order bits of a 
     *      uint32_t.
     */
    static uint32_t initializationLatch(Powerdown_t powerdown, 
            CPCurrent2_t current2, CPCurrent1_t current1, TCTimeout_t tctimeout,
            Fastlock_t fastlock, CPThreestate_t threestate, 
            PPolarity_t ppolarity, Muxout_t muxout, CounterReset_t counterreset);
    
    /**
     * Construct and return a 24-bit R-counter latch in the 24 low-order bits
     * of a uint32_t.
     * @param ldp The LDP_t value for lock detect precision.
     * @param abp The ABP_t value for anti-backlash pulsewidth.
     * @param rdivider The R divider value, which must be in the range 1-16383.
     * @return a 24-bit R-counter latch in the 24 low-order bits of a uint32_t.
     */
    static uint32_t rLatch(LDP_t ldp, ABP_t abp, uint16_t rdivider);
    
    /**
     * Construct and return a 24-bit N-counter latch in the 24 low-order bits
     * of a uint32_t.
     * @param cpg The CPGain_t value for disabling/enabling use of charge pump
     *      current 2.
     * @param ndivider The N divider value, which must be in the range 1-8191.
     * @return a 24-bit R-counter latch in the 24 low-order bits of a uint32_t.
     */
    static uint32_t nLatch(CPGain_t cpg, uint16_t ndivider);

};

#endif /* ADF4001_H_ */
