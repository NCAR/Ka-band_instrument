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

const std::string TtyOscillator::SIM_OSCILLATOR("<SimTtyOscillator>");

TtyOscillator::TtyOscillator(std::string devName, unsigned int oscillatorNum, 
        unsigned int freqStep, unsigned int scaledMinFreq, 
        unsigned int scaledMaxFreq, unsigned int scaledStartFreq) :
        _devName(devName), 
        _oscillatorNum(oscillatorNum), 
        _freqStep(freqStep),
        _scaledRequestedFreq(0),
        _scaledCurrentFreq(0),
        _scaledMinFreq(scaledMinFreq),
        _scaledMaxFreq(scaledMaxFreq),
        _nextWrite(0),
        _simulate(devName == SIM_OSCILLATOR) {
    // Test that min <= start <= max
    if (scaledStartFreq < scaledMinFreq) {
        ELOG << "Oscillator " << _oscillatorNum << " scaled start freq " << 
            scaledStartFreq << " is less than scaled min freq " << scaledMinFreq;
        abort();
    }
    if (scaledStartFreq > scaledMaxFreq) {
        ELOG << "Oscillator " << _oscillatorNum << " scaled start freq " << 
            scaledStartFreq << " is greater than scaled max freq " << 
            scaledMaxFreq;
        abort();
    }
    // Set up the serial port if we're not simulating
    if (! _simulate) {
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
    }
    
    // Set the initial oscillator frequency
    setScaledFreq(scaledStartFreq);
}

TtyOscillator::~TtyOscillator() {
}

void
TtyOscillator::setScaledFreq(unsigned int scaledFreq) {
    int maxattempts = 5;
    for (int attempt = 0; attempt < maxattempts; attempt++) {
        if (attempt > 0) {
            ILOG << __PRETTY_FUNCTION__ << ": attempt " << attempt + 1 <<
                    " to set frequency...";
        }
        setScaledFreqAsync(scaledFreq);
        if (freqAttained())
            return;
    }
    // If we get here, we got no status reply after many attempts
    ELOG << __PRETTY_FUNCTION__ << ": Failed for oscillator " <<
            _oscillatorNum << " after " << maxattempts <<
            " attempts. _FREQCHANGE_WAIT time may be too short.";
    abort();
}

void
TtyOscillator::setScaledFreqAsync(unsigned int scaledFreq) {
    // min/max checking
    if (scaledFreq > _scaledMaxFreq) {
        WLOG << __PRETTY_FUNCTION__ << ": requested scaled frequency " << 
                scaledFreq << " has been set to the maximum value of " <<
                _scaledMaxFreq;
        scaledFreq = _scaledMaxFreq;
    }
    if (scaledFreq < _scaledMinFreq) {
        WLOG << __PRETTY_FUNCTION__ << ": requested scaled frequency " << 
                scaledFreq << " has been set to the minimum value of " <<
                _scaledMinFreq;
        scaledFreq = _scaledMinFreq;
    }
    
    // Nothing to do if they requested the current frequency...
    if (scaledFreq == _scaledCurrentFreq)
        return;

    DLOG << __PRETTY_FUNCTION__ << ": setting frequency to (" << scaledFreq <<
            " x " << _freqStep << ") Hz";
    
    // Set _scaledRequestedFreq to non-zero while the request is in progress.
    // It will be set back to zero by the freqAttained() test.
    _scaledRequestedFreq = scaledFreq;
    
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
    _sendCmd(freqCmd, _FREQCHANGE_WAIT);   // send the command and allow _FREQCHANGE_WAIT seconds before next
}

bool
TtyOscillator::freqAttained() {
    // If we're not in the middle of a change, we're good!
    if (! _scaledRequestedFreq)
        return(true);
    // Get the status and see if the frequency matches the requested frequency.
    if (! _getStatus()) {
        ELOG << __PRETTY_FUNCTION__ <<
                ": status read error for oscillator " << _oscillatorNum;
    }
    bool attained = (_scaledCurrentFreq == _scaledRequestedFreq);
    _scaledRequestedFreq = 0;   // request completed
    return(attained);
}

bool
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
        _sendCmd(statusCmd, _STATUS_WAIT);
        
        // If simulating, behave as if the requested frequency is now the
        // current frequency.
        if (_simulate) {
            if (_scaledRequestedFreq)
                _scaledCurrentFreq = _scaledRequestedFreq;
            return(0);
        }

        // Get the full 13-byte reply
        char reply[14];
        int nread = 0;
        bool timeout = false;
        while (nread < 13) {
            int status = read(_fd, reply + nread, 13 - nread);
            if (status == 0) {
                WLOG << __PRETTY_FUNCTION__ << ": oscillator " <<
                        _oscillatorNum << " status read timeout";
                timeout = true;
                break;
            } else if (status < 0) {
                WLOG << __PRETTY_FUNCTION__ << ": oscillator " <<
                        _oscillatorNum << ": " << strerror(errno);
                return(1);
            }
            nread += status;
        }
        // If we got a read timeout, go back and try the status command again
        if (timeout)
            continue;

        // Read frequency from the status message and set _currentFreq
        sscanf(reply + 8, "%5u", &_scaledCurrentFreq);
        DLOG << "Oscillator " << _oscillatorNum << " status '" << reply << "'";
        DLOG << "Oscillator " << _oscillatorNum << " freq is (" <<
                _scaledCurrentFreq << " x " << _freqStep << ") Hz";

        return 0;
    }
    // If we get here, we got no status reply after many attempts
    ELOG << __PRETTY_FUNCTION__ << ": No status reply from oscillator " <<
            _oscillatorNum << " after " << maxattempts <<
            "attempts. Is the serial line connected? Is the oscillator num wrong?";
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
    if (! _simulate)
        write(_fd, cmd, cmdLen);

    // Set the soonest time to send the next command
    _nextWrite = time(0) + timeNeeded;
}
