/*
 * KaXmitter.h
 *
 *  Created on: Jan 7, 2011
 *      Author: burghart
 */

#ifndef KAXMITTER_H_
#define KAXMITTER_H_

#include <string>

struct KaXmitStatus {
    bool serialConnected;
    bool faultSummary;
    bool hvpsRunup;
    bool standby;
    bool heaterWarmup;
    bool cooldown;
    bool unitOn;
    bool magnetronCurrentFault;
    bool blowerFault;
    bool hvpsOn;
    bool remoteEnabled;
    bool safetyInterlock;
    bool reversePowerFault;
    bool pulseInputFault;
    bool hvpsCurrentFault;
    bool waveguidePressureFault;
    bool hvpsUnderVoltage;
    bool hvpsOverVoltage;
    
    double hvpsVoltage;        // kV
    double magnetronCurrent;   // mA
    double hvpsCurrent;        // mA
    double temperature;        // C
};

class KaXmitter {
public:
    /**
     * Construct a KaXmitter providing access to the Ka-band transmitter on
     * the given serial port. If special serial port name KaXmitter::SIM_DEVICE
     * is used, existence of the transmitter will be simulated.
     * @param ttyDev the name of the serial port connected to the transmitter.
     */
    KaXmitter(std::string ttyDev);
    virtual ~KaXmitter();
    
    /**
     * Get current status values from the transmitter.
     */
    KaXmitStatus getStatus(unsigned int recursion = 0);
    
    /**
     * Turn on the transmitter unit (does not enable high voltage and actual
     * transmission). A warmup period is required before high voltage can be
     * enabled.
     */
    void powerOn();
    
    /**
     * Turn off the transmitter unit. After power is turned off, the unit will
     * stay up in a cooldown mode for three minutes before all power is 
     * removed.
     */
    void powerOff();
    
    /**
     * Reset all faults conditions.
     */
    void faultReset();
    
    /**
     * Enter "standby" state (ready to operate, but high voltage disabled).
     */
    void standby();
    
    /**
     * Enter "operate" state, with high voltage enabled and ready to transmit
     * when triggered.
     */
    void operate();
    
    /**
     * Reopen our serial port talking to the transmitter. Generally this should
     * be called any time the serial port on the transmitter has been reset.
     */
    void reopenTty();
    
    /**
     * Device name to use when creating a simulation KaXmitter.
     */
    static const std::string SIM_DEVICE;
        
private:
    /**
     * Open and configure our tty connection to the transmitter
     */
    void _openTty();
    
    /**
     * Send a command to the transmitter.
     */
    void _sendCommand(std::string cmd);
    
    /**
     * Is the argument string (command or reply) valid?
     * @return true iff the argument string is valid, including its checksum
     */
    bool _argValid(std::string arg);
    
    /**
     * Wait for input on our file descriptor, with a timeout specified in
     * milliseconds. 
     * @return 0 when input is ready, -1 if the select timed out, -2 on
     *      select error
     */
    int _readSelect(unsigned int timeoutMsecs);
    
    /**
     * Fill a KaXmitStatus struct with 0.0/false values.
     * @param status the KaXmitStatus to be cleared
     */
    void _clearStatus(KaXmitStatus & status);
    
    /**
     * Initialize the simulated status struct.
     */
    void _initSimStatus();
    
    // Command strings for the transmitter
    static const std::string _OPERATE_COMMAND;
    static const std::string _STANDBY_COMMAND;
    static const std::string _RESET_COMMAND;
    static const std::string _POWERON_COMMAND;
    static const std::string _POWEROFF_COMMAND;
    static const std::string _STATUS_COMMAND;
    
    // Are we simulating?
    bool _simulate;
    
    // Keep a local status struct to use when simulating.
    KaXmitStatus _simStatus;
    
    // Our serial port device name (may be SIM_DEVICE)
    std::string _ttyDev;
    
    // File descriptor for the open serial port
    int _fd;
};

#endif /* KAXMITTER_H_ */
