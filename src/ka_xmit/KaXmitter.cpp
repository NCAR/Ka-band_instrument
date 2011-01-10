/*
 * KaXmitter.cpp
 *
 *  Created on: Jan 7, 2011
 *      Author: burghart
 */

#include "KaXmitter.h"

#include <iostream>
#include <iomanip>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <termios.h>
#include <fcntl.h>

#include <logx/Logging.h>

LOGGING("KaXmitter")

const std::string KaXmitter::SIM_DEVICE = "SimulatedKaXmitter";

// operate command: STX 'O' ETX 0x2e
const std::string KaXmitter::_OPERATE_COMMAND = "\x02\x4f\x03\x2e";

// standby command: STX 'S' ETX 0x2a
const std::string KaXmitter::_STANDBY_COMMAND = "\x02\x53\x03\x2a";

// reset command: STX 'R' ETX 0x2a
const std::string KaXmitter::_RESET_COMMAND = "\x02\x52\x03\x2b";

// power on command: STX 'P' ETX 0x2d
const std::string KaXmitter::_POWERON_COMMAND = "\x02\x50\x03\x2d";

// power off command: STX 'p' ETX 0x0d
const std::string KaXmitter::_POWEROFF_COMMAND = "\x02\x70\x03\x0d";

// status command: STX 'W' ETX 0x26
const std::string KaXmitter::_STATUS_COMMAND = "\x02\x57\x03\x26";



KaXmitter::KaXmitter(std::string ttyDev) :
        _faultSummary(false),
        _hvpsRunup(false),
        _standby(false),
        _heaterWarmup(false),
        _cooldown(false),
        _unitOn(false),
        _magnetronCurrentFault(false),
        _blowerFault(false),
        _hvpsOn(false),
        _remoteEnabled(false),
        _safetyInterlock(false),
        _reversePowerFault(false),
        _pulseInputFault(false),
        _hvpsCurrentFault(false),
        _waveguidePressureFault(false),
        _hvpsUnderVoltage(false),
        _hvpsOverVoltage(false),
        _hvpsVoltage(0.0),
        _magnetronCurrent(0.0),
        _hvpsCurrent(0.0),
        _temperature(0.0),
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
        
        // Set a 0.2 second timeout for reads
        ios.c_cc[VMIN] = 0;
        ios.c_cc[VTIME] = 2;    // 0.2 seconds

        if (tcsetattr(_fd, TCSAFLUSH, &ios) == -1) {
            ELOG << __PRETTY_FUNCTION__ << ": error setting " << _ttyDev << 
                    " attributes: " << strerror(errno);
            abort();
        }
    }
    // Get current status
    _getStatus();
}

KaXmitter::~KaXmitter() {
}


void
KaXmitter::_getStatus() {
    if (_simulate)
        return;
    
    // Get rid of any unread input
    tcflush(_fd, TCIFLUSH);
    
    // Send the status command
    _sendCommand(_STATUS_COMMAND);
    
    // Wait up to a second for reply to be ready
    if (_readSelect(1000) < 0) {
        abort();
    }
    
    // Read the 21-byte status reply
    static const int REPLYSIZE = 21;
    char reply[REPLYSIZE];
    int nRead = 0;
    while (nRead < REPLYSIZE) {
        int result = read(_fd, reply + nRead, REPLYSIZE - nRead);
        if (result == 0) {
            WLOG << __PRETTY_FUNCTION__ << ": reply read timeout. Trying again.";
            return _getStatus();
        } else if (result < 0) {
            ELOG << __PRETTY_FUNCTION__ << ": read error: " << strerror(errno);
            abort();
        } else {
            nRead += result;
        }
    }
    
    // Validate the reply
    if (! _argValid(std::string(reply, REPLYSIZE))) {
        WLOG << __PRETTY_FUNCTION__ << ": Trying again after bad reply";
        return _getStatus();
    }
    
    // Finally, parse the reply
    
    // Six used bits in the first status byte
    _faultSummary = (reply[1] >> 5) & 0x1;
    _hvpsRunup = (reply[1] >> 4) & 0x1;
    _standby = (reply[1] >> 3) & 0x1;
    _heaterWarmup = (reply[1] >> 2) & 0x1;
    _cooldown = (reply[1] >> 1) & 0x1;
    _unitOn = (reply[1] >> 0) & 0x1;
    
    // Six used bits in the second status byte
    _magnetronCurrentFault = (reply[2] >> 5) & 0x1;
    _blowerFault = (reply[2] >> 4) & 0x1;
    _hvpsOn = (reply[2] >> 3) & 0x1;
    _remoteEnabled = (reply[2] >> 2) & 0x1;
    _safetyInterlock = (reply[2] >> 1) & 0x1;
    _reversePowerFault = (reply[2] >> 0) & 0x1;
    
    // Five used bits in the third status byte
    _pulseInputFault = (reply[3] >> 5) & 0x1;
    _hvpsCurrentFault = (reply[3] >> 3) & 0x1;
    _waveguidePressureFault = (reply[3] >> 2) & 0x1;
    _hvpsUnderVoltage = (reply[3] >> 1) & 0x1;
    _hvpsOverVoltage = (reply[3] >> 0) & 0x1;
}

