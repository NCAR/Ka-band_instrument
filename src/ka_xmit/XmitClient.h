/*
 * XmitClient.h
 *
 *  Created on: Mar 11, 2011
 *      Author: burghart
 */

#ifndef XMITCLIENT_H_
#define XMITCLIENT_H_

#include <string>
#include <XmlRpc.h>
#include "XmitdStatus.h"

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
    
    // Send a "getStatus" command, filling a XmitClient::XmitdStatus object if
    // we get status from the ka_xmitd.
    // @param status the XmitClient::XmitdStatus object to be filled
    // @return true and fill the status object if status is obtained from 
    // ka_xmitd, otherwise return false and leave the status object 
    // unmodified.
    bool getStatus(XmitdStatus & status);
    
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
