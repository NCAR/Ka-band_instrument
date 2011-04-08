/*
 * XmitClient.h
 *
 *  Created on: Mar 11, 2011
 *      Author: burghart
 */

#ifndef XMITCLIENT_H_
#define XMITCLIENT_H_

#include <XmlRpc.h>
#include <string>

/**
 * XmitClient encapsulates an XML-RPC connection to a ka_xmitd daemon 
 * process which is controlling the Ka-band transmitter.
 */
class XmitClient : private XmlRpc::XmlRpcClient {
public:
    /**
     * Instantiate XmitClient to communicate with a ka_xmitd process running
     * on host xmitdHost and using port xmitdPort.
     * @param xmitdHost the name of the host on which ka_xmitd is running
     * @param xmitdPort the port number being used by ka_xmitd
     */
    XmitClient(std::string xmitdHost, int xmitdPort);
    ~XmitClient();
    
    /// XmitStatus is a class encapsulating all status values available from the
    /// Ka-band transmitter.
    class XmitStatus {
    public:
        /**
         * Default constructor creates an instance with all boolean members set to
         * false, int members set to -1, and double members set to -9999.9.
         */
        XmitStatus();
        /**
         * Construct using status values extracted from the given 
         * XmlRpc::XmlRpcValue dictionary, which should come from an XML-RPC
         * getStatus() call to ka_xmitd.
         */
        XmitStatus(XmlRpc::XmlRpcValue & statusDict);
        /**
         * Destructor
         */
        virtual ~XmitStatus();

        /**
         * Does ka_xmitd have a serial connection to the transmitter?
         * @return true iff ka_xmitd has a serial connection to the transmitter.
         */
        bool serialConnected() const { return(_serialConnected); }
        /**
         * Is the transmitter reporting any faults?
         * @return true iff the transmitter is reporting one or more faults
         */
        bool faultSummary() const { return(_faultSummary); }
        /**
         * Is high-voltage enabled?
         * @return true iff the HV voltage power supply has been commanded ON.
         */
        bool hvpsRunup() const { return(_hvpsRunup); }
        /**
         * Is the transmitter ready for high voltage to be applied?
         * @return true iff the transmitter is ready for HV on.
         */
        bool standby() const { return(_standby); }
        /**
         * Is the heater in its 30-minute warmup period?
         * @return true iff the heater is in its 30-minute warmup period.
         */
        bool heaterWarmup() const { return(_heaterWarmup); }
        /**
         * Is the transmitter in its 3-minute cooldown period?
         * @return true iff the transmitter is in its 3-minute cooldown period.
         */
        bool cooldown() const { return(_cooldown); }
        /**
         * Is transmitter power on?
         * @return true iff transmitter power is on.
         */
        bool unitOn() const { return(_unitOn); }
        /**
         * Magnetron over-current state.
         * @return true iff magentron over-current is detected.
         */
        bool magnetronCurrentFault() const { return(_magnetronCurrentFault); }
        /**
         * Blower fault state.
         * @return true iff a blower fault is detected.
         */
        bool blowerFault() const { return(_blowerFault); }
        /**
         * High voltage power supply state.
         * @return true iff the high voltage is on.
         */
        bool hvpsOn() const { return(_hvpsOn); }
        /**
         * Remote control state.
         * @return true iff remote control is enabled.
         */
        bool remoteEnabled() const { return(_remoteEnabled); }
        /**
         * Safety interlock state.
         * @return true iff power transmitter door/cover is open.
         */
        bool safetyInterlock() const { return(_safetyInterlock); }
        /**
         * Reverse power fault state.
         * @return true iff VSWR shows greater than 2:1 load mismatch.
         */
        bool reversePowerFault() const { return(_reversePowerFault); }
        /**
         * Pulse input fault state.
         * @return true iff the transmitter input pulse has exceeded the duty 
         * cycle or PRF limits.
         */
        bool pulseInputFault() const { return(_pulseInputFault); }
        /**
         * HVPS current fault state.
         * @return true iff HVPS protection has detected over-current.
         */
        bool hvpsCurrentFault() const { return(_hvpsCurrentFault); }
        /**
         * Waveguide pressure fault state. (This applies to SF6 pressure in
         * waveguide inside the transmitter)
         * @return true iff SF6 pressure in the waveguide (within the 
         * transmitter) is too low.
         */
        bool waveguidePressureFault() const { return(_waveguidePressureFault); }
        /**
         * HVPS under voltage state.
         * @return true iff HVPS voltage is too low.
         */
        bool hvpsUnderVoltage() const { return(_hvpsUnderVoltage); }
        /**
         * HVPS over voltage state.
         * @return true iff HVPS voltage is too high.
         */
        bool hvpsOverVoltage() const { return(_hvpsOverVoltage); }
        /**
         * HVPS voltage.
         * @return HVPS voltage, in kV.
         */
        double hvpsVoltage() const { return(_hvpsVoltage); }
        /**
         * Magnetron current.
         * @return magnetron current, in mA
         */
        double magnetronCurrent() const { return(_magnetronCurrent); }
        /**
         * HVPS current.
         * @return HVPS current, in mA
         */
        double hvpsCurrent() const { return(_hvpsCurrent); }
        /**
         * Transmitter temperature.
         * @return transmitter temperature, in C
         */
        double temperature() const { return(_temperature); }

