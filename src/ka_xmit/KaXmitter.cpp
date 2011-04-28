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

// device name to use for simulated transmitter
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
        _simulate(ttyDev == SIM_DEVICE),
        _ttyDev(ttyDev),
        _fd(-1) {
    ILOG << "KaXmitter on device " << ttyDev;
    // Open the serial port
    if (! _simulate) {
        _openTty();
    }
    // Initialize our simulated status
    _initSimStatus();
}

KaXmitter::~KaXmitter() {
}

void
KaXmitter::powerOn() {
    ILOG << "Power On";
    if (! _simulate) {
        _sendCommand(_POWERON_COMMAND);
    } else {
        _simStatus.unitOn = true;
        _simStatus.standby = true;
    }
    return;
}

void
KaXmitter::powerOff() {
    ILOG << "Power Off";
    if (! _simulate) {
        _sendCommand(_POWEROFF_COMMAND);
    } else {
        _initSimStatus();
    }
    return;
}

void
KaXmitter::faultReset() {
    ILOG << "Fault Reset";
    if (! _simulate) {
        _sendCommand(_RESET_COMMAND);
    } else {
        if (! _simStatus.unitOn)
            return;
        
        _simStatus.faultSummary = false;
        _simStatus.blowerFault = false;
        _simStatus.hvpsCurrentFault = false;
        _simStatus.hvpsOverVoltage = false;
        _simStatus.hvpsUnderVoltage = false;
        _simStatus.magnetronCurrentFault = false;
        _simStatus.pulseInputFault = false;
        _simStatus.reversePowerFault = false;
        _simStatus.safetyInterlock = false;
    }
    return;
}

void
KaXmitter::standby() {
    ILOG << "Standby";
    if (! _simulate) {
        _sendCommand(_STANDBY_COMMAND);
    } else {
        if (! _simStatus.unitOn)
            return;
        
        _simStatus.hvpsOn = false;
        _simStatus.hvpsRunup = false;
        _simStatus.hvpsVoltage = 0.0;
        _simStatus.standby = true;
    }
    return;
}

void
KaXmitter::operate() {
    ILOG << "Operate";
    if (! _simulate) {
        _sendCommand(_OPERATE_COMMAND);
    } else {
        if (! _simStatus.unitOn)
            return;
        
        _simStatus.hvpsOn = true;
        _simStatus.hvpsRunup = true;
        _simStatus.hvpsVoltage = 20.0;
        _simStatus.standby = false;
    }
    return;
}

