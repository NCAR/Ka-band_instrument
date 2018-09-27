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
                                     uint32_t oscNum,
                                     uint32_t refFreqMhz,
                                     uint32_t freqStep,
                                     uint32_t scaledMinFreq,
                                     uint32_t scaledMaxFreq) :
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

    // Turn off oscillator output while we configure
    bool rfOn = _sendCmdAndGetBoolReply("POWER:RF OFF; POWER:RF?");
    if (rfOn) {
        std::ostringstream os;
        os << _oscName() << ": attempt to disable output failed!";
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

    // Get the current frequency (returned in MHz) and calculate
    // _scaledCurrentFreq in units of _freqStep
    float currentFreqMhz = _sendCmdAndGetFloatReply("FREQ:RETACT?");
    _scaledCurrentFreq = rintf((1.0e6 * currentFreqMhz) / _freqStep);
    ILOG << _oscName() << " started at " << currentFreqMhz << " MHz";

    // XXX Bogus frequency setting for testing
    unsigned int seed;
    getentropy(&seed, sizeof(seed));
    srandom(seed);
    int iGhz = int(4 * (double(random()) / RAND_MAX) + 1);
    setScaledFreq(iGhz * 10000 + (time(0) % 60));

    // Note if the oscillator's startup frequency is not between the specified
    // min and max.
    if ((_scaledCurrentFreq < _scaledMinFreq) ||
        (_scaledCurrentFreq > _scaledMaxFreq)) {
        WLOG << _oscName() <<
            " startup scaled freq (" << _scaledCurrentFreq <<
            ") is not between given min (" << _scaledMinFreq << ") and max (" <<
            _scaledMaxFreq << ")";
    }
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

    // Get requested frequency in MHz
    float requestFreqMhz = (_freqStep * scaledFreq) * 1.0e-6;

    // Nothing to do if they requested the current frequency...
    if (scaledFreq == _scaledCurrentFreq) {
        ILOG << _oscName() << " already at " << 
                (scaledFreq * _freqStep) * 1.0e-6 << " MHz";
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
        DLOG << _oscName() << " frequency set to " << requestFreqMhz << "MHz";
    }
    _scaledCurrentFreq = rintf((1.0e6 * newFreqMhz) / _freqStep);
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
    reply.erase(reply.find_last_not_of("\s"));
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
