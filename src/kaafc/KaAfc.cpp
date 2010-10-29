/*
 * KaAfc.cpp
 *
 *  Created on: Oct 29, 2010
 *      Author: burghart
 */

#include <iostream>
#include <pmc730.h>

int
main(int argc, char *argv[]) {
    struct pmc730 c_block;
    struct dio730 dio730;/* digital I/O section pointer */
    struct sdio730 sdio730;/* Status block pointer */
    struct handler_data hdata;/* interrupt handler structure */

    /*
     * Initialize the Pmc library
     */
    if (InitPmcLib() != S_OK)
    {
        std::cerr << "Unable to initialize the Pmc library." << std::endl;
        exit(1);
    }

    /*
        Initialize the Configuration Parameter Block to default values.
     */
    c_block.bPmc = FALSE;               /* indicate no Pmc initialized and set up yet */
    c_block.bInitialized = FALSE;       /* indicate not ready to talk to Pmc */
    c_block.nHandle = 0;                /* make handle to a closed Pmc board */
    c_block.dio730ptr = &dio730;        /* digital I/O section pointer */
    c_block.sdio730ptr = &sdio730;      /* Status block pointer */

    for (int item = 0; item < 2; item++)
    { 
        /* configuration structure */
        c_block.dio730ptr->param = 0;               /* parameter mask */
        c_block.dio730ptr->int_status[item] = 0;    /* pending interrupts to clear */
        c_block.dio730ptr->int_enable[item] = 0;    /* interrupt enable (per bit) */
        c_block.dio730ptr->int_polarity[item] = 0;  /* interrupt polarity */
        c_block.dio730ptr->int_type[item] = 0;      /* interrupt type */
        c_block.dio730ptr->direction = 0;           /* direction */
        c_block.dio730ptr->debounce_duration[item] = 0;/* debounce */

        /* status structure */
        c_block.sdio730ptr->int_status[item] = 0;   /* pending interrupts to clear */
        c_block.sdio730ptr->int_enable[item] = 0;   /* interrupt enable (per bit) */
        c_block.sdio730ptr->int_polarity[item] = 0; /* interrupt polarity */
        c_block.sdio730ptr->int_type[item] = 0;     /* interrupt type */
        c_block.sdio730ptr->direction = 0;          /* direction */
        c_block.sdio730ptr->debounce_duration[item] = 0;/* debounce */
    }

    /*
     * Open the PMC-730 device.
     */
    if (PmcOpen(0, &c_block.nHandle, DEVICE_NAME) != S_OK)
    {
        std::cerr << "Unable to open the PMC-730 device!" << std::endl;
        exit(1);
    }
    
    std::cout << "PMC-730 device opened" << std::endl;
    
    PmcClose(c_block.nHandle);
}
