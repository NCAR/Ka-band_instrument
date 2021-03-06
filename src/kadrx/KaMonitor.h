/*
 * KaMonitor.h
 *
 *  Created on: Dec 13, 2010
 *      Author: burghart
 */

#ifndef KAMONITOR_H_
#define KAMONITOR_H_

#include <stdint.h>
#include <deque>

#include <QtCore/QThread>
#include <QtCore/QMutex>

#include <XmitClient.h>

class KaMonitorPriv;
typedef struct {
    float power;
    float voltage;
} QEA_Cal_Val;


/// QThread object which handles Ka monitoring, regularly sampling all status 
/// available via the multi-IO card as well as transmitter status information 
/// obtained from the ka_xmitd process.
class KaMonitor : public QThread {
	Q_OBJECT
public:
    /**
     * Construct a KaMonitor which will get transmitter status from ka_xmitd
     * running on host xmitdHost/port xmitdPort.
     */
    KaMonitor(std::string xmitdHost, int xmitdPort);
    
    ~KaMonitor();
    
    void run();

    /**
     * Return processor enclosure temperature, C.
     * @return processor enclosure temperature, C
     */
    float procEnclosureTemp() const;
    /**
     * Return temperature near the DRX computer, C.
     * @return temperature near the DRX computer, C
     */
    float procDrxTemp() const;
    /**
     * Return the transmitter enclosure temperature, C.
     * @return the transmitter enclosure temperature, C
     */
    float txEnclosureTemp() const;
    /**
     * Return the temperature at the top of the receiver enclosure, C.
     * @return the temperature at the top of the receiver enclosure, C
     */
    float rxTopTemp() const;
    /**
     * Return the temperature at the back of the receiver enclosure, C.
     * @return the temperature at the back of the receiver enclosure, C
     */
    float rxBackTemp() const;
    /**
     * Return the temperature at the front of the receiver enclosure, C.
     * @return the temperature at the front of the receiver enclosure, C
     */
    float rxFrontTemp() const;
    /**
     * Transmit power @ H channel QuinStar power detector, dBm
     * @return raw transmit power @ H channel, dBm
     */
    float hTxPowerRaw() const;
    /**
     * Transmit power @ V channel QuinStar power detector, dBm
     * @return raw transmit power @ V channel, dBm
     */
    float vTxPowerRaw() const;
    /**
     * Test target power @ QuinStar power detector, dBm
     * @return raw test target power, dBm
     */
    float testTargetPowerRaw() const;
    /**
     * Measured voltage @ 5V power supply, V
     * @return measured voltage @ 5V power supply, V
     */
    float psVoltage() const;
    /**
     * Return true iff the N2 waveguide pressure outside of the transmitter
     * is good
     * @return true iff the N2 waveguide pressure outside the transmitter is
     *good.
     */
    bool wgPressureGood() const;
    /**
     * Is the 100 MHz oscillator locked?
     * @return true iff the 100 MHz oscillator is locked.
     */
    bool locked100MHz() const;
    /**
     * Is the GPS NTP time server locked and running properly?
     * @return true iff the GPS NTP time server is locked and running properly.
     */
    bool gpsTimeServerGood() const;
    /**
     * Get the transmitter status.
     * @return the transmitter status.
     */
    XmitdStatus transmitterStatus() const;
    
    /**
     * Oscillator 0 frequency, in Hz.
     * @return oscillator 0 frequency, in Hz.
     */
    uint64_t osc0Frequency() const;

    /// @brief Return true iff AFC is currently tracking transmitter frequency
    /// (vs. doing a coarse search for transmitter frequency).
    /// @return true iff AFC is currently tracking transmitter frequency
    /// (vs. doing a coarse search for transmitter frequency).
    bool afcIsTracking() const;

    /// @brief Return the last g0 average power used by AFC, dBm
    /// @return the last g0 average power used by AFC, dBm
    double g0AvgPower() const;