        /**
         * Number of magnetron current faults since ka_xmitd startup.
         * @return the number of magnetron current faults since ka_xmitd 
         * startup.
         */
        int magnetronCurrentFaultCount() const { return(_magnetronCurrentFaultCount); }
        /**
         * Number of blower faults since ka_xmitd startup.
         * @return the number of blower faults since ka_xmitd 
         * startup.
         */
        int blowerFaultCount() const { return(_blowerFaultCount); }
        /**
         * Number of safety interlock faults since ka_xmitd startup.
         * @return the number of safety interlock faults since ka_xmitd 
         * startup.
         */
        int safetyInterlockCount() const { return(_safetyInterlockCount); }
        /**
         * Number of reverse power faults since ka_xmitd startup.
         * @return the number of reverse power faults since ka_xmitd 
         * startup.
         */
        int reversePowerFaultCount() const { return(_reversePowerFaultCount); }
        /**
         * Number of pulse input faults since ka_xmitd startup.
         * @return the number of pulse input faults since ka_xmitd 
         * startup.
         */
        int pulseInputFaultCount() const { return(_pulseInputFaultCount); }
        /**
         * Number of HVPS current faults since ka_xmitd startup.
         * @return the number of HVPS current faults since ka_xmitd 
         * startup.
         */
        int hvpsCurrentFaultCount() const { return(_hvpsCurrentFaultCount); }
        /**
         * Number of waveguide pressure faults since ka_xmitd startup.
         * @return the number of waveguide pressure faults since ka_xmitd 
         * startup.
         */
        int waveguidePressureFaultCount() const { return(_waveguidePressureFaultCount); }
        /**
         * Number of HVPS under-voltage faults since ka_xmitd startup.
         * @return the number of HVPS under-voltage faults since ka_xmitd 
         * startup.
         */
        int hvpsUnderVoltageCount() const { return(_hvpsUnderVoltageCount); }
        /**
         * Number of HVPS over-voltage faults since ka_xmitd startup.
         * @return the number of HVPS over-voltage faults since ka_xmitd 
         * startup.
         */
        int hvpsOverVoltageCount() const { return(_hvpsOverVoltageCount); }
        /**
         * Unix time of last magnetron current fault since ka_xmitd startup.
         * @return the Unix time of the last magnetron current fault since ka_xmitd 
         * startup.
         */
        int magnetronCurrentFaultTime() const { return(_magnetronCurrentFaultTime); }
        /**
         * Unix time of last blower fault since ka_xmitd startup.
         * @return the Unix time of the last blower fault since ka_xmitd 
         * startup.
         */
        int blowerFaultTime() const { return(_blowerFaultTime); }
        /**
         * Unix time of last safety interlock fault since ka_xmitd startup.
         * @return the Unix time of the last safety interlock fault since ka_xmitd 
         * startup.
         */
        int safetyInterlockTime() const { return(_safetyInterlockTime); }
        /**
         * Unix time of last reverse power fault since ka_xmitd startup.
         * @return the Unix time of the last reverse power fault since ka_xmitd 
         * startup.
         */
        int reversePowerFaultTime() const { return(_reversePowerFaultTime); }
        /**
         * Unix time of last pulse input fault since ka_xmitd startup.
         * @return the Unix time of the last pulse input fault since ka_xmitd 
         * startup.
         */
        int pulseInputFaultTime() const { return(_pulseInputFaultTime); }
        /**
         * Unix time of last HVPS current fault since ka_xmitd startup.
         * @return the Unix time of the last HVPS current fault since ka_xmitd 
         * startup.
         */
        int hvpsCurrentFaultTime() const { return(_hvpsCurrentFaultTime); }
        /**
         * Unix time of last waveguide pressure fault since ka_xmitd startup.
         * @return the Unix time of the last waveguide pressure fault since ka_xmitd 
         * startup.
         */
        int waveguidePressureFaultTime() const { return(_waveguidePressureFaultTime); }
        /**
         * Unix time of last HVPS under-voltage fault since ka_xmitd startup.
         * @return the Unix time of the last HVPS under-voltage fault since ka_xmitd 
         * startup.
         */
        int hvpsUnderVoltageTime() const { return(_hvpsUnderVoltageTime); }
        /**
         * Unix time of last HVPS over-voltage fault since ka_xmitd startup.
         * @return the Unix time of the last HVPS over-voltage fault since ka_xmitd 
         * startup.
         */
        int hvpsOverVoltageTime() const { return(_hvpsOverVoltageTime); }
        /**
         * Number of auto pulse fault resets by ka_xmitd since startup.
         * @return the number of auto pulse fault resets by ka_xmitd since
         * startup.
         */
        int autoPulseFaultResets() const { return(_autoPulseFaultResets); }