void
KaXmitter::_sendCommand(std::string cmd) {
    // Sanity check on command
    if (! _argValid(cmd)) {
        abort();
    }
    // Try up to five times to send all of the chars out
    int nSent = 0;
    int nToSend = cmd.length();
    for (int attempt = 0; attempt < 5; attempt++) {
        if (attempt > 0) {
            ILOG << __PRETTY_FUNCTION__ << ": Attempt " << attempt + 1 << 
                    " to send '" << cmd[1] << "' command.";
        }
        int result = write(_fd, cmd.c_str() + nSent, nToSend);
        if (result == -1) {
            WLOG << __PRETTY_FUNCTION__ << ": Error (" << strerror(errno) <<
                    ") sending '" << cmd[1] << "' command.";
        } else {
            nToSend -= result;
            nSent += result;
        }
        if (nToSend == 0)
            return;
        sleep(1);
    }
    // Abort if we fail to get the command through after many attempts...
    abort();
}

bool
KaXmitter::_argValid(std::string arg) {
    // Create an external representation of the argument, e.g.,
    // "0x02 0x57 0x03 0x26". We'll use this for error messages.
    std::ostringstream ss;
    ss << std::setfill('0') << std::setw(2) << std::hex;
    for (unsigned int i = 0; i < arg.length(); i++) {
        if (i > 0)
            ss << " ";
        ss << "0x" << arg[i];
    }
    // Arg must be at least 4 characters long
    if (arg.length() < 4) {
        
        DLOG << __PRETTY_FUNCTION__ << ": arg '" << ss.str() << 
                "' too short; it must be at least 4 characters long";
        return false;
    }
    // First character of arg must be STX
    if (arg[0] != '\x02') {
        DLOG << __PRETTY_FUNCTION__ << ": first character of arg '" << 
                ss.str() << "' is not STX (0x02)";
        return false;
    }
    // Next-to-last character of arg must be ETX
    if (arg[arg.length() - 2] != '\x03') {
        DLOG << __PRETTY_FUNCTION__ << ": last character before checksum in" <<
                " arg '" << ss.str() << "' is not ETX (0x03)";
    }
    // Sum of bytes after STX must be zero (in lowest 7 bits)
    int cksum = 0;
    for (unsigned int i = 1; i < arg.length(); i++) {
        cksum += arg[i];
    }
    if ((cksum & 0x7f) != 0) {
        DLOG << __PRETTY_FUNCTION__ << ": bad checksum in arg '" << ss.str() <<
                "'";
        return false; 
    }
    // Looks OK!
    return true;
}

int 
KaXmitter::_readSelect(unsigned int waitMsecs)
{
    /*
     * check only on fd file descriptor
     */
    fd_set readFds;
    FD_ZERO(&readFds);
    FD_SET(_fd, &readFds);

    while (true) {
        /*
         * set timeval structure
         */
        struct timeval wait;
        wait.tv_sec = waitMsecs / 1000;
        wait.tv_usec = (waitMsecs % 1000) * 1000;

        int nready = select(_fd + 1, &readFds, NULL, NULL, &wait);
        if (nready == 1) {
            return 0;
        } else if (nready == 0) {
            return -1;      // timeout
        } else {
            if (errno == EINTR) /* system call was interrupted */
                continue;

            ELOG << __PRETTY_FUNCTION__ << ": Read select error: " <<
                    strerror(errno);
            return -2; // select failed
        } 
    }
}
