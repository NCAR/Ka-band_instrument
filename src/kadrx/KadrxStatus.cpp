/*
 * KadrxStatus.cpp
 *
 *  Created on: May 14, 2018
 *      Author: Chris Burghart <burghart@ucar.edu>
 */
#include "KadrxStatus.h"
#include <sstream>
#include <Archive_xmlrpc_c.h>
#include <logx/Logging.h>

LOGGING("KadrxStatus")

KadrxStatus::KadrxStatus() {
    _zeroAllMembers();
}

KadrxStatus::KadrxStatus(const NoXmitBitmap & noXmitBitmap,
                         bool afcEnabled,
                         bool gpsTimeServerGood,
                         bool locked100MHz,
                         bool n2PressureGood,
                         double osc0Frequency,
                         double osc1Frequency,
                         double osc2Frequency,
                         double osc3Frequency,
                         double derivedTxFrequency,
                         double hTxPower,
                         double vTxPower,
                         double testPulsePower,
                         double procDrxTemp,
                         double procEnclosureTemp,
                         double rxBackTemp,
                         double rxFrontTemp,
                         double rxTopTemp,
                         double txEnclosureTemp,
                         double psVoltage) :
    _afcEnabled(afcEnabled),
    _gpsTimeServerGood(gpsTimeServerGood),
    _locked100MHz(locked100MHz),
    _n2PressureGood(n2PressureGood),
    _osc0Frequency(osc0Frequency),
    _osc1Frequency(osc1Frequency),
    _osc2Frequency(osc2Frequency),
    _osc3Frequency(osc3Frequency),
    _derivedTxFrequency(derivedTxFrequency),
    _hTxPower(hTxPower),
    _vTxPower(vTxPower),
    _testPulsePower(testPulsePower),
    _procDrxTemp(procDrxTemp),
    _procEnclosureTemp(procEnclosureTemp),
    _rxBackTemp(rxBackTemp),
    _rxFrontTemp(rxFrontTemp),
    _rxTopTemp(rxTopTemp),
    _txEnclosureTemp(txEnclosureTemp),
    _psVoltage(psVoltage),
    _noXmitBitmap(noXmitBitmap)
{
}

KadrxStatus::KadrxStatus(const xmlrpc_c::value_struct & status) {
    _zeroAllMembers();
    // Create an input archiver wrapper around the xmlrpc_c::value_struct
    // dictionary, and use serialize() to populate our members from its content.
    Iarchive_xmlrpc_c iar(status);
    iar >> *this;
}

KadrxStatus::~KadrxStatus() {
}

void
KadrxStatus::_zeroAllMembers() {
    _afcEnabled = false;
    _gpsTimeServerGood = false;
    _locked100MHz = false;
    _n2PressureGood = false;
    _osc0Frequency = 0.0;
    _osc1Frequency = 0.0;
    _osc2Frequency = 0.0;
    _osc3Frequency = 0.0;
    _derivedTxFrequency = 0.0;
    _hTxPower = 0.0;
    _vTxPower = 0.0;
    _testPulsePower = 0.0;
    _procDrxTemp = 0.0;
    _procEnclosureTemp = 0.0;
    _rxBackTemp = 0.0;
    _rxFrontTemp = 0.0;
    _rxTopTemp = 0.0;
    _txEnclosureTemp = 0.0;
    _psVoltage = 0.0;
    _noXmitBitmap = NoXmitBitmap();
}

xmlrpc_c::value_struct
KadrxStatus::toXmlRpcValue() const {
    std::map<std::string, xmlrpc_c::value> statusDict;
    // Clone ourself to a non-const instance
    KadrxStatus clone(*this);
    // Stuff our content into the statusDict, i.e., _serialize() to an
    // output archiver wrapped around the statusDict.
    Oarchive_xmlrpc_c oar(statusDict);
    oar << clone;
    // Finally, return the statusDict
    return(xmlrpc_c::value_struct(statusDict));
}
