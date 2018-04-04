/*
 * KaMonitor.cpp
 *
 *  Created on: Oct 29, 2010
 *      Author: burghart
 */

#include "KaMonitor.h"
#include "KaOscControl.h"
#include "KaPmc730.h"

#include <XmitClient.h>

#include <QDateTime>
#include <QTimer>
#include <QMutexLocker>

#include <iomanip>
#include <cmath>
#include <vector>
#include <deque>

#include <logx/Logging.h>

LOGGING("KaMonitor")

/* 
 * Test Target for Quinstar QEA crystal RF power detector calibration measurements from 
 * 11/04/2010, input power in dBm vs. output volts.
 */

static const QEA_Cal_Val  QEA_Cal_TT[] = {
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
static const int QEA_CalLen_TT = (sizeof(QEA_Cal_TT) / (sizeof(QEA_Cal_Val)));

// Ka Band Quinstar Diode Detector with Coax/ WG Adapter
// calibration 2011/9/27 @ Dynamo
// Input Sig Gen (34.7GHz),Measured PWR,,Measured mVPWR

static const QEA_Cal_Val  QEA_Cal_HChan[] = {

{-26.6, 0.0234},
{-25.06, 0.0288},
{-23.82, 0.0344},
{-22.74, 0.0404},
{-21.76, 0.0484},
{-20.71, 0.0564},
{-19.67, 0.0672},
{-18.66, 0.08},
{-17.64, 0.0936},
{-16.63, 0.1096},
{-15.64, 0.1316},
{-14.35, 0.16},
{-13.36, 0.1856},
{-12.38, 0.216},
{-11.41, 0.252},
{-10.43, 0.288},
{-9.46, 0.328},
{-8.49, 0.38},
{-7.51, 0.428},
{-6.54, 0.488},
{-5.56, 0.56},
{-4.3, 0.672},
{-3.32, 0.752},
{-2.36, 0.848},
{-1.38, 0.96},
{-0.42, 1.072},
{0.55, 1.232},
{1.52, 1.392},
{2.49, 1.552},
{3.47, 1.712},
{4.44, 1.904},
{5.59, 2.16},
{6.56, 2.36},
{7.53, 2.56},
{8.5, 2.76},
{9.48, 3.04},
};
static const int QEA_CalLen_HChan = (sizeof(QEA_Cal_HChan) / (sizeof(QEA_Cal_Val)));

// no calibration for V - return -999
static const QEA_Cal_Val  QEA_Cal_VChan[] = {
{-999, 5.00},
{999, 10.0},
};

static const int QEA_CalLen_VChan = (sizeof(QEA_Cal_VChan) / (sizeof(QEA_Cal_Val)));

KaMonitor::KaMonitor(std::string xmitdHost, int xmitdPort) :
    QThread(),
    _mutex(QMutex::Recursive),
    _procEnclosureTemps(),
    _procDrxTemps(),
    _txEnclosureTemps(),
    _rxTopTemps(),
    _rxBackTemps(),
    _rxFrontTemps(),
    _hTxPowerRaw(0.0),
    _vTxPowerRaw(0.0),
    _testTargetPowerRaw(0.0),
    _psVoltage(0.0),
    _wgPressureGood(false),
    _locked100MHz(false),
    _gpsTimeServerGood(false),
    _osc0Frequency(0),
    _osc1Frequency(0),
    _osc2Frequency(0),
    _osc3Frequency(0),
    _xmitClient(xmitdHost, xmitdPort),
    _xmitStatus() {
}

KaMonitor::~KaMonitor() {
    terminate();
    if (! wait(5000)) {
        ELOG << "KaMonitor thread failed to stop in 5 seconds. Exiting anyway.";
    }
}

float
KaMonitor::procEnclosureTemp() const {
    QMutexLocker locker(&_mutex);
    return _dequeAverage(_procEnclosureTemps);
}

float
KaMonitor::procDrxTemp() const {
    QMutexLocker locker(&_mutex);
    return _dequeAverage(_procDrxTemps);
}

float
KaMonitor::txEnclosureTemp() const {
    QMutexLocker locker(&_mutex);
    return _dequeAverage(_txEnclosureTemps);
}

float
KaMonitor::rxTopTemp() const {
    QMutexLocker locker(&_mutex);
    return _dequeAverage(_rxTopTemps);
}

float
KaMonitor::rxBackTemp() const {
    QMutexLocker locker(&_mutex);
    return _dequeAverage(_rxBackTemps);
}

float
KaMonitor::rxFrontTemp() const {
    QMutexLocker locker(&_mutex);
    return _dequeAverage(_rxFrontTemps);
}

float
KaMonitor::hTxPowerRaw() const {
    QMutexLocker locker(&_mutex);
    return _hTxPowerRaw;
}

float
KaMonitor::vTxPowerRaw() const {
    QMutexLocker locker(&_mutex);
    return _vTxPowerRaw;
}

float
KaMonitor::testTargetPowerRaw() const {
    QMutexLocker locker(&_mutex);
    return _testTargetPowerRaw;
}

float
KaMonitor::psVoltage() const {
    QMutexLocker locker(&_mutex);
    return _psVoltage;
}

bool
KaMonitor::wgPressureGood() const {
    QMutexLocker locker(&_mutex);
    return _wgPressureGood;
}

bool
KaMonitor::locked100MHz() const {
    QMutexLocker locker(&_mutex);
    return _locked100MHz;
}

bool
KaMonitor::gpsTimeServerGood() const {
    QMutexLocker locker(&_mutex);
    return _gpsTimeServerGood;
}

XmitClient::XmitStatus
KaMonitor::transmitterStatus() const {
    QMutexLocker locker(&_mutex);
    return _xmitStatus;
}

uint64_t
KaMonitor::osc0Frequency() const {
    QMutexLocker locker(&_mutex);
    return(_osc0Frequency);
}

uint64_t
KaMonitor::osc1Frequency() const {
    QMutexLocker locker(&_mutex);
    return(_osc1Frequency);
}

uint64_t
KaMonitor::osc2Frequency() const {
    QMutexLocker locker(&_mutex);
    return(_osc2Frequency);
}

uint64_t
KaMonitor::osc3Frequency() const {
    QMutexLocker locker(&_mutex);
    return(_osc3Frequency);
}

uint64_t
KaMonitor::derivedTxFrequency() const {
    QMutexLocker locker(&_mutex);
    return(2 * _osc2Frequency + _osc0Frequency + _osc3Frequency + 25000000);
}

void
KaMonitor::run() {
    QDateTime lastUpdateTime(QDateTime::fromTime_t(0).toUTC());
    
    // Since we have no event loop, allow thread termination via the terminate()
    // method.
    setTerminationEnabled(true);
  
    while (true) {
        // Sleep if necessary to get ~1 second between updates
        QDateTime now = QDateTime::currentDateTime().toUTC();
        uint64_t msecsSinceUpdate = uint64_t(lastUpdateTime.daysTo(now)) * 1000 * 86400 + 
            lastUpdateTime.time().msecsTo(now.time());
        lastUpdateTime = now;
        if (msecsSinceUpdate < 1000) {
            usleep((1000 - msecsSinceUpdate) * 1000);
        }
        
        // Get new values from the multi-IO card and from ka_xmitd
        _getMultiIoValues();
        
        // Get transmitter status.
        _getXmitStatus();
        
        // Get oscillator frequencies
        _getOscFrequencies();
    }
}

void
KaMonitor::_getMultiIoValues() {
    QMutexLocker locker(&_mutex);

    KaPmc730 & pmc730 = KaPmc730::theKaPmc730();
    // Get data from analog channels 0-9 on the PMC-730 multi-IO card
    std::vector<float> analogData = pmc730.readAnalogChannels(0, 9);
    // Channels 0-2 give us RF power measurements
    _testTargetPowerRaw = _lookupQEAPower(QEA_Cal_TT,  QEA_CalLen_TT, analogData[0]);
    _vTxPowerRaw = _lookupQEAPower(QEA_Cal_VChan, QEA_CalLen_VChan, analogData[1]);
    _hTxPowerRaw = _lookupQEAPower(QEA_Cal_HChan,QEA_CalLen_HChan,  analogData[2]);
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
    _wgPressureGood = pmc730.wgPressureValid();
    
    // Get the 100 MHz oscillator locked signal from DIO line 6
    // (Things are OK when this line is high)
    _locked100MHz = pmc730.oscillator100MhzLocked();
    
    // Get the GPS time server alarm state
    _gpsTimeServerGood = ! pmc730.gpsClockAlarm();

    DLOG << std::fixed << std::setprecision(1) <<
        "TT: " << _testTargetPowerRaw << " dBm, " <<
        "V: " << _vTxPowerRaw << " dBm, " <<
        "H: " << _hTxPowerRaw << " dBm";
    DLOG << std::fixed << std::setprecision(1) << 
        "rx front: " << rxFrontTemp() <<  " C, " << 
        "back: " << rxBackTemp() << " C, " << 
        "top: " << rxTopTemp() << " C";
    DLOG << std::fixed << std::setprecision(1) << 
        "tx enclosure: " << txEnclosureTemp() << " C";
    DLOG << std::fixed << std::setprecision(1) << 
        "proc enclosure: " << procEnclosureTemp() << " C, " << 
        "drx: " << procDrxTemp() << " C";
    DLOG << std::fixed << std::setprecision(2) << 
        "5V PS: " << _psVoltage << " V";
    DLOG << "wg pressure OK: " << (_wgPressureGood ? "true" : "false");
    DLOG << "100 MHz oscillator locked: " << (_locked100MHz ? "true" : "false");
    DLOG << "GPS time server OK: " << (_gpsTimeServerGood ? "true" : "false");
}

void
KaMonitor::_getXmitStatus() {
    // Get the status first (which may take a little while under some 
    // circumstances), then get the mutex and set our member variable. This 
    // way, we don't have the mutex locked very long at all....
    XmitClient::XmitStatus xmitStatus;
    _xmitClient.getStatus(xmitStatus);
    
    QMutexLocker locker(&_mutex);
    _xmitStatus = xmitStatus;
}

void
KaMonitor::_getOscFrequencies() {
    QMutexLocker locker(&_mutex);
    KaOscControl::theControl().getOscFrequencies(_osc0Frequency,
            _osc1Frequency, _osc2Frequency, _osc3Frequency);
    
    DLOG << std::fixed << "oscillator frequencies - " <<
        std::setprecision(4) << "0: " << _osc0Frequency / 1.0e9 << " GHz" <<
        std::setprecision(2) << ", 1: " << _osc1Frequency / 1.0e6 << " MHz" <<
        std::setprecision(3) << ", 2: " << _osc2Frequency / 1.0e9 << " GHz" <<
        std::setprecision(2) << ", 3: " << _osc3Frequency / 1.0e6 << " MHz";
}

double
KaMonitor::_lookupQEAPower(const QEA_Cal_Val *qea_cal, unsigned len, double voltage) {
    // If we're below the lowest voltage in the cal table, just return the
    // lowest power in the cal table.
    if (voltage < qea_cal[0].voltage) {
        return(qea_cal[0].power);
    }
    // If we're above the highest voltage in the cal table, just return the
    // highest power in the cal table.
    if (voltage > qea_cal[len - 1].voltage) {
        return(qea_cal[len - 1].power);
    }
    // OK, our voltage is somewhere in the table. Move up through the table, 
    // and interpolate between the two enclosing points.
    for (unsigned i = 0; i < len - 1; i++) {
        float powerLow = qea_cal[i].power;
        float vLow = qea_cal[i].voltage;
        float powerHigh = qea_cal[i + 1].power;
        float vHigh = qea_cal[i + 1].voltage;
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
KaMonitor::_dequeAverage(const std::deque<float> & list) {
    unsigned int nPoints = list.size();
    if (nPoints == 0)
        return(-99.9);
        
    float sum = 0.0;
    for (unsigned int i = 0; i < nPoints; i++)
        sum += list.at(i);
    return(sum / nPoints); 
}
