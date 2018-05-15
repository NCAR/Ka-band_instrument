/*
 * KadrxStatus.cpp
 *
 *  Created on: May 14, 2018
 *      Author: Chris Burghart <burghart@ucar.edu>
 */
#include "KadrxStatus.h"
#include <sstream>
#include <logx/Logging.h>

LOGGING("KadrxStatus")

/// Template function which returns the value of dict[key] as the templated type
/// and removes the key from dict.
template<typename T>
T extractFromDict(XmlRpc::XmlRpcValue::ValueStruct & dict, std::string key) {
    if (dict.count(key) == 0) {
        std::ostringstream os;
        os << "Key '" << key << "' missing in kadrx status dictionary";
        throw(std::runtime_error(os.str()));
    }
    T value = dict[key];
    // Clear the extracted value from the dictionary
    dict.erase(key);
    return(value);
}

KadrxStatus::KadrxStatus(XmlRpc::XmlRpcValue::ValueStruct status) :
    _gpsTimeServerGood(extractFromDict<bool>(status, "gpsTimeServerGood")),
    _locked100MHz(extractFromDict<bool>(status, "locked100MHz")),
    _n2PressureGood(extractFromDict<bool>(status, "n2PressureGood")),
    _osc0Frequency(extractFromDict<double>(status, "osc0Frequency")),
    _osc1Frequency(extractFromDict<double>(status, "osc1Frequency")),
    _osc2Frequency(extractFromDict<double>(status, "osc2Frequency")),
    _osc3Frequency(extractFromDict<double>(status, "osc3Frequency")),
    _derivedTxFrequency(extractFromDict<double>(status, "derivedTxFrequency")),
    _hTxPower(extractFromDict<double>(status, "hTxPower")),
    _vTxPower(extractFromDict<double>(status, "vTxPower")),
    _testTargetPowerRaw(extractFromDict<double>(status, "testTargetPowerRaw")),
    _procDrxTemp(extractFromDict<double>(status, "procDrxTemp")),
    _procEnclosureTemp(extractFromDict<double>(status, "procEnclosureTemp")),
    _rxBackTemp(extractFromDict<double>(status, "rxBackTemp")),
    _rxFrontTemp(extractFromDict<double>(status, "rxFrontTemp")),
    _rxTopTemp(extractFromDict<double>(status, "rxTopTemp")),
    _txEnclosureTemp(extractFromDict<double>(status, "txEnclosureTemp")),
    _psVoltage(extractFromDict<double>(status, "psVoltage")),
    _noXmitBitmap(extractFromDict<int>(status, "noXmitBitmap"))
{
    // We should have emptied the status dictionary during construction. If
    // not, warn about keys which we did not use.
    XmlRpc::XmlRpcValue::ValueStruct::const_iterator it;
    for (it = status.begin(); it != status.end(); it++) {
        WLOG << "Unexpected key '" << it->first <<
                "' in kadrx status dictionary.";
    }
}

KadrxStatus::~KadrxStatus() {
}
