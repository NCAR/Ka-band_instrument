/*
 * KaXmitter.h
 *
 *  Created on: Jan 7, 2011
 *      Author: burghart
 */

#ifndef KAXMITTER_H_
#define KAXMITTER_H_

#include <string>

class KaXmitter {
public:
    KaXmitter(std::string ttyDev);
    virtual ~KaXmitter();
    
    /**
     * Return a logical OR of all of the fault bits (i.e., true iff any of
     * the fault bits are true)
     * @return a logical OR of all of the fault bits (i.e., true iff any of
     * the fault bits are true)
     */
    bool getFaultSummary() const { return _faultSummary; }
    /**
     * Return true iff HVPS has been commanded ON. The value returned is from
     * the time of the last call to updateStatus().
     * @return true iff HVPS has been commanded ON
     */
    bool getHvpsRunup() const { return _hvpsRunup; }
    /**
     * Return true iff transmitter is ready for HV on. The value returned is from
     * the time of the last call to updateStatus().
     * @return true iff transmitter is ready for HV on
     */
    bool getStandby() const { return _standby; }
    /**
     * Return true iff heater 3-minute warmup is in progress. The value returned
     * is from the time of the last call to updateStatus().
     * @return true iff heater 3-minute warmup is in progress
     */
    bool getHeaterWarmup() const { return _heaterWarmup; }
    /**
     * Return true iff 3-minute transmitter cooldown period is in progress
     * @return true iff 3-minute transmitter cooldown period is in progress
     */
    bool getCooldown() const { return _cooldown; }
    /**
     * Return true iff transmitter contactor energized and unit not in cooldown.
     * The value returned is from the time of the last call to updateStatus().
     * @return true iff transmitter contactor energized and unit not in cooldown
     */
    bool getUnitOn() const { return _unitOn; }
    /**
     * Return true iff magnetron over-current detected. The value returned is 
     * from the time of the last call to updateStatus().
     * @return true iff magnetron over-current detected
     */
    bool getMagnetronCurrentFault() const { return _magnetronCurrentFault; }
    /**
     * Return true iff blower speed detector indicates that the blower is 
     * stopped. The value returned is from the time of the last call to 
     * updateStatus().
     * @return true iff blower speed detector indicates that the blower is
     * stopped
     */
    bool getBlowerFault() const { return _blowerFault; }
    /**
     * Return true iff all HV power supplies have cleared the preset low 
     * limits and HV is on. The value returned is from the time of the last call
     * to updateStatus().
     * @return true iff all HV power supplies have cleared the preset low 
     * limits and HV is on
     */
    bool getHvpsOn() const { return _hvpsOn; }
    /**
     * Return true iff the unit is ready to accept remote commands. The value
     * returned is from the time of the last call to updateStatus().
     * @return true iff the unit is ready to accept remote commands
     */
    bool getRemoteEnabled() const { return _remoteEnabled; }
    /**
     * Return true iff power transmitter door/cover is open. The value returned
     * is from the time of the last call to updateStatus().
     * @return true iff power transmitter door/cover is open
     */
    bool getSafetyInterlock() const { return _safetyInterlock; }
    /**
     * Return true iff VSWR greater than 2:1 load mismatch. The value returned
     * is from the time of the last call to updateStatus().
     * @return true iff VSWR greater than 2:1 load mismatch
     */
    bool getReversePowerFault() const { return _reversePowerFault; }
    /**
     * Return true iff transmitter input pulse has exceeded the duty cycle or 
     * PRF limits. The value returned is from the time of the last call to
     * updateStatus().
     * @return true iff transmitter input pulse has exceeded the duty cycle or 
     * PRF limits
     */
    bool getPulseInputFault() const { return _pulseInputFault; }
    /**
     * Return true iff HVPS protection has detected over-current. The value 
     * returned is from the time of the last call to updateStatus().
     * @return true iff HVPS protection has detected over-current
     */
    bool getHvpsCurrentFault() const { return _hvpsCurrentFault; }
    /**
     * Return true iff waveguide pressure is too low. The value returned is from
     * the time of the last call to updateStatus().
     * @return true iff waveguide pressure is too low
     */
    bool getWaveguidePressureFault() const { return _waveguidePressureFault; }
    /**
     * Return true iff HVPS voltage is too low. The value returned is from the 
     * time of the last call to updateStatus().
     * @return true iff HVPS voltage is too low
     */
    bool getHvpsUnderVoltage() const { return _hvpsUnderVoltage; }
    /**
     * Return true iff HVPS voltage is too high. The value returned is from the
     * time of the last call to updateStatus().
     * @return true iff HVPS voltage is too high
     */
    bool getHvpsOverVoltage() const { return _hvpsOverVoltage; }
    /**
     * Return HVPS voltage, in kV. The value returned is from the time of the
     * last call to updateStatus().
     * @return HVPS voltage, in kV
     */
    double getHvpsVoltage() const { return _hvpsVoltage; }
    /**
     * Return average magnetron current, in mA. The value returned is from the
     * time of the last call to updateStatus().
     * @return average magnetron current, in mA
     */
    double getMagnetronCurrent() const { return _magnetronCurrent; }
    /**
     * Return HVPS current, in mA. The value returned is from the time of the
     * last call to updateStatus().
     * @return HVPS current, in mA
     */
    double getHvpsCurrent() const { return _hvpsCurrent; }
    /**
     * Return transmitter temperature, in C. The value returned is from the time
     * of the last call to updateStatus().
     * @return transmitter temperature, in C
     */
    double getTemperature() const { return _temperature; }
    
    /**
     * Get current status values from the transmitter, updating our local
     * values.
     */
    void updateStatus();
    
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
    bool _faultSummary;
    bool _hvpsRunup;
    bool _standby;
    bool _heaterWarmup;
    bool _cooldown;
    bool _unitOn;
    bool _magnetronCurrentFault;
    bool _blowerFault;
    bool _hvpsOn;
    bool _remoteEnabled;
    bool _safetyInterlock;
    bool _reversePowerFault;
    bool _pulseInputFault;
    bool _hvpsCurrentFault;
    bool _waveguidePressureFault;
    bool _hvpsUnderVoltage;
    bool _hvpsOverVoltage;
    
    double _hvpsVoltage;        // kV
    double _magnetronCurrent;   // mA
    double _hvpsCurrent;        // mA
    double _temperature;        // C
    
    // Are we simulating?
    bool _simulate;
    
    // Our serial port device name (may be SIM_DEVICE)
    std::string _ttyDev;
    
    // File descriptor for the open serial port
    int _fd;
};

#endif /* KAXMITTER_H_ */
