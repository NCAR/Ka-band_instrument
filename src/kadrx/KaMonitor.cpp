/*
 * KaMonitor.cpp
 *
 *  Created on: Oct 29, 2010
 *      Author: burghart
 */

#include "KaMonitor.h"
#include "KaPmc730.h"

#include <QThread>
#include <QTimer>
#include <QMutex>

#include <iomanip>
#include <cmath>
#include <vector>
#include <deque>

#include <logx/Logging.h>

LOGGING("KaMonitor")

/* 
 * Quinstar QEA crystal detector calibration measurements from 11/04/2010, 
 * input power in dBm vs. output volts.
 */
static const float QEA_Cal[][2] = {
  {-34.72, 2.48E-03},
  {-33.72, 3.36E-03},
  {-32.72, 4.60E-03},
  {-31.72, 6.00E-03},
  {-30.72, 7.60E-03},
  {-29.72, 9.60E-03},
  {-28.72, 1.24E-02},
  {-27.72, 1.54E-02},
  {-26.72, 2.06E-02},
  {-25.72, 2.58E-02},
  {-24.72, 3.16E-02},
  {-23.72, 3.86E-02},
  {-22.72, 4.72E-02},
  {-21.72, 5.70E-02},
  {-20.72, 6.92E-02},
  {-19.72, 8.22E-02},
  {-18.72, 9.80E-02},
  {-17.72, 1.16E-01},
  {-16.72, 1.44E-01},
  {-15.72, 1.67E-01},
  {-14.72, 1.95E-01},
  {-13.72, 2.26E-01},
  {-12.72, 2.62E-01},
  {-11.72, 2.94E-01},
  {-10.72, 3.40E-01},
  {-9.72, 3.90E-01},
  {-8.72, 4.48E-01},
  {-7.72, 5.14E-01},
  {-6.72, 6.08E-01},
  {-5.72, 6.90E-01},
  {-4.72, 7.84E-01},
  {-3.72, 8.86E-01},
  {-2.72, 1.00E+00},
  {-1.72, 1.13E+00},
  {-0.72, 1.26E+00},
  {0.28, 1.41E+00},
  {1.28, 1.56E+00},
  {2.28, 1.74E+00},
  {3.28, 1.97E+00},
  {4.28, 2.16E+00},
  {5.28, 2.36E+00},
  {6.28, 2.56E+00},
  {7.28, 2.76E+00},
  {8.28, 2.86E+00},
  {9.28, 3.08E+00},
  {10.28, 3.30E+00},
  {11.28, 3.50E+00},
  {12.28, 3.72E+00},
  {13.28, 3.94E+00},
  {14.28, 4.14E+00},
};
static const int QEA_CalLen = (sizeof(QEA_Cal) / (sizeof(*QEA_Cal)));

// Pointer to our singleton instance
KaMonitor * KaMonitor::_theMonitor = 0;

/// KaMonitorPriv is the private implementation class for KaMonitor, subclassed 
/// from QThread. The object polls various temperatures and rx video powers 
/// on a regular (1 Hz) basis, and provides access to the status via XML-RPC
/// methods.
class KaMonitorPriv : public QThread {
public:
    KaMonitorPriv();
    
    ~KaMonitorPriv();
    
    void run();
    
private:
    /**
     * Convert QEA crystal detector voltage to input power (in dBm), based
     * on a fixed table of calibration measurements.
     * @param voltage QEA crystal detector output, in V
     * @return power measured by the QEA crystal detector, in dBm
     */
    double _lookupQEAPower(double voltage);
    /**
     * Return the temperature, in C, based on temperature sensor voltage.
     * @param voltage voltage from temperature sensor
     * @return the temperature, in C
     */
    double _voltsToTemp(double voltage) {
        // Temperature sensor voltage = 0.01 * T(Kelvin)
        // Convert volts to Kelvin, then to Celsius
        return((100 * voltage) - 273.15);
    }
    /**
     * Get new values for all of our sensor data
     */
    void _getNewValues();
    /**
     * Return the average of values in a deque<float>, or -99.9 if the deque
     * is empty.
     * @return the average of values in a deque<float>, or -99.9 if the deque
     *      is empty.
     */
    float _dequeAverage(std::deque<float> & list);
    
