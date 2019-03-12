// QM2010_Oscillator.cpp
//
//  Created on: Sep 26, 2018
//      Author: Chris Burghart <burghart@ucar.edu>

#include "QM2010_Oscillator.h"
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <exception>
#include <iomanip>
#include <sstream>
#include <unistd.h>
#include <Logging.h>

LOGGING("QM2010_Oscillator")

QM2010_Oscillator::QM2010_Oscillator(std::string devName,
                                     uint oscNum,
                                     uint refFreqMhz,
                                     uint freqStep,
                                     uint scaledMinFreq,
                                     uint scaledMaxFreq) :
_devName(devName),
_fd(-1),
_oscNum(oscNum),
_freqStep(freqStep),
_scaledCurrentFreq(0),
_scaledMinFreq(scaledMinFreq),
_scaledMaxFreq(scaledMaxFreq),
_locked(false) {
    // Test that min <= max
    if (_scaledMinFreq > _scaledMaxFreq) {
        std::ostringstream os;
        os << _oscName() << " scaled min freq " << _scaledMinFreq <<
              " is greater than scaled max freq " << _scaledMaxFreq;
        ELOG << os.str();
        throw(std::runtime_error(os.str()));
    }
    // Open the device
    if ((_fd = open(_devName.c_str(), O_RDWR)) == -1) {
        std::ostringstream os;
        os << _oscName() << ": Error opening " <<
              _devName << ": " << strerror(errno);
        ELOG << os.str();
        throw(std::runtime_error(os.str()));
    }

    std::string cmd;

    // Get the current frequency (returned in MHz) and calculate
    // _scaledCurrentFreq in units of _freqStep
    float currentFreqMhz = _sendCmdAndGetFloatReply("FREQ:RETACT?");
    _scaledCurrentFreq = rintf((1.0e6 * currentFreqMhz) / _freqStep);
    ILOG << _oscName() << " started at " << currentFreqMhz << " MHz";

    // Note if the oscillator's startup frequency is not between the specified
    // min and max.
    if ((_scaledCurrentFreq < _scaledMinFreq) ||
        (_scaledCurrentFreq > _scaledMaxFreq)) {
        WLOG << _oscName() <<
            " startup scaled freq (" << _scaledCurrentFreq <<
            ") is not between given min (" << _scaledMinFreq << ") and max (" <<
            _scaledMaxFreq << ")";
    }

    // Set up to use external reference at the given frequency
    {
        std::ostringstream os;
        os << "FREQ:REF:FREQ " << refFreqMhz << "; FREQ:REF:FREQ?";
        cmd = os.str();
    }
    int replyFreqMhz = _sendCmdAndGetIntReply(cmd);
    if (replyFreqMhz != int(refFreqMhz)) {
        std::ostringstream os;
        os << _oscName() << ": failed to set reference frequency to " <<
              refFreqMhz << " MHz!";
        throw(std::runtime_error(os.str()));
    }

    bool usingExtRef = _sendCmdAndGetBoolReply("FREQ:REF:EXT ON; FREQ:REF:EXT?");
    if (! usingExtRef) {
        std::ostringstream os;
        os << _oscName() << ": failed to enable use of external reference";
        throw(std::runtime_error(os.str()));
    }

    // Set frequency reference divider so that _freqStep is the integer PLL step
    // and set the PLL mode to "integer"
    int refDiv = (1000000 * refFreqMhz) / _freqStep;
    {
        std::ostringstream os;
        os << "FREQ:REF:DIV " << refDiv << "; FREQ:REF:DIV?";
        cmd = os.str();
    }
    int replyRefDiv = _sendCmdAndGetIntReply(cmd);
    if (replyRefDiv != refDiv) {
        std::ostringstream os;
        os << _oscName() << ": failed to set FREQ:REF_DIV to " << refDiv;
        throw(std::runtime_error(os.str()));
    }

    int replyPllmInt = _sendCmdAndGetIntReply("FREQ:PLLM INT; FREQ:PLLM?");
    if (replyPllmInt != 1) {    // 0 -> Fractional Mode, 1 -> Integer Mode
        std::ostringstream os;
        os << _oscName() << ": failed to set PLL mode to INTEGER";
        throw(std::runtime_error(os.str()));
    }

    // @TODO Below here is configuration that is specific for the Ka-band
    // radar's "oscillator 0". This should probably be moved elsewhere...

    // Turn off oscillator output while we configure
    bool rfOn = _sendCmdAndGetBoolReply("POWER:RF OFF; POWER:RF?");
    if (rfOn) {
        std::ostringstream os;
        os << _oscName() << ": failed to turn off RF output";
        ELOG << os.str();
        throw(std::runtime_error(os.str()));
    }

    // Set the output power we want from the oscillator. We currently fix this
    // at 12 dBm.
    float requestPowerDbm = 12.0;
    {
        std::ostringstream os;
        // We can specify the desired power to 0.1 dBm precision
        os << "POWER:SET " << std::fixed << std::setprecision(1) <<
              requestPowerDbm << "; POWER:SET?";
        cmd = os.str();
    }
    float actualPowerDbm = _sendCmdAndGetFloatReply(cmd);
    float powerDiff = fabs(requestPowerDbm - actualPowerDbm);
    if (powerDiff > 0.2) {
        WLOG << _oscName() << ": requested output power " <<
              requestPowerDbm << " dBm, but got " << actualPowerDbm << " dBm";
    } else {
        ILOG << _oscName() << ": output power is " << actualPowerDbm << "dBm";
    }

    // Finally, turn on RF output
    rfOn = _sendCmdAndGetBoolReply("POWER:RF ON; POWER:RF?");
    if (! rfOn) {
        std::ostringstream os;
        os << _oscName() << ": failed to turn on RF output";
        ELOG << os.str();
        throw(std::runtime_error(os.str()));
    }

    // Wait briefly for lock and log the result
    _logLockedState();
}

