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
 * KadrxRpcClient.cpp
 *
 *  Created on: May 17, 2018
 *      Author: Chris Burghart <burghart@ucar.edu>
 */

#include <kadrx/KadrxRpcClient.h>
#include <cstdlib>
#include <logx/Logging.h>

LOGGING("KadrxRpcClient")

KadrxRpcClient::KadrxRpcClient(std::string kadrxHost, int kadrxPort) :
    _kadrxHost(kadrxHost),
    _kadrxPort(kadrxPort),
    _client() {
    // build _daemonUrl: "http://<_kadrxHost>:<_kadrxPort>/RPC2"
    std::ostringstream ss;
    ss << "http://" << _kadrxHost << ":" << _kadrxPort << "/RPC2";
    _daemonUrl = ss.str();
    ILOG << "KadrxRpcClient on " << _daemonUrl;
}

KadrxRpcClient::~KadrxRpcClient() {
}

bool
KadrxRpcClient::_execXmlRpcCall(std::string methodName,
        xmlrpc_c::value & result) {
    try {
        _client.call(_daemonUrl, methodName, "", &result);
    } catch (std::exception & e) {
        WLOG << "Error on XML-RPC " << methodName << "() call: " << e.what();
        return(false);
    }
    return(true);
}

bool
KadrxRpcClient::getStatus(KadrxStatus & status) {
    xmlrpc_c::value result;
    if (! _execXmlRpcCall("getStatus", result)) {
        return(false);
    }
    // Convert the result into a KadrxStatus object and return it in status
    xmlrpc_c::value_struct resultStruct(result);
    status = KadrxStatus(resultStruct);
    return(true);
}