    /**
     * Oscillator 1 frequency, in Hz.
     * @return oscillator 1 frequency, in Hz.
     */
    uint64_t osc1Frequency() const;
    
    /**
     * Oscillator 2 frequency, in Hz.
     * @return oscillator 2 frequency, in Hz.
     */
    uint64_t osc2Frequency() const;
    
    /**
     * Oscillator 3 frequency, in Hz.
     * @return oscillator 3 frequency, in Hz.
     */
    uint64_t osc3Frequency() const;
    
    /**
     * Transmitter frequency, derived from frequencies of oscillators 0-3, in 
     * Hz.
     * @return transmitter frequency, derived from frequencies of oscillators 
     * 0-3, in Hz.
     */
    uint64_t derivedTxFrequency() const;
    
private:
    /**
     * Convert QEA crystal detector voltage to input power (in dBm), based
     * on a fixed table of calibration measurements.
     * @param voltage QEA crystal detector output, in V
     * @return power measured by the QEA crystal detector, in dBm
     */
    static double _lookupQEAPower(const QEA_Cal_Val *qea_vals, unsigned len, double voltage);
    /**
     * Return the temperature, in C, based on temperature sensor voltage.
     * @param voltage voltage from temperature sensor
     * @return the temperature, in C
     */
    static double _voltsToTemp(double voltage) {
        // Temperature sensor voltage = 0.01 * T(Kelvin)
        // Convert volts to Kelvin, then to Celsius
        return((100 * voltage) - 273.15);
    }
    /**
     * Get new values for all of our sensor data supplied via the PMC730
     * multi-IO card.
     */
    void _getMultiIoValues();
    /**
     * Get status from the transmitter and put it in our local _xmitStatus
     * member.
     */
    void _getXmitStatus();
    /**
     * Get oscillator frequencies from the singleton KaOscControl.
     */
    void _getAfcStatus();
    /**
     * Return the average of values in a deque<float>, or -99.9 if the deque
     * is empty.
     * @return the average of values in a deque<float>, or -99.9 if the deque
     *      is empty.
     */
    static float _dequeAverage(const std::deque<float> & list);
    
    /// Thread access mutex (mutable so we can lock the mutex even in const
    /// methods)
    mutable QMutex _mutex;
    
    /// window size for moving temperature averages
    static const unsigned int TEMP_AVERAGING_LEN = 20;
    
    /// Deques to hold temperature lists. Lists of temperatures are kept so
    /// that we can time-average to reduce noise in the sampling.
    std::deque<float> _procEnclosureTemps;
    std::deque<float> _procDrxTemps;
    std::deque<float> _txEnclosureTemps;
    std::deque<float> _rxTopTemps;
    std::deque<float> _rxBackTemps;
    std::deque<float> _rxFrontTemps;
    
    /// Measurements from the three QuinStar power detectors
    float _hTxPowerRaw;           // H tx pulse power, dBm
    float _vTxPowerRaw;           // V tx pulse power, dBm
    float _testTargetPowerRaw;    // test target power, dBm
    
    /// 5V power supply
    float _psVoltage;
    
    /// Valid pressure in waveguide outside the transmitter?
    bool _wgPressureGood;

    /// 100 MHz oscillator OK?
    bool _locked100MHz;

    /// GPS time server alarm state
    bool _gpsTimeServerGood;
    
    /// Is AFC currently tracking the transmitter (vs. doing a coarse search)?
    bool _afcIsTracking;

    /// Last g0 power average used by AFC, dBm
    double _g0AvgPower;

    /// Oscillator frequencies, Hz
    uint64_t _osc0Frequency;
    uint64_t _osc1Frequency;
    uint64_t _osc2Frequency;
    uint64_t _osc3Frequency;
    
    /// XML-RPC access to ka_xmitd for its status
    XmitClient _xmitClient;
    XmitdStatus _xmitStatus;
};


#endif /* KAMONITOR_H_ */