    private:
        static bool _StatusBool(XmlRpc::XmlRpcValue statusDict, std::string key);
        static int _StatusInt(XmlRpc::XmlRpcValue statusDict, std::string key);
        static double _StatusDouble(XmlRpc::XmlRpcValue statusDict, std::string key);

        bool _serialConnected;
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

        int _magnetronCurrentFaultCount;
        int _blowerFaultCount;
        int _safetyInterlockCount;
        int _reversePowerFaultCount;
        int _pulseInputFaultCount;
        int _hvpsCurrentFaultCount;
        int _waveguidePressureFaultCount;
        int _hvpsUnderVoltageCount;
        int _hvpsOverVoltageCount;
        
        int _autoPulseFaultResets;

        time_t _magnetronCurrentFaultTime;
        time_t _blowerFaultTime;
        time_t _safetyInterlockTime;
        time_t _reversePowerFaultTime;
        time_t _pulseInputFaultTime;
        time_t _hvpsCurrentFaultTime;
        time_t _waveguidePressureFaultTime;
        time_t _hvpsUnderVoltageTime;
        time_t _hvpsOverVoltageTime;

        double _hvpsVoltage;
        double _magnetronCurrent;
        double _hvpsCurrent;
        double _temperature;
    };
    
    /// Send an "operate" command to the transmitter
    void operate();
    
    /// Send a "powerOn" command to the transmitter
    void powerOn();
    
    /// Send a "powerOff" command to the transmitter
    void powerOff();
    
    /// Send a "standby" command to the transmitter
    void standby();
    
    /// Send a "faultReset" command to the transmitter
    void faultReset();
    
    // Send a "getStatus" command, filling a XmitClient::XmitStatus object if
    // we get status from the ka_xmitd.
    // @param status the XmitClient::XmitStatus object to be filled
    // @return true and fill the status object if status is obtained from 
    // ka_xmitd, otherwise return false and leave the status object 
    // unmodified.
    bool getStatus(XmitClient::XmitStatus & status);
    
    /**
     * Get the port number of the associated ka_xmitd.
     * @return the port number of the associated ka_xmitd.
     */
    int getXmitdPort() { return(_xmitdPort); }
    
    /**
     * Get the name of the host on which the associated ka_xmitd is running.
     * @return the name of the host on which the associated ka_xmitd is running.
     */
    std::string getXmitdHost() { return(_xmitdHost); }
    
    /**
     * Get log messages from the associated ka_xmitd at and after a selected
     * index.
     * @param firstIndex[in] the first message index to include in the returned
     * log messages
     * @param msgs[out] all log messages at or later than the selected start index
     * will be appended to msgs
     * @param nextLogIndex[out] the index of the next log message after the last
     * available message is returned in nextLogIndex
     */
    void getLogMessages(unsigned int firstIndex, std::string & msgs, 
            unsigned int  & nextLogIndex);
private:
    /**
     * Execute an XML-RPC command in ka_xmitd and get the result.
     * @param cmd the XML-RPC command to execute
     * @param params XmlRpc::XmlRpcValue list of parameters for the command
     * @param result reference to XmlRpc::XmlRpcValue to hold the command result
     * @return true iff the command was executed successfully
     */
    bool _executeXmlRpcCommand(const std::string cmd, 
        const XmlRpc::XmlRpcValue & params, XmlRpc::XmlRpcValue & result);
    
    std::string _xmitdHost;
    int _xmitdPort;
};

#endif /* XMITCLIENT_H_ */
