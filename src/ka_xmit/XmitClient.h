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

        bool serialConnected() const { return(_serialConnected); }
        bool faultSummary() const { return(_faultSummary); }
        bool hvpsRunup() const { return(_hvpsRunup); }
        bool standby() const { return(_standby); }
        bool heaterWarmup() const { return(_heaterWarmup); }
        bool cooldown() const { return(_cooldown); }
        bool unitOn() const { return(_unitOn); }
        bool magnetronCurrentFault() const { return(_magnetronCurrentFault); }
        bool blowerFault() const { return(_blowerFault); }
        bool hvpsOn() const { return(_hvpsOn); }
        bool remoteEnabled() const { return(_remoteEnabled); }
        bool safetyInterlock() const { return(_safetyInterlock); }
        bool reversePowerFault() const { return(_reversePowerFault); }
        bool pulseInputFault() const { return(_pulseInputFault); }
        bool hvpsCurrentFault() const { return(_hvpsCurrentFault); }
        bool waveguidePressureFault() const { return(_waveguidePressureFault); }
        bool hvpsUnderVoltage() const { return(_hvpsUnderVoltage); }
        bool hvpsOverVoltage() const { return(_hvpsOverVoltage); }

        int magnetronCurrentFaultCount() const { return(_magnetronCurrentFaultCount); }
        int blowerFaultCount() const { return(_blowerFaultCount); }
        int safetyInterlockCount() const { return(_safetyInterlockCount); }
        int reversePowerFaultCount() const { return(_reversePowerFaultCount); }
        int pulseInputFaultCount() const { return(_pulseInputFaultCount); }
        int hvpsCurrentFaultCount() const { return(_hvpsCurrentFaultCount); }
        int waveguidePressureFaultCount() const { return(_waveguidePressureFaultCount); }
        int hvpsUnderVoltageCount() const { return(_hvpsUnderVoltageCount); }
        int hvpsOverVoltageCount() const { return(_hvpsOverVoltageCount); }
        int autoPulseFaultResets() const { return(_autoPulseFaultResets); }

        double hvpsVoltage() const { return(_hvpsVoltage); }
        double magnetronCurrent() const { return(_magnetronCurrent); }
        double hvpsCurrent() const { return(_hvpsCurrent); }
        double temperature() const { return(_temperature); }
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
