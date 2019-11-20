// KadrxStatus.h
//
//  Created on: May 14, 2018
//      Author: Chris Burghart <burghart@ucar.edu>


#ifndef SRC_KADRX_KADRXSTATUS_H_
#define SRC_KADRX_KADRXSTATUS_H_

#include <xmlrpc-c/base.hpp>
#include <boost/serialization/nvp.hpp>
#include <boost/serialization/version.hpp>
#include "KaMonitor.h"
#include "NoXmitBitmap.h"

/// @brief Class which encapsulates status from the kadrx process
class KadrxStatus {
public:
    /// @brief Default constructor, with members set to all zeros.
    KadrxStatus();

    /// @brief The specify-everything constructor.
    /// @param noXmitBitmap the NoXmitBitmap currently being used by kadrx
    /// @param afcEnabled true iff AFC is currently enabled
    /// @param gpsTimeServerGood true iff the GPS time server is locked
    /// @param locked100MHz true iff the 100 MHz oscillator is locked
    /// @param n2PressureGood true iff pressure in the N2 waveguide is good
    /// @param afcIsTracking true iff the AFC is currently tracking transmitter
    /// frequency
    /// @param g0AvgPower the last burst channel power average calculated by
    /// the AFC, dBm
    /// @param osc0Frequency current frequency of AFC oscillator 0, Hz
    /// @param osc1Frequency current frequency of AFC oscillator 1, Hz
    /// @param osc2Frequency current frequency of AFC oscillator 2, Hz
    /// @param osc3Frequency current frequency of AFC oscillator 3, Hz
    /// @param derivedTxFrequency transmit frequency derived from current
    /// frequencies of the AFC oscillators, Hz
    /// @param hTxPower horizontal channel transmit power, dBm
    /// @param vTxPower vertical channel transmit power, dBm
    /// @param testPulsePower measured test pulse power, dBm
    /// @param procDrxTemp temperature at the DRX computer in the processor
    /// enclosure, deg C
    /// @param procEnclosureTemp ambient temperature in the processor
    /// enclosure, deg C
    /// @param rxBackTemp ambient temperature at the back of the receiver
    /// enclosure, deg C
    /// @param rxFrontTemp ambient temperature at the front of the receiver
    /// enclosure, deg C
    /// @param rxTopTemp ambient temperature at the top of the receiver
    /// enclosure, deg C
    /// @param txEnclosureTemp ambient temperature in the transmitter
    /// enclosure, deg C
    /// @param psVoltage measured voltage of the 5V digital signal power
    /// supply, V
    KadrxStatus(const NoXmitBitmap & noXmitBitmap,
                bool afcEnabled,
                bool gpsTimeServerGood,
                bool locked100MHz,
                bool n2PressureGood,
                bool afcIsTracking,
                double g0AvgPower,
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
                double psVoltage);

    /// @brief Construct using information from a KaMonitor instance and
    /// kadrx's current NoXmitBitmap state.
    /// @param kaMonitor the kaMonitor instance providing status
    /// @param noXmitBitmap kadrx's current NoXmitBitmap
    KadrxStatus(const KaMonitor & kaMonitor, const NoXmitBitmap & noXmitBitmap);

    /// @brief Construct from a status dictionary returned by kadrx's
    /// getStatus() XML-RPC method.
    /// @param statusDict dictionary of kadrx status values, as returned by
    /// an XML-RPC call to kadrx's getStatus() method
    KadrxStatus(const xmlrpc_c::value_struct & statusDict);

    /// @brief Destructor
    virtual ~KadrxStatus();

    /// @brief Return an external representation of the object's state as
    /// an xmlrpc_c::value_struct dictionary.
    ///
    /// The returned value can be used on the other side of an XML-RPC
    /// connection to create an identical object via the
    /// XmitStatus(const xmlrpc_c::value_struct &) constructor.
    /// @return an external representation of the object's state as
    /// an xmlrpc_c::value_struct dictionary.
    xmlrpc_c::value_struct toXmlRpcValue() const;

    /// @brief Return true iff AFC is currently enabled
    /// @return true iff AFC is currently enabled
    bool afcEnabled() const { return(_afcEnabled); }

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

    /// @brief Return true iff the AFC is currently tracking transmitter
    /// frequency
    /// @return true iff the AFC is currently tracking transmitter
    /// frequency
    bool afcIsTracking() const { return(_afcIsTracking); }

    /// @brief Return the last average burst channel power calculated by the
    /// AFC, dBm
    /// @return the last average burst channel power calculated by the
    /// AFC, dBm
    double g0AvgPower() const { return(_g0AvgPower); }

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
    double testPulsePower() const { return(_testPulsePower); }

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
    friend class boost::serialization::access;

