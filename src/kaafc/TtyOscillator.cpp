/*
 * TtyOscillator.cpp
 *
 *  Created on: Dec 7, 2010
 *      Author: burghart
 */

#include "TtyOscillator.h"
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <logx/Logging.h>

LOGGING("TtyOscillator")

TtyOscillator::TtyOscillator(std::string devName, unsigned int defaultFreq,
        unsigned int freqStep) :
        _devName(devName), _freqStep(freqStep) {
    /*
     * Open the serial port
     */
    if ((_fd = open(_devName.c_str(), O_RDWR)) == -1) {
        ELOG << __PRETTY_FUNCTION__ << ": error opening " << _devName << ": " <<
                strerror(errno);
        abort();
    }
}

TtyOscillator::~TtyOscillator() {
}
