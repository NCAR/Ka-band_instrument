/*
 * Adf4001.cpp
 *
 *  Created on: Dec 1, 2010
 *      Author: burghart
 */

#include "Adf4001.h"
#include <iostream>
#include <cstdlib>

uint32_t
Adf4001::rLatch(Adf4001::LDP_t ldp, Adf4001::ABP_t abp, uint16_t rdivider) {
    if (rdivider < 1 || rdivider > 16383) {
        std::cerr << __PRETTY_FUNCTION__ << ": bad rdivider " << rdivider << 
                ", value must be between 1 and 16383!" << std::endl;
        abort();
    }
    return uint32_t(ldp | abp | rdivider << 2 | 0x0);
}

uint32_t
Adf4001::nLatch(Adf4001::CPGain_t cpg, uint16_t ndivider) {
    if (ndivider < 1 || ndivider > 8191) {
        std::cerr << __PRETTY_FUNCTION__ << ": bad ndivider " << ndivider <<
                ", value must be between 1 and 8191!" << std::endl;
        abort();
    }
    return uint32_t(cpg | ndivider << 8 | 0x1);
}

uint32_t
Adf4001::functionLatch(Adf4001::Powerdown_t powerdown, 
        Adf4001::CPCurrent2_t current2, Adf4001::CPCurrent1_t current1, 
        Adf4001::TCTimeout_t tctimeout, Adf4001::Fastlock_t fastlock,
        Adf4001::CPThreestate_t threestate, Adf4001::PPolarity_t ppolarity, 
        Adf4001::Muxout_t muxout, Adf4001::CounterReset_t counterreset) {
    return uint32_t(powerdown | current2 | current1 | tctimeout | fastlock |
            threestate | ppolarity | muxout | counterreset | 0x2);
}

uint32_t
Adf4001::initializationLatch(Adf4001::Powerdown_t powerdown, 
        Adf4001::CPCurrent2_t current2, Adf4001::CPCurrent1_t current1, 
        Adf4001::TCTimeout_t tctimeout, Adf4001::Fastlock_t fastlock,
        Adf4001::CPThreestate_t threestate, Adf4001::PPolarity_t ppolarity, 
        Adf4001::Muxout_t muxout, Adf4001::CounterReset_t counterreset) {
    return uint32_t(powerdown | current2 | current1 | tctimeout | fastlock |
            threestate | ppolarity | muxout | counterreset | 0x3);
}

