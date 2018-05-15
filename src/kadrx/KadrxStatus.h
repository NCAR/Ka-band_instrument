// KadrxStatus.h
//
//  Created on: May 14, 2018
//      Author: Chris Burghart <burghart@ucar.edu>


#ifndef SRC_KADRX_KADRXSTATUS_H_
#define SRC_KADRX_KADRXSTATUS_H_

#include <XmlRpc.h>
#include "NoXmitBitmap.h"

/// @brief Class which encapsulates status from the kadrx process
class KadrxStatus {
public:
    /// @brief Construct from a status dictionary returned by kadrx's
    /// getStatus() XML-RPC method.
    /// @param statusDict dictionary of kadrx status values, as returned by
    /// an XML-RPC call to kadrx's getStatus() method
    KadrxStatus(XmlRpc::XmlRpcValue::ValueStruct statusDict);

    virtual ~KadrxStatus();

    /// @brief Return true iff the GPS time server is good (i.e., is not
    /// reporting an alarm)
    /// @return true iff the GPS time server is good (i.e., is not reporting
    /// an alarm)
    bool gpsTimeServerGood() const { return(_gpsTimeServerGood); }

    /// @brief Return true iff the 100 MHz oscillator is locked
    /// @return true iff the 100 MHz oscillator is locked
    bool locked100MHz() const { return(_locked100MHz); }

    /// @brief Return true iff pressure is good in the N2 waveguide between
    /// the outside of the transmitter and the antenna.
    /// @return true iff pressure is good in the N2 waveguide between
    /// the outside of the transmitter and the antenna.
    bool n2PressureGood() const { return(_n2PressureGood); }

    /// @brief Return the currently programmed frequency (in Hz) for AFC
    /// oscillator 0
    /// @return the currently programmed frequency (in Hz) for AFC oscillator 0
    double osc0Frequency() const { return(_osc0Frequency); }

    /// @brief Return the currently programmed frequency (in Hz) for AFC
    /// oscillator 1
    /// @return the currently programmed frequency (in Hz) for AFC oscillator 1
    double osc1Frequency() const { return(_osc1Frequency); }

    /// @brief Return the currently programmed frequency (in Hz) for AFC
    /// oscillator 2
    /// @return the currently programmed frequency (in Hz) for AFC oscillator 2
    double osc2Frequency() const { return(_osc2Frequency); }

    /// @brief Return the currently programmed frequency (in Hz) for AFC
    /// oscillator 3
    /// @return the currently programmed frequency (in Hz) for AFC oscillator 3
    double osc3Frequency() const { return(_osc3Frequency); }

    /// @brief Return the current transmit frequency (in Hz) as derived from
    /// the current AFC oscillator settings
    /// @return the current transmit frequency (in Hz) as derived from
    /// the current AFC oscillator settings
    double derivedTxFrequency() const { return(_derivedTxFrequency); }

    /// @brief Return transmit power in horizontal polarization, dBm
    /// @return transmit power in horizontal polarization, dBm
    double hTxPower() const { return(_hTxPower); }

    /// @brief Return transmit power in vertical polarization, dBm
    /// @return transmit power in vertical polarization, dBm
    double vTxPower() const { return(_vTxPower); }

    /// @brief Return test pulse power measured at the receiver, dBm
    /// @return test pulse power measured at the receiver, dBm
    double testTargetPowerRaw() const { return(_testTargetPowerRaw); }

    /// @brief Return the temperature of the DRX board in the processor
    /// enclosure, deg C
    /// @return the temperature of the DRX board in the processor
    /// enclosure, deg C
    double procDrxTemp() const { return(_procDrxTemp); }

    /// @brief Return the ambient temperature inside the processor enclosure,
    /// deg C
    /// @return the ambient temperature inside the processor enclosure, deg C
    double procEnclosureTemp() const { return(_procEnclosureTemp); }

    /// @brief Return the ambient temperature in the back of the receiver
    /// enclosure, deg C
    /// @return the ambient temperature in the back of the receiver
    /// enclosure, deg C
    double rxBackTemp() const { return(_rxBackTemp); }

    /// @brief Return the ambient temperature in the front of the receiver
    /// enclosure, deg C
    /// @return the ambient temperature in the front of the receiver
    /// enclosure, deg C
    double rxFrontTemp() const { return(_rxFrontTemp); }

    /// @brief Return the ambient temperature at the top of the receiver
    /// enclosure, deg C
    /// @return the ambient temperature at the top of the receiver
    /// enclosure, deg C
    double rxTopTemp() const { return(_rxTopTemp); }

    /// @brief Return the ambient temperature inside the transmitter
    /// enclosure, deg C
    /// @return the ambient temperature inside the transmitter enclosure, deg C
    double txEnclosureTemp() const { return(_txEnclosureTemp); }

    /// @brief Return the 5V digital V_CC power supply voltage, V
    /// @return the 5V digital V_CC power supply voltage, V
    double psVoltage() const { return(_psVoltage); }

    /// @brief Return a NoXmitBitmap object encapsulating reasons (if any)
    /// that kadrx is currently disabling transmit
    /// @return a NoXmitBitmap object encapsulating reasons (if any)
    /// that kadrx is currently disabling transmit
    NoXmitBitmap noXmitBitmap() const { return(_noXmitBitmap); }

private:
    bool _gpsTimeServerGood;    ///< is the GPS time server available?
    bool _locked100MHz;         ///< is the 100 MHz oscillator locked?
    bool _n2PressureGood;       ///< is the N2 waveguide pressure good?

    double _osc0Frequency;       ///< AFC oscillator 0 frequency, Hz
    double _osc1Frequency;       ///< AFC oscillator 1 frequency, Hz
    double _osc2Frequency;       ///< AFC oscillator 2 frequency, Hz
    double _osc3Frequency;       ///< AFC oscillator 3 frequency, Hz
    double _derivedTxFrequency;  ///< transmit frequency, derived from AFC

    double _hTxPower;            ///< H polarization transmit power, dBm
    double _vTxPower;            ///< V polarization transmit power, dBm
    double _testTargetPowerRaw;  ///< test pulse raw power, dBm

    double _procDrxTemp;         ///< DRX temp in the processor enclosure, deg C
    double _procEnclosureTemp;   ///< processor enclosure temperature, deg C
    double _rxBackTemp;          ///< receiver enclosure back temperature, deg C
    double _rxFrontTemp;         ///< receiver enclosure front temperature, deg C
    double _rxTopTemp;           ///< receiver enclosure top temperature, deg C
    double _txEnclosureTemp;     ///< transmitter enclosure temperature, deg C

    double _psVoltage;           ///< power supply voltage, V

    NoXmitBitmap _noXmitBitmap; ///< bitmap of reasons transmit is disabled
};

#endif /* SRC_KADRX_KADRXSTATUS_H_ */
