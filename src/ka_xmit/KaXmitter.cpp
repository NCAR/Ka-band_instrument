/*
 * KaXmitter.cpp
 *
 *  Created on: Jan 7, 2011
 *      Author: burghart
 */

#include "KaXmitter.h"

#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <termios.h>
#include <fcntl.h>

#include <logx/Logging.h>

LOGGING("KaXmitter")

const std::string KaXmitter::SIM_DEVICE = "SimulatedKaXmitter";

KaXmitter::KaXmitter(std::string ttyDev) :
        _simulate(ttyDev == SIM_DEVICE),
        _ttyDev(ttyDev),
        _fd(-1) {
    // Open the serial port
    if (! _simulate) {
        if ((_fd = open(_ttyDev.c_str(), O_RDWR)) == -1) {
            ELOG << __PRETTY_FUNCTION__ << ": error opening " << _ttyDev << ": " <<
                    strerror(errno);
            abort();
        }

        // Make the port 9600 8N1, "raw"
        struct termios ios;
        if (tcgetattr(_fd, &ios) == -1) {
            ELOG << __PRETTY_FUNCTION__ << ": error getting " << _ttyDev << 
                    " attributes: " << strerror(errno);
            abort();
        }
        cfmakeraw(&ios);
        cfsetspeed(&ios, B9600);
    }
}

KaXmitter::~KaXmitter() {
}