    /// @brief Serialize our members to a boost save (output) archive or populate
    /// our members from a boost load (input) archive.
    /// @param ar the archive to load from or save to.
    /// @param version the KadrxStatus version number
    template<class Archive>
    void serialize(Archive & ar, const unsigned int version) {
        using boost::serialization::make_nvp;
        // Version 0 (see BOOST_CLASS_VERSION macro below for latest version)
        if (version >= 0) {
            // Map named entries to our member variables using serialization's
            // name/value pairs (nvp).
            ar & BOOST_SERIALIZATION_NVP(_afcEnabled);
            ar & BOOST_SERIALIZATION_NVP(_gpsTimeServerGood);
            ar & BOOST_SERIALIZATION_NVP(_locked100MHz);
            ar & BOOST_SERIALIZATION_NVP(_n2PressureGood);
            ar & BOOST_SERIALIZATION_NVP(_afcIsTracking);
            ar & BOOST_SERIALIZATION_NVP(_g0AvgPower);
            ar & BOOST_SERIALIZATION_NVP(_osc0Frequency);
            ar & BOOST_SERIALIZATION_NVP(_osc1Frequency);
            ar & BOOST_SERIALIZATION_NVP(_osc2Frequency);
            ar & BOOST_SERIALIZATION_NVP(_osc3Frequency);
            ar & BOOST_SERIALIZATION_NVP(_derivedTxFrequency);
            ar & BOOST_SERIALIZATION_NVP(_hTxPower);
            ar & BOOST_SERIALIZATION_NVP(_vTxPower);
            ar & BOOST_SERIALIZATION_NVP(_testPulsePower);
            ar & BOOST_SERIALIZATION_NVP(_procDrxTemp);
            ar & BOOST_SERIALIZATION_NVP(_procEnclosureTemp);
            ar & BOOST_SERIALIZATION_NVP(_rxBackTemp);
            ar & BOOST_SERIALIZATION_NVP(_rxFrontTemp);
            ar & BOOST_SERIALIZATION_NVP(_rxTopTemp);
            ar & BOOST_SERIALIZATION_NVP(_txEnclosureTemp);
            ar & BOOST_SERIALIZATION_NVP(_psVoltage);

            int rawBitmap = _noXmitBitmap.rawBitmap();
            ar & BOOST_SERIALIZATION_NVP(rawBitmap);
            _noXmitBitmap = NoXmitBitmap(rawBitmap);
        }
        if (version >= 1) {
            // Version 1 stuff will go here...
        }
    }

    /// @brief Initialize all members to zero
    void _zeroAllMembers();

    bool _afcEnabled;           ///< is AFC enabled?
    bool _gpsTimeServerGood;    ///< is the GPS time server available?
    bool _locked100MHz;         ///< is the 100 MHz oscillator locked?
    bool _n2PressureGood;       ///< is the N2 waveguide pressure good?

    bool _afcIsTracking;        ///< true iff AFC is tracking transmitter frequency
    double _g0AvgPower;         ///< last burst channel power reported to AFC, dBm
    double _osc0Frequency;      ///< AFC oscillator 0 frequency, Hz
    double _osc1Frequency;      ///< AFC oscillator 1 frequency, Hz
    double _osc2Frequency;      ///< AFC oscillator 2 frequency, Hz
    double _osc3Frequency;      ///< AFC oscillator 3 frequency, Hz
    double _derivedTxFrequency; ///< transmit frequency, derived from AFC

    double _hTxPower;           ///< H polarization transmit power, dBm
    double _vTxPower;           ///< V polarization transmit power, dBm
    double _testPulsePower;     ///< test pulse raw power, dBm

    double _procDrxTemp;         ///< DRX temp in the processor enclosure, deg C
    double _procEnclosureTemp;   ///< processor enclosure temperature, deg C
    double _rxBackTemp;          ///< receiver enclosure back temperature, deg C
    double _rxFrontTemp;         ///< receiver enclosure front temperature, deg C
    double _rxTopTemp;           ///< receiver enclosure top temperature, deg C
    double _txEnclosureTemp;     ///< transmitter enclosure temperature, deg C

    double _psVoltage;           ///< power supply voltage, V

    NoXmitBitmap _noXmitBitmap; ///< bitmap of reasons transmit is disabled
};

// Increment this class version number when member variables are changed.
BOOST_CLASS_VERSION(KadrxStatus, 0)

#endif /* SRC_KADRX_KADRXSTATUS_H_ */
