// *=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*
// ** Copyright UCAR (c) 1990 - 2018
// ** University Corporation for Atmospheric Research (UCAR)
// ** National Center for Atmospheric Research (NCAR)
// ** Boulder, Colorado, USA
// ** BSD licence applies - redistribution and use in source and binary
// ** forms, with or without modification, are permitted provided that
// ** the following conditions are met:
// ** 1) If the software is modified to produce derivative works,
// ** such modified software should be clearly marked, so as not
// ** to confuse it with the version available from UCAR.
// ** 2) Redistributions of source code must retain the above copyright
// ** notice, this list of conditions and the following disclaimer.
// ** 3) Redistributions in binary form must reproduce the above copyright
// ** notice, this list of conditions and the following disclaimer in the
// ** documentation and/or other materials provided with the distribution.
// ** 4) Neither the name of UCAR nor the names of its contributors,
// ** if any, may be used to endorse or promote products derived from
// ** this software without specific prior written permission.
// ** DISCLAIMER: THIS SOFTWARE IS PROVIDED "AS IS" AND WITHOUT ANY EXPRESS
// ** OR IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
// ** WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
// *=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*
/*
 * KadrxRpcClient.h
 *
 *  Created on: May 17, 2018
 *      Author: Chris Burghart <burghart@ucar.edu>
 */

#ifndef KADRXRPCCLIENT_H_
#define KADRXRPCCLIENT_H_

#include <xmlrpc-c/client_simple.hpp>
#include <string>
#include "KadrxStatus.h"

/**
 * KadrxRpcClient encapsulates an XML-RPC connection to a kadrx
 * process.
 */
class KadrxRpcClient {
public:
    /**
     * Instantiate KadrxRpcClient to communicate with an kadrx process running
     * on host kadrxHost and using port kadrxPort.
     * @param kadrxHost the name of the host on which kadrx is running
     * @param kadrxPort the port number being used by kadrx
     */
    KadrxRpcClient(std::string kadrxHost, int kadrxPort);
    virtual ~KadrxRpcClient();


    /// @brief Send a "getStatus" command, filling a KadrxStatus
    /// object if we get status from the kadrx.
    /// @param status the KadrxRpcClient::Status object to be filled
    /// @return true and fill the status object if status is obtained from
    /// kadrx, otherwise return false and leave the status object
    /// unmodified.
    /// @return true iff the XML-RPC call executes, otherwise return false.
    bool getStatus(KadrxStatus & status);


    /// @brief Get the port number of the associated kadrx.
    /// @return the port number of the associated kadrx.
    int getKadrxPort() { return(_kadrxPort); }

    /// @brief Get the name of the host on which the associated kadrx is
    /// running.
    /// @return the name of the host on which the associated kadrx is running.
    std::string getKadrxHost() { return(_kadrxHost); }

private:
    /// @brief Execute an XML-RPC method call and return the result.
    /// @param[in] methodName the name of the XML-RPC method to execute
    /// @param[out] result if the call is successful, the returned value is
    /// written in result
    /// @return true and write the returned value in result iff the XML-RPC
    /// call was successful
    bool _execXmlRpcCall(std::string methodName, xmlrpc_c::value & result);

    std::string _kadrxHost;
    int _kadrxPort;
    std::string _daemonUrl;
    xmlrpc_c::clientSimple _client;
};

#endif /* KADRXRPCCLIENT_H_ */
