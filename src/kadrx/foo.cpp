#include <iostream>
#include <unistd.h>
#include <vector>
#include <Pmc730.h>

Pmc730 *card;

const unsigned int PLL_CLOCK_LINE       = 8;
const unsigned int PLL_CLOCKINV_LINE    = 9;
const unsigned int PLL_DATA_LINE        = 10;
const unsigned int PLL_DATAINV_LINE     = 11;
const unsigned int PLL_CE_LINE          = 12;
const unsigned int PLL_CEINV_LINE       = 13;
const unsigned int PLL_LE_LINE          = 14;
const unsigned int PLL_LEINV_LINE       = 15;

const unsigned int STARTBIT_PD2 = 21;
const unsigned int STARTBIT_CS2 = 18;
const unsigned int STARTBIT_CS1 = 15;
const unsigned int STARTBIT_TC = 11;
const unsigned int STARTBIT_CP3STATE = 8;
const unsigned int STARTBIT_PHDPOL = 7;
const unsigned int STARTBIT_PD1 = 3;
const unsigned int STARTBIT_RESET = 2;
const unsigned int STARTBIT_CONTROL = 0;

unsigned char 
turnBitOn(unsigned char src, unsigned int bitnum) {
    unsigned char mask = (1 << bitnum);
    return(src | mask);
}

unsigned char
turnBitOff(unsigned char src, unsigned int bitnum) {
    unsigned char mask = (1 << bitnum);
    return(src & ~mask);
}

void setClock(unsigned int bit) {
    unsigned char new8_15 = card->getDio8_15();
    if (bit) {
        new8_15 = turnBitOn(new8_15, PLL_CLOCK_LINE - 8);
        new8_15 = turnBitOff(new8_15, PLL_CLOCKINV_LINE - 8);
    } else {
        new8_15 = turnBitOff(new8_15, PLL_CLOCK_LINE - 8);
        new8_15 = turnBitOn(new8_15, PLL_CLOCKINV_LINE - 8);
    }
    card->setDio8_15(new8_15);
}

void setData(unsigned int bit) {
    unsigned char new8_15 = card->getDio8_15();
    if (bit) {
        new8_15 = turnBitOn(new8_15, PLL_DATA_LINE - 8);
        new8_15 = turnBitOff(new8_15, PLL_DATAINV_LINE - 8);
    } else {
        new8_15 = turnBitOff(new8_15, PLL_DATA_LINE - 8);
        new8_15 = turnBitOn(new8_15, PLL_DATAINV_LINE - 8);
    }
    card->setDio8_15(new8_15);
}

void setLe(unsigned int bit) {
    unsigned char new8_15 = card->getDio8_15();
    if (bit) {
        new8_15 = turnBitOn(new8_15, PLL_LE_LINE - 8);
        new8_15 = turnBitOff(new8_15, PLL_LEINV_LINE - 8);
    } else {
        new8_15 = turnBitOff(new8_15, PLL_LE_LINE - 8);
        new8_15 = turnBitOn(new8_15, PLL_LEINV_LINE - 8);
    }
    card->setDio8_15(new8_15);
}

void setCe(unsigned int bit) {
    unsigned char new8_15 = card->getDio8_15();
    if (bit) {
        new8_15 = turnBitOn(new8_15, PLL_CE_LINE - 8);
        new8_15 = turnBitOff(new8_15, PLL_CEINV_LINE - 8);
    } else {
        new8_15 = turnBitOff(new8_15, PLL_CE_LINE - 8);
        new8_15 = turnBitOn(new8_15, PLL_CEINV_LINE - 8);
    }
    card->setDio8_15(new8_15);
}

/**
 * Send out a 24-bit latch command to the PLL chip
 * @param val the 24-bit value to send to the PLL (only the low order 24 bits
 *    of val are sent to the PLL).
 */
void pllBitbang24(uint32_t val) {
    // Assure that CLOCK and LE lines are low when we start.
    setClock(0);
    setLe(0);
    // Send over each bit, from most to least significant
    for (int bitnum = 23; bitnum >= 0; bitnum--) {
        // Send the bit on the DATA line
        unsigned int bit = (val >> bitnum) & 0x1;
        unsigned int invbit = ~bit & 0x1;
        setData(bit);
        // CLOCK high to make the PLL get the bit
        setClock(1);
        // ..and CLOCK back to low
        setClock(0);
    }
    // Finally, set LE high to latch the whole thing
    setLe(1);
    // ..and LE back to low to clean up
    setLe(0);    
}

