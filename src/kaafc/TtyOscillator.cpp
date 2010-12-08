/*
 * TtyOscillator.cpp
 *
 *  Created on: Dec 7, 2010
 *      Author: burghart
 */

#include "TtyOscillator.h"
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <termios.h>
#include <fcntl.h>
#include <logx/Logging.h>

LOGGING("TtyOscillator")

TtyOscillator::TtyOscillator(std::string devName, unsigned int oscillatorNum, 
        unsigned int defaultFreq, unsigned int freqStep) :
        _devName(devName), 
        _oscillatorNum(oscillatorNum), 
        _freqStep(freqStep) {
    // Verify that defaultFreq is a multiple of _freqStep
    if ((defaultFreq % _freqStep) != 0) {
        ELOG << __PRETTY_FUNCTION__ << ": for oscillator " << _oscillatorNum <<
                ", default freq " << defaultFreq << 
                " Hz is not a multiple of the step (" << _freqStep << " Hz)";
        abort();
    }
    
    // Open the serial port
    if ((_fd = open(_devName.c_str(), O_RDWR)) == -1) {
        ELOG << __PRETTY_FUNCTION__ << ": error opening " << _devName << ": " <<
                strerror(errno);
        abort();
    }
    
    // Make the port 9600 8N1 and "raw"
    struct termios ios;
    if (tcgetattr(_fd, &ios) == -1) {
        ELOG << __PRETTY_FUNCTION__ << ": error getting " << _devName << 
                " attributes: " << strerror(errno);
        abort();
    }
    cfmakeraw(&ios);
    cfsetspeed(&ios, B9600);
    if (tcsetattr(_fd, TCSAFLUSH, &ios) == -1) {
        ELOG << __PRETTY_FUNCTION__ << ": error setting " << _devName << 
                " attributes: " << strerror(errno);
        abort();
    }
    
    // Start with the oscillator frequency at the default value
    setFrequency(defaultFreq);
}

TtyOscillator::~TtyOscillator() {
}

void
TtyOscillator::setFrequency(unsigned int freq) {
    // Scale frequency into units of _freqStep
    unsigned int scaledFreq = (freq / _freqStep);
    // Complain if frequency is not a multiple of _freqStep
    if ((freq % _freqStep) != 0) {
        freq = scaledFreq * _freqStep; // here's what we'll really get
        WLOG << __PRETTY_FUNCTION__ << ": for oscillator " << _oscillatorNum <<
                ", frequency " << freq << 
                " Hz is not a multiple of the step (" << _freqStep << " Hz)" <<
                "\n    Using " << freq << " Hz instead.";
    }
    
    while (freq != _currentFreq) {
        // Set frequency command is 8 bytes:
        //      byte 0:     oscillator number (ASCII '0', 1', or '2')
        //      byte 1:     ASCII 'm'
        //      bytes 2-6:  5-digit frequency string, in units of the oscillator's 
        //                  frequency step.
        //      byte 7:     ASCII NUL
        // E.g., to set oscillator 0's frequency to 12345 freqStep units, the
        // command would be "0m12345\0".
        char freqCmd[8];
        sprintf(freqCmd, "%cm%05u", '0' + _oscillatorNum, scaledFreq);
        write(_fd, freqCmd, sizeof(freqCmd));
        if (_getStatus() != 0) {
            ELOG << __PRETTY_FUNCTION__ << ": status timeout for oscillator " <<
                    _oscillatorNum;
        }
    }
}

int
TtyOscillator::_getStatus() {
    // Get rid of any unread input from the oscillator
    tcflush(_fd, TCIFLUSH);
    
    // Send the status request
    // The status command is 3 bytes:
    //      byte 0:     oscillator number (ASCII '0', 1', or '2')
    //      byte 1:     ASCII 's'
    //      byte 2:     ASCII NUL
    char statusCmd[3];
    sprintf(statusCmd, "%cs", '0' + _oscillatorNum);
    write(_fd, statusCmd, sizeof(statusCmd));
    
    // We'll wait up to 2 seconds to get the reply
    alarm(2);
    
    // Get the full 13-byte reply
    char reply[13];
    int nread = 0;
    while (nread < 13) {
        int status = read(_fd, reply + nread, 13 - nread);
        if (status < 0) {
            switch (errno) {
            case EINTR:
                // We got the timeout alarm
                WLOG << __PRETTY_FUNCTION__ << 
                    ": status timeout for oscillator " << _oscillatorNum;
                break;
            default:
                WLOG << __PRETTY_FUNCTION__ << ": oscillator " << 
                    _oscillatorNum << ": " << strerror(errno);
                alarm(0);   // cancel timeout alarm
                break;
            }
            return 1;
        }
        nread += status;
    }
    alarm(0);   // cancel timeout alarm
    
    unsigned int scaledFreq;
    sscanf(reply + 8, "%5u", &scaledFreq);
    _currentFreq = scaledFreq * _freqStep;
    
    return 0;
}