    /// Thread access mutex
    QMutex _mutex;
    
    /// Temperature accessors
    float _procEnclosureTemp() { return _dequeAverage(_procEnclosureTemps); } // C
    float _procDrxTemp() { return _dequeAverage(_procDrxTemps); } // C
    float _txEnclosureTemp() { return _dequeAverage(_txEnclosureTemps); } // C
    float _rxTopTemp() { return _dequeAverage(_rxTopTemps); } // C
    float _rxBackTemp() { return _dequeAverage(_rxBackTemps); } // C
    float _rxFrontTemp() { return _dequeAverage(_rxFrontTemps); } // C
    
    /// window size for moving temperature averages
    static const unsigned int TEMP_AVERAGING_LEN = 20;
    
    /// Deques to hold temperature lists
    std::deque<float> _procEnclosureTemps;
    std::deque<float> _procDrxTemps;
    std::deque<float> _txEnclosureTemps;
    std::deque<float> _rxTopTemps;
    std::deque<float> _rxBackTemps;
    std::deque<float> _rxFrontTemps;
    
    /// Video power monitors
    float _hTxPowerVideo;           // H tx pulse power, dBm
    float _vTxPowerVideo;           // V tx pulse power, dBm
    float _testTargetPowerVideo;    // test target power, dBm
    
    /// 5V power supply
    float _psVoltage;
    
    /// Valid pressure in waveguide outside the transmitter?
    bool _wgPressureValid;

    /// 100 MHz oscillator OK?
    bool _100MhzOscLocked;

    /// GPS time server alarm state
    bool _gpsClockAlarm;
};

KaMonitor::KaMonitor() {
    // Instantiate our private thread and start it.
    _privImpl = new KaMonitorPriv();
    _privImpl->start();
}

KaMonitor::~KaMonitor() {
    _privImpl->terminate();
    if (! _privImpl->wait(5000)) {
        ELOG << "KaMonitorPriv thread failed to stop in 5 seconds. Exiting anyway.";
    }
}

KaMonitorPriv::KaMonitorPriv() :
    QThread(),
    _mutex(QMutex::Recursive) {
}

KaMonitorPriv::~KaMonitorPriv() {
}

void
KaMonitorPriv::run() {
    // Since we have no event loop, allow thread termination via the terminate()
    // method.
    setTerminationEnabled(true);
  
    while (true) {
        _getNewValues();
        DLOG << std::fixed << std::setprecision(1) <<
            "TT: " << _testTargetPowerVideo << " dBm, " <<
            "V: " << _vTxPowerVideo << " dBm, " <<
            "H: " << _hTxPowerVideo << " dBm";
        DLOG << std::fixed << std::setprecision(1) << 
            "rx front: " << _rxFrontTemp() <<  " C, " << 
            "back: " << _rxBackTemp() << " C, " << 
            "top: " << _rxTopTemp() << " C";
        DLOG << std::fixed << std::setprecision(1) << 
            "tx enclosure: " << _txEnclosureTemp() << " C";
        DLOG << std::fixed << std::setprecision(1) << 
            "proc enclosure: " << _procEnclosureTemp() << " C, " << 
            "drx: " << _procDrxTemp() << " C";
        DLOG << std::fixed << std::setprecision(2) << 
            "5V PS: " << _psVoltage << " V";
        DLOG << "wg pressure valid: " << (_wgPressureValid ? "true" : "false");
        DLOG << "100 MHz oscillator locked: " << (_100MhzOscLocked ? "true" : "false");
        DLOG << "GPS time server alarm: " << (_gpsClockAlarm ? "true" : "false");
        usleep(1000000); // 1 s
    }
}