typedef enum { INIT_LATCH_METHOD, CE_METHOD, COUNTER_RESET_METHOD } Method_t;

typedef enum { 
    FASTLOCK_DISABLED = 0x000000, 
    FASTLOCK_MODE1 = 0x000200,
    FASTLOCK_ALSO_DISABLED = 0x000400,
    FASTLOCK_MODE2 = 0x000600,
    FASTLOCK_ALLBITS = 0x000600         // bits 9-10 control fastlock
} Fastlock_t;

typedef enum {
    MUX_THREESTATE = 0x000000,
    MUX_DLOCK_DETECT = 0x000010,
    MUX_NDIVIDER = 0x000020,
    MUX_AVDD = 0x000030,
    MUX_RDIVIDER = 0x000040,
    MUX_NLOCK_DETECT = 0x000050,
    MUX_SERIAL_DATA = 0x000060,
    MUX_DGND = 0x000070,
    MUX_ALLBITS = 0x000070              // bits 4-6 define mux output
} Mux_t;

typedef enum {
    COUNTER_RESET_DISABLED = 0x000000,
    COUNTER_RESET_ENABLED = 0x000004,
    COUNTER_RESET_ALLBITS = 0x000004    // bit 2 is for counter reset
} CounterReset_t;

typedef enum {
    CONTROL_R = 0x000000,
    CONTROL_N = 0x000001,
    CONTROL_FUNCTION = 0x000002,
    CONTROL_INITIALIZATION = 0x000003,
    CONTROL_ALLBITS = 0x000003            // bits 0-1 define the latch being set
} Control_t;
    
int main(int argc, char *argv[]) {
    
    card = new Pmc730(0);
    card->setDioDirection(0, 1);
    
    Method_t method = INIT_LATCH_METHOD;
    
    unsigned int pd2 = 0;
    unsigned int current2 = 0x7;
    unsigned int current1 = 0x7;
    unsigned int tc = 0xf;          // 4 bits
    Fastlock_t fastlock = FASTLOCK_DISABLED;
    unsigned int cp3state = 0x0;    // 1 bit
    unsigned int polarity = 0x1;    // 1 bit
    Mux_t muxout = MUX_RDIVIDER;
    unsigned int pd1 = 0x0;         // 1 bit
    CounterReset_t reset = COUNTER_RESET_DISABLED;
    unsigned int function = (pd2 << STARTBIT_PD2) | (current2 << STARTBIT_CS2) |
        (current1 << STARTBIT_CS1) | (tc << STARTBIT_TC) | 
        fastlock | (cp3state << STARTBIT_CP3STATE) |
        (polarity << STARTBIT_PHDPOL) | muxout |
        (pd1 << STARTBIT_PD1) | reset;
        
    // PLL chip enable (CE)
    setCe(1);
    
    unsigned int r = 200;  // 10 MHz / 200 = 50kHz
    unsigned int cmd;
    for (unsigned int n = 2000; n <= 2400; n += 50) {
//    for (unsigned int n = 1800; n <= 2600; n += 200) {
//    for (unsigned int n = 100; n <= 8100; n += 2000) {
        // Before loading R and N...
        switch (method) {
            case INIT_LATCH_METHOD:
                pllBitbang24(function | CONTROL_INITIALIZATION); // initialization latch
                break;
            case CE_METHOD:
                setCe(0);
                pllBitbang24(function | CONTROL_FUNCTION); // function latch
                break;
            case COUNTER_RESET_METHOD:
                pllBitbang24(function | COUNTER_RESET_ENABLED | CONTROL_FUNCTION); // function latch, reset and halt counters
                break;
        }
        
        // Load R and N
        cmd = 0x100000 | (r << 2) | CONTROL_R;
        pllBitbang24(cmd); // R counter
        
        cmd = (n << 8) | CONTROL_N;
        pllBitbang24(cmd); // N counter
        
        // After loading R and N...
        switch (method) {
            case INIT_LATCH_METHOD:
                break;
            case CE_METHOD:
                setCe(1);
                break;
            case COUNTER_RESET_METHOD:
                pllBitbang24(function | CONTROL_FUNCTION); // function latch, reenable counters
                break;
        }
        std::cout << "Set N to " << n << " (" << (n * 50000) << " Hz)" << std::endl;
        sleep(10);
    }
    
    while (1) {
        sleep(1);
    }
}
