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
    KaXmitter(std::string ttyDev);
    virtual ~KaXmitter();
    
    /**
     * Get current status values from the transmitter, updating our local
     * values.
     */
    const KaXmitStatus & getStatus();
    
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
     * Device name to use when creating a simulation KaXmitter.
     */
    static const std::string SIM_DEVICE;
        
private:
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
    
    // Command strings for the transmitter
    static const std::string _OPERATE_COMMAND;
    static const std::string _STANDBY_COMMAND;
    static const std::string _RESET_COMMAND;
    static const std::string _POWERON_COMMAND;
    static const std::string _POWEROFF_COMMAND;
    static const std::string _STATUS_COMMAND;
    
    // The last status values obtained from the transmitter
    KaXmitStatus _status;
    
    // Are we simulating?
    bool _simulate;
    
    // Our serial port device name (may be SIM_DEVICE)
    std::string _ttyDev;
    
    // File descriptor for the open serial port
    int _fd;
};

#endif /* KAXMITTER_H_ */
