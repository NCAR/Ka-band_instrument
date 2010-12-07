/*
 * KaAfc.cpp
 *
 *  Created on: Oct 29, 2010
 *      Author: burghart
 */

#include <iostream>
#include <Pmc730.h>
#include "KaOscillator3.h"
#include <logx/Logging.h>

LOGGING("KaAfc")

int
main(int argc, char *argv[]) {
    logx::ParseLogArgs(argc, argv);
    Pmc730 card(0);
    card.setDioDirection8_15(1);
    KaOscillator3 osc3(card);
}