void
KaMonitorPriv::_getNewValues() {
    KaPmc730 & pmc730 = KaPmc730::theKaPmc730();
    // We get our data from analog channels 0-9 on the PMC-730 multi-IO card
    std::vector<float> analogData = pmc730.readAnalogChannels(0, 9);
    // Channels 0-2 give us RF power measurements
    _testTargetPowerVideo = _lookupQEAPower(analogData[0]);
    _vTxPowerVideo = _lookupQEAPower(analogData[1]);
    _hTxPowerVideo = _lookupQEAPower(analogData[2]);
    // Channels 3-8 give us various temperatures. The data are a bit noisy, so
    // we keep up to TEMP_AVERAGING_LEN samples so we can generate moving 
    // averages.
    _rxFrontTemps.push_back(_voltsToTemp(analogData[3]));
    _rxBackTemps.push_back(_voltsToTemp(analogData[4]));
    _rxTopTemps.push_back(_voltsToTemp(analogData[5]));
    _txEnclosureTemps.push_back(_voltsToTemp(analogData[6]));
    _procDrxTemps.push_back(_voltsToTemp(analogData[7]));
    _procEnclosureTemps.push_back(_voltsToTemp(analogData[8]));
    while (_rxFrontTemps.size() > TEMP_AVERAGING_LEN) {
        _rxFrontTemps.pop_front();
        _rxBackTemps.pop_front();
        _rxTopTemps.pop_front();
        _txEnclosureTemps.pop_front();
        _procDrxTemps.pop_front();
        _procEnclosureTemps.pop_front();
    }
    // Channel 9 gives us the voltage read from our 5V power supply
    _psVoltage = analogData[9];
    // We read the "waveguide pressure valid" signal from DIO line 5
    _wgPressureValid = pmc730.wgPressureValid();
    // Get the 100 MHz oscillator locked signal from DIO line 6
    // (Things are OK when this line is high)
    _100MhzOscLocked = pmc730.oscillator100MhzLocked();
    // Get the GPS time server alarm state
    _gpsClockAlarm = pmc730.gpsClockAlarm();
}

double
KaMonitorPriv::_lookupQEAPower(double voltage) {
    // If we're below the lowest voltage in the cal table, just return the
    // lowest power in the cal table.
    if (voltage < QEA_Cal[0][1]) {
        return(QEA_Cal[0][0]);
    }
    // If we're above the highest voltage in the cal table, just return the
    // highest power in the cal table.
    if (voltage > QEA_Cal[QEA_CalLen - 1][1]) {
        return(QEA_Cal[QEA_CalLen - 1][0]);
    }
    // OK, our voltage is somewhere in the table. Move up through the table, 
    // and interpolate between the two enclosing points.
    for (int i = 0; i < QEA_CalLen - 1; i++) {
        float powerLow = QEA_Cal[i][0];
        float vLow = QEA_Cal[i][1];
        float powerHigh = QEA_Cal[i + 1][0];
        float vHigh = QEA_Cal[i + 1][1];
        if (vHigh < voltage)
            continue;
        // Convert powers to linear space, then interpolate to our input voltage
        double powerLowLinear = pow(10.0, powerLow / 10.0);
        double powerHighLinear = pow(10.0, powerHigh / 10.0);
        double fraction = (voltage - vLow) / (vHigh - vLow);
        double powerLinear = powerLowLinear + 
            (powerHighLinear - powerLowLinear) * fraction;
        // Convert interpolated power back to dBm and return it.
        return(10.0 * log10(powerLinear));
    }
    // Oops if we get here...
    ELOG << __PRETTY_FUNCTION__ << ": Bad lookup for " << voltage << " V!";
    abort();
}

float
KaMonitorPriv::_dequeAverage(std::deque<float> & list) {
    unsigned int nPoints = list.size();
    if (nPoints == 0)
        return(-99.9);
        
    float sum = 0.0;
    for (unsigned int i = 0; i < nPoints; i++)
        sum += list[i];
    return(sum / nPoints); 
}