QM2010_Oscillator::~QM2010_Oscillator() {
    DLOG << "destructor for " << _oscName();
    close(_fd);
}

void
QM2010_Oscillator::setScaledFreq(unsigned int scaledFreq) {
    // min/max checking
    if (scaledFreq > _scaledMaxFreq) {
        WLOG << _oscName() << ": requested scaled frequency " << scaledFreq << 
            " has been set to the maximum value of " << _scaledMaxFreq;
        scaledFreq = _scaledMaxFreq;
    }
    if (scaledFreq < _scaledMinFreq) {
        WLOG << _oscName() << ": requested scaled frequency " << scaledFreq << 
            " has been set to the minimum value of " << _scaledMinFreq;
        scaledFreq = _scaledMinFreq;
    }

    // Calculate requested frequency in MHz
    float requestFreqMhz = (_freqStep * scaledFreq) * 1.0e-6;

    // Nothing to do if they requested the current frequency...
    if (scaledFreq == _scaledCurrentFreq) {
        ILOG << _oscName() << " already at " << requestFreqMhz << " MHz";
        return;
    }

    // Send a command to request the new frequency and get the actual resulting
    // frequency.
    std::ostringstream os;
    os << "FREQ:SET " << std::fixed << std::setprecision(3) << requestFreqMhz <<
          "; FREQ:RETACT?";
    float newFreqMhz = _sendCmdAndGetFloatReply(os.str());

    // Complain if the resulting output frequency is too far from the request
    // (greater than _freqStep/2)
    float freqDiff = 1.0e6 * fabs(requestFreqMhz - newFreqMhz);
    if (freqDiff > (0.5 * _freqStep)) {
        WLOG << _oscName() << ": requested freq. " << requestFreqMhz <<
                " MHz, but got " << newFreqMhz << " MHz!";
    } else {
        ILOG << _oscName() << " frequency set to " << requestFreqMhz << "MHz";
    }
    _scaledCurrentFreq = rintf((1.0e6 * newFreqMhz) / _freqStep);

    // Wait briefly for lock and log the result
    _logLockedState();
}

void
QM2010_Oscillator::_logLockedState() {
    // Test for lock, waiting up to 10 ms for lock. Note that the oscillator
    // will only report as locked if RF output is enabled.
    bool locked = false;
    static const int LOCK_WAIT_MS = 10;
    for (int i = 0; i <= LOCK_WAIT_MS; i++) {
        locked = _sendCmdAndGetBoolReply("FREQ:LOCK?");
        if (locked) {
            ILOG << _oscName() << " output locked after " << i << " ms";
            break;
        }
        // Try again in 1 ms
        usleep(1000);
    }
    if (! locked) {
        ELOG << _oscName() << " did not lock within " << LOCK_WAIT_MS << "ms";
    }
}

void
QM2010_Oscillator::_sendCmd(const std::string & cmd) {
    // Send the command
    DLOG << _oscName() << ": sending command '" << cmd << "'";
    int nsent = write(_fd, cmd.c_str(), cmd.length());
    if (nsent != int(cmd.length())) {
        std::ostringstream os;
        os << _oscName() << ": Failed to send command '" << cmd <<
              "'";
        ELOG << os.str();
        throw(std::runtime_error(os.str()));
    }
}

std::string
QM2010_Oscillator::_sendCmdAndGetReply(const std::string & cmd) {
    // Send the command
    _sendCmd(cmd);

    // Read the reply
    char buf[128];
    int nread;
    do {
        nread = read(_fd, buf, sizeof(buf));
    } while (nread == -1 && errno == EAGAIN);

    if (nread == -1) {
        std::ostringstream os;
        os << _oscName() << ": reply read failure for cmd '" <<
              cmd << "': " << strerror(errno);
        ELOG << os.str();
        throw(std::runtime_error(os.str()));
    } else if (nread == 0) {
        return(std::string(""));
    }
    // Trim trailing whitespace and return the reply string
    std::string reply(buf, nread);
    reply.erase(reply.find_last_not_of(" \n\r\t") + 1);
    return(reply);
}

bool
QM2010_Oscillator::_sendCmdAndGetBoolReply(const std::string & cmd) {
    std::string reply = _sendCmdAndGetReply(cmd);
    bool badReply(false);
    
    int ival;
    try {
        ival = stol(reply);
    } catch (std::invalid_argument & e) {
        badReply = true;
    }

    badReply |= (ival != 0 && ival != 1);
    if (badReply) {
        std::ostringstream os;
        os << _oscName() << ": command '" << cmd << "' reply '" <<
              reply << "' cannot be parsed as bool";
        throw(std::runtime_error(os.str()));
    }
    return(bool(ival));
}

float
QM2010_Oscillator::_sendCmdAndGetFloatReply(const std::string & cmd) {
    std::string reply = _sendCmdAndGetReply(cmd);
    float fval;
    try {
        fval = stof(reply);
    } catch (std::invalid_argument & e) {
        std::ostringstream os;
        os << _oscName() << ": command '" << cmd << "' reply '" <<
              reply << "' cannot be parsed as float";
        throw(std::runtime_error(os.str()));
    }
    return(fval);
}

int
QM2010_Oscillator::_sendCmdAndGetIntReply(const std::string & cmd) {
    std::string reply = _sendCmdAndGetReply(cmd);
    int ival;
    try {
        ival = stoi(reply);
    } catch (std::invalid_argument & e) {
        std::ostringstream os;
        os << _oscName() << ": command '" << cmd << "' reply '" <<
              reply << "' cannot be parsed as int";
        throw(std::runtime_error(os.str()));
    }
    return(ival);
}