KaXmitStatus
KaXmitter::getStatus() {
    // Special handling if we're simulating...
    if (_simulate) {
        _simStatus.magnetronCurrent = 0.1 + (0.2 * random()) / RAND_MAX;
        _simStatus.hvpsCurrent = 0.2 + (0.2 * random()) / RAND_MAX;
        _simStatus.temperature = 30 + (2.0 * random()) / RAND_MAX;
        
        // If in operate mode, occasionally generate a pulse input fault
        if (_simStatus.hvpsRunup && (random() / float(RAND_MAX)) < 0.05) {
            _simStatus.pulseInputFault = true;
        }

        _simStatus.faultSummary = _simStatus.blowerFault ||
                _simStatus.hvpsCurrentFault|| _simStatus.hvpsOverVoltage ||
                _simStatus.hvpsUnderVoltage || _simStatus.magnetronCurrentFault ||
                _simStatus.pulseInputFault || _simStatus.reversePowerFault ||
                _simStatus.safetyInterlock;
        
        // If we simulated a fault, also simulate leaving operate mode
        if (_simStatus.faultSummary) {
            _simStatus.hvpsOn = false;
            _simStatus.hvpsRunup = false;
            _simStatus.hvpsVoltage = 0.0;
            _simStatus.standby = true;
        }
        
        return(_simStatus);
    }
    
    // This must be the real thing, so get status from the transmitter
    KaXmitStatus status;
    _clearStatus(status);
    
    // Get rid of any unread input
    tcflush(_fd, TCIFLUSH);
    
    // Try up to five times so send a status command and wait for *something*
    // to come back
    for (int attempt = 0; ; attempt++) {
        // Send the status command
        _sendCommand(_STATUS_COMMAND);
    
        // Wait up to a quarter second for reply to be ready
        if (_readSelect(250) == 0) {
            if (attempt > 0) {
                DLOG << "Took " << attempt + 1 << " tries to get status reply";
            }
            break;
        } else {
            if (attempt == 4) {
                WLOG << "No status reply in " << attempt + 1 << 
                    " tries; Is the transmitter plugged in?";
                return(status);
            }
        }
    }
    
    // Read the 21-byte status reply
    static const int REPLYSIZE = 21;
    char reply[REPLYSIZE];
    int nRead = 0;
    while (nRead < REPLYSIZE) {
        int result = read(_fd, reply + nRead, REPLYSIZE - nRead);
        if (result == 0) {
            DLOG << "Status reply read timeout. Trying again.";
            return getStatus();
        } else if (result < 0) {
            ELOG << "Status reply read error: " << strerror(errno);
            return(status);
        } else {
            nRead += result;
        }
    }
    
    // Validate the reply
    if (! _argValid(std::string(reply, REPLYSIZE))) {
        WLOG << ": Trying again after bad status reply";
        return(getStatus());
    }
    
    // Finally, parse the reply
    status.serialConnected = true;
    
    // Six used bits in the first status byte
    status.faultSummary = (reply[1] >> 5) & 0x1;
    status.hvpsRunup = (reply[1] >> 4) & 0x1;
    status.standby = (reply[1] >> 3) & 0x1;
    status.heaterWarmup = (reply[1] >> 2) & 0x1;
    status.cooldown = (reply[1] >> 1) & 0x1;
    status.unitOn = (reply[1] >> 0) & 0x1;
    DLOG << "flt summary " << status.faultSummary << 
        ", runup " << status.hvpsRunup << 
        ", standby " << status.standby << 
        ", warmup " << status.heaterWarmup << 
        ", cooldown " << status.cooldown << 
        ", unit on " << status.unitOn;
    
    // Six used bits in the second status byte
    status.magnetronCurrentFault = (reply[2] >> 5) & 0x1;
    status.blowerFault = (reply[2] >> 4) & 0x1;
    status.hvpsOn = (reply[2] >> 3) & 0x1;
    status.remoteEnabled = (reply[2] >> 2) & 0x1;
    status.safetyInterlock = (reply[2] >> 1) & 0x1;
    status.reversePowerFault = (reply[2] >> 0) & 0x1;
    DLOG << "mag cur flt " << status.magnetronCurrentFault << 
        ", blower flt " << status.blowerFault << 
        ", hvpsOn " << status.hvpsOn <<
        ", remote ena " << status.remoteEnabled << 
        ", safety " << status.safetyInterlock <<
        ", reverse pwr flt " << status.reversePowerFault;
    
    // Five used bits in the third status byte
    status.pulseInputFault = (reply[3] >> 5) & 0x1;
    status.hvpsCurrentFault = (reply[3] >> 3) & 0x1;
    status.waveguidePressureFault = (reply[3] >> 2) & 0x1;
    status.hvpsUnderVoltage = (reply[3] >> 1) & 0x1;
    status.hvpsOverVoltage = (reply[3] >> 0) & 0x1;
    DLOG << "pulse flt " << status.pulseInputFault << 
        ", HVPS cur flt " << status.hvpsCurrentFault <<
        ", wg pres flt " << status.waveguidePressureFault <<
        ", HVPS underV " << status.hvpsUnderVoltage <<
        ", HVPS overV " << status.hvpsOverVoltage;
    
    // 4-character HVPS voltage starting at character 4, e.g., " 0.0"
    if (sscanf(reply + 4, "%4lf", &status.hvpsVoltage) != 1) {
        WLOG << __PRETTY_FUNCTION__ << ": Bad HVPS voltage in status!";
    } else {
        DLOG << "HVPS voltage " << status.hvpsVoltage << " kV";
    }
        
    // 4-character magnetron current starting at character 8, e.g., " 0.2"
    if (sscanf(reply + 8, "%4lf", &status.magnetronCurrent) != 1) {
        WLOG << "Bad magnetron current in status!";
    } else {
        DLOG << "magnetron current " << status.magnetronCurrent << " mA";
    }
        
    // 4-character HVPS current starting at character 12, e.g., " 0.2"
    if (sscanf(reply + 12, "%4lf", &status.hvpsCurrent) != 1) {
        WLOG << "Bad HVPS current in status!";
    } else {
        DLOG << "HVPS current " << status.hvpsCurrent << " mA";
    }
    
    // 3-character temperature starting at character 16, e.g., "+27."
    if (sscanf(reply+16, "%4lf", &status.temperature) != 1) {
        WLOG << "Bad temperature in status!";
    } else {
        DLOG << "transmitter temperature " << status.temperature << " C";
    }
    
    return(status);
}

void
KaXmitter::reopenTty() {
    ILOG << "Re-opening " << _ttyDev;
    close(_fd);
    _fd = -1;
    _openTty();
}

void
KaXmitter::_openTty() {
    DLOG << "Opening " << _ttyDev;
    if ((_fd = open(_ttyDev.c_str(), O_RDWR)) == -1) {
        ELOG << __PRETTY_FUNCTION__ << ": error opening " << _ttyDev << ": " <<
                strerror(errno);
        exit(1);
    }

    // Make the port 9600 8N1, "raw"
    struct termios ios;
    if (tcgetattr(_fd, &ios) == -1) {
        ELOG << __PRETTY_FUNCTION__ << ": error getting " << _ttyDev << 
                " attributes: " << strerror(errno);
        exit(1);
    }
    cfmakeraw(&ios);
    cfsetspeed(&ios, B9600);

    // Set a 0.2 second timeout for reads
    ios.c_cc[VMIN] = 0;
    ios.c_cc[VTIME] = 2;    // 0.2 seconds

    if (tcsetattr(_fd, TCSAFLUSH, &ios) == -1) {
        ELOG << __PRETTY_FUNCTION__ << ": error setting " << _ttyDev << 
                " attributes: " << strerror(errno);
        exit(1);
    }
    DLOG << "Done configuring " << _ttyDev;
}

void
KaXmitter::_clearStatus(KaXmitStatus & status) {
    memset(&status, 0, sizeof(KaXmitStatus));
}

void
KaXmitter::_sendCommand(std::string cmd) {
    if (_simulate)
        return;
    
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
    // Exit if we fail to get the command through after many attempts...
    exit(1);
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

            ELOG << __PRETTY_FUNCTION__ << ": select error: " <<
                    strerror(errno);
            return -2; // select failed
        } 
    }
}

void
KaXmitter::_initSimStatus() {
    _clearStatus(_simStatus);
    _simStatus.serialConnected = true;
    _simStatus.remoteEnabled = true;
}
