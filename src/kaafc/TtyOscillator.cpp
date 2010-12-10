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
#include <ctime>
#include <termios.h>
#include <fcntl.h>
#include <logx/Logging.h>

LOGGING("TtyOscillator")

TtyOscillator::TtyOscillator(std::string devName, unsigned int oscillatorNum, 
        unsigned int startFreq, unsigned int freqStep) :
        _devName(devName), 
        _oscillatorNum(oscillatorNum), 
        _freqStep(freqStep),
        _currentFreq(0),
        _nextWrite(0) {
    // Verify that startFreq is a multiple of _freqStep
    if ((startFreq % _freqStep) != 0) {
        ELOG << __PRETTY_FUNCTION__ << ": for oscillator " << _oscillatorNum <<
                ", start freq " << startFreq <<
                " Hz is not a multiple of the step (" << _freqStep << " Hz)";
        abort();
    }
    
    // Open the serial port
    if ((_fd = open(_devName.c_str(), O_RDWR)) == -1) {
        ELOG << __PRETTY_FUNCTION__ << ": error opening " << _devName << ": " <<
                strerror(errno);
        abort();
    }
    
    // Make the port 9600 8N1, "raw", and with a timeout for reads
    struct termios ios;
    if (tcgetattr(_fd, &ios) == -1) {
        ELOG << __PRETTY_FUNCTION__ << ": error getting " << _devName << 
                " attributes: " << strerror(errno);
        abort();
    }
    cfmakeraw(&ios);
    cfsetspeed(&ios, B9600);

    ios.c_cc[VMIN] = 0;
    ios.c_cc[VTIME] = 2;    // 0.2 second timeout for reads

    if (tcsetattr(_fd, TCSAFLUSH, &ios) == -1) {
        ELOG << __PRETTY_FUNCTION__ << ": error setting " << _devName << 
                " attributes: " << strerror(errno);
        abort();
    }
    
    // Set the initial oscillator frequency
    setFrequency(startFreq);
}

TtyOscillator::~TtyOscillator() {
}

void
TtyOscillator::setFrequency(unsigned int freq) {
    if (freq == _currentFreq)
        return;

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
    
    int maxattempts = 5;
    for (int attempt = 0; attempt < maxattempts; attempt++) {
        // Set frequency command is 8 bytes:
        //      byte 0:     oscillator number (ASCII '0', 1', or '2')
        //      byte 1:     ASCII 'm'
        //      bytes 2-6:  5-digit frequency string, in units of the oscillator's 
        //                  frequency step.
        //      byte 7:     ASCII NUL
        // E.g., to set oscillator 0's frequency to 12345 freqStep units, the
        // command would be "0m12345\0".
        //
        // Note that after we write the command, we should not send *anything*
        // to the oscillator for a bit, or the command will never be
        // recognized!
        char freqCmd[8];
        sprintf(freqCmd, "%cm%05u", '0' + _oscillatorNum, scaledFreq);
        _sendCmd(freqCmd, 3);   // send the command and allow 3 seconds before next

        // Get the status and see if the frequency matches.
        if (_getStatus() != 0) {
            ELOG << __PRETTY_FUNCTION__ <<
                    ": status read error for oscillator " << _oscillatorNum;
        }
        if (_currentFreq == freq)
            return;
    }
    // If we get here, we got no status reply after many attempts
    ELOG << __PRETTY_FUNCTION__ << ": Failed for oscillator " <<
            _oscillatorNum << " after " << maxattempts <<
            "attempts. Command wait time may be too short.";
    abort();
}

int
TtyOscillator::_getStatus() {
    // Get rid of any unread input from the oscillator
    tcflush(_fd, TCIFLUSH);
    
    int maxattempts = 10;
    for (int attempt = 0; attempt < maxattempts; attempt++) {
        // Send the status request
        // The status command is 3 bytes:
        //      byte 0:     oscillator number (ASCII '0', 1', or '2')
        //      byte 1:     ASCII 's'
        //      byte 2:     ASCII NUL
        char statusCmd[3];
        sprintf(statusCmd, "%cs", '0' + _oscillatorNum);
        _sendCmd(statusCmd, 0);

        // Get the full 13-byte reply
        char reply[13];
        int nread = 0;
        bool timeout = false;
        while (nread < 13) {
            int status = read(_fd, reply + nread, 13 - nread);
            if (status == 0) {
                WLOG << __PRETTY_FUNCTION__ << ": read timeout";
                timeout = true;
                break;
            } else if (status < 0) {
                WLOG << __PRETTY_FUNCTION__ << ": oscillator " <<
                        _oscillatorNum << ": " << strerror(errno);
                return 1;
            }
            nread += status;
        }
        // If we got a read timeout, go back and try the status command again
        if (timeout)
            continue;

        // Read frequency from the status message and set _currentFreq
        unsigned int scaledFreq;
        sscanf(reply + 8, "%5u", &scaledFreq);
        _currentFreq = scaledFreq * _freqStep;
        DLOG << "Current freq is " << _currentFreq << " Hz";

        return 0;
    }
    // If we get here, we got no status reply after many attempts
    ELOG << __PRETTY_FUNCTION__ << ": No status reply from oscillator " <<
            _oscillatorNum << " after " << maxattempts <<
            "attempts. Is the serial line connected?";
    abort();
}

void
TtyOscillator::_sendCmd(char *cmd, unsigned int timeNeeded) {
    int cmdLen = strlen(cmd) + 1;   // include the terminating NULL

    // If the previous command sent still needs time to complete, sleep a bit
    int delay = _nextWrite - time(0);
    if (delay > 0)
        sleep(delay);

    // Send the command
    DLOG << "Sending command '" << cmd << "'";
    write(_fd, cmd, cmdLen);

    // Set the soonest time to send the next command
    _nextWrite = time(0) + timeNeeded;
}
