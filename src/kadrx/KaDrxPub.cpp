#include "KaDrxPub.h"
#include "KaOscControl.h"
#include "KaMerge.h"
#include "PulseData.h"
#include "BurstData.h"
#include <logx/Logging.h>
#include <sys/timeb.h>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <iomanip>

using namespace boost::posix_time;

LOGGING("KaDrxPub")

// 1970-01-01 00:00:00 UTC
static const ptime Epoch1970(boost::gregorian::date(1970, 1, 1), time_duration(0, 0, 0));

////////////////////////////////////////////////////////////////////////////////
KaDrxPub::KaDrxPub(
                Pentek::p7142sd3c& sd3c,
                KaChannel chanId,
                const KaDrxConfig& config,
                KaMerge *merge,
                int tsLength,
                std::string gaussianFile,
                std::string kaiserFile,
                double simPauseMS,
                int simWavelength) :
     QThread(),
     _sd3c(sd3c),
     _chanId(chanId),
     _config(config),
     _down(0),
     _nGates(config.gates()),
     _merge(merge),
     _pulseData(NULL),
     _burstData(NULL),
     _doAfc(_config.afc_enabled()),
     _numerator(0),
     _denominator(0),
     _g0Magnitude(-9999.0),
     _g0PowerDbm(-9999.0),
     _g0PhaseDeg(-9999.0),
     _g0IvalNorm(-9999.0),
     _g0QvalNorm(-9999.0),
     _g0FreqHz(-9999.0),
     _g0FreqCorrHz(-9999.0),
     _doPeiFile(config.write_pei_files()),
     _peiFile(0),
     _maxPeiGates(config.max_pei_gates())
{
    // Bail out if we're not configured legally.
    if (! _configIsValid())
        abort();

    // scaling between A2D counts and volts

    _iqScaleForMw = _config.iqcount_scale_for_mw();

    // Create our associated downconverter.
    double delay = _config.rcvr_gate0_delay();
    double width = _config.rcvr_pulse_width();
    
    // Special handling for the burst channel
    bool burstSampling = (_chanId == KA_BURST_CHANNEL);
    if (burstSampling) {
        delay = _config.burst_sample_delay();
        width = _config.burst_sample_width();
    }
    
    _down = _sd3c.addDownconverter(_chanId, burstSampling, tsLength,
        delay, width, gaussianFile, kaiserFile, simPauseMS, simWavelength, 
        !(_config.external_clock()));

    if (burstSampling) {
        // Get the burst gate count calculated by the downconverter
        _nGates = _down->gates();
        std::cout << "Burst channel sampling " << _nGates << 
            " gates" << std::endl;
    }

}

////////////////////////////////////////////////////////////////////////////////
KaDrxPub::~KaDrxPub() {

  if (_pulseData) {
    delete _pulseData;
  }

  if (_burstData) {
    delete _burstData;
  }

}

////////////////////////////////////////////////////////////////////////////////
void KaDrxPub::run() {

  int bl = _down->beamLength();
  
  std::cout << "Channel " << _chanId << " beam length is " << bl <<
    ", waiting for data..." << std::endl;

  // Since we have no event loop, allow thread termination via the terminate()
  // method.
  setTerminationEnabled(true);
  
  // start the loop. The thread will block on getBeam()
  while (true) {
    
    int64_t pulseSeqNum;

    char* buf = _down->getBeam(pulseSeqNum);

    // derive burst values if this is the burst channel
    if (_chanId == KA_BURST_CHANNEL) {
      _handleBurst(reinterpret_cast<const int16_t *>(buf), pulseSeqNum);
    }

    //
    // If requested, write IQ data to a simple "Pei format" time series file.
    // Pei format is written as a series of pulses, where each pulse is
    // in the form:
    // 
    // time|PRF|pulse_width|n_gates|I(0)|Q(0)|I(1)|Q(1)|...|I(n_gates-1)|Q(n_gates-1)
    //
    // time - pulse time in seconds since 1970-01-01 00:00:00 UTC, 8-byte IEEE float
    // PRF - PRF in Hz, 4-byte IEEE float
    // pulse_width - receiver pulse width in seconds, 4-byte IEEE float
    // n_gates - gate count, 4-byte int
    // I(n) - inphase counts for gate n, 2-byte int
    // Q(n) - quadrature counts for gate n, 2-byte int
    //
    // All values are written little-endian
    //
    if (_doPeiFile) {
        if (! _peiFile) {
            char filename[256];
            ptime pulsePtime = _sd3c.timeOfPulse(pulseSeqNum);
            boost::gregorian::date pulseDate = pulsePtime.date();
            time_duration pulseTime = pulsePtime.time_of_day();
            sprintf(filename, "chan%d_%4d%02d%02d_%02d%02d%02d_pei", _chanId,
                int(pulseDate.year()), int(pulseDate.month()), int(pulseDate.day()),
                pulseTime.hours(), pulseTime.minutes(), pulseTime.seconds());
            if ((_peiFile = fopen(filename, "a+")) < 0) {
                ELOG << "Error opening Pei file '" << filename;
                abort();
            }
        }

        int16_t *iqData = (int16_t*)buf;
        time_duration timeFromEpoch = _sd3c.timeOfPulse(pulseSeqNum) - Epoch1970;
    
        double timeSecs = timeFromEpoch.total_seconds() + 
            double(timeFromEpoch.fractional_seconds()) / time_duration::ticks_per_second();
        fwrite((char*)&timeSecs, sizeof(double), 1, _peiFile);
    
        float prf = 1.0 / _sd3c.prt();
        fwrite((char*)&prf, sizeof(float), 1, _peiFile);
        
        float pulseWidth = _down->rcvrPulseWidth();
        fwrite((char*)&pulseWidth, sizeof(float), 1, _peiFile);
        
        int peiGates = (_nGates < _maxPeiGates) ? _nGates : _maxPeiGates;
        fwrite((char*)&peiGates, sizeof(int), 1, _peiFile);
        
        // Write I and Q (4 bytes total per gate) for all gates
        fwrite((char*)iqData, 4, peiGates, _peiFile);
    }

    _addToMerge(reinterpret_cast<const int16_t *>(buf), pulseSeqNum);

  }

}

////////////////////////////////////////////////////////////////////////////////
double KaDrxPub::_nowTime() {
  struct timeb timeB;
  ftime(&timeB);
  return timeB.time + timeB.millitm/1000.0;
}

////////////////////////////////////////////////////////////////////////////////
void
  KaDrxPub::_addToMerge(const int16_t *iq, int64_t pulseSeqNum)
  
{

  time_duration timeFromEpoch = _sd3c.timeOfPulse(pulseSeqNum) - Epoch1970;
  time_t timeSecs = timeFromEpoch.total_seconds();
  int nanoSecs = timeFromEpoch.fractional_seconds() * 
          (1000000000 / time_duration::ticks_per_second());

  if (_chanId == KA_BURST_CHANNEL) {

    // allocate on first time

    if (_burstData == NULL) {
      _burstData = new BurstData;
    }

    // set data in burst object

    _burstData->set(pulseSeqNum, timeSecs, nanoSecs,
                    _g0Magnitude, _g0PowerDbm, _g0PhaseDeg,
                    _g0IvalNorm, _g0QvalNorm,
                    _g0FreqHz, _g0FreqCorrHz,
                    _nGates, iq);

    // we write to the merge queue using one object,
    // and get back another for reuse

    _burstData = _merge->writeBurst(_burstData);

  } else {

    // allocate on first time

    if (_pulseData == NULL) {
      _pulseData = new PulseData;
    }

    // set data in pulse object

    _pulseData->set(pulseSeqNum, timeSecs, nanoSecs,
                    _chanId,
                    _nGates, iq);

    // we write to the merge queue using one object,
    // and get back another for reuse

    if (_chanId == KA_H_CHANNEL) {
      _pulseData = _merge->writePulseH(_pulseData);
    } else if (_chanId == KA_V_CHANNEL) {
      _pulseData = _merge->writePulseV(_pulseData);
    }

  }

}
    
////////////////////////////////////////////////////////////////////////////////
bool
KaDrxPub::_configIsValid() const {
    bool valid = true;
    
    // gate count must be in the interval [1,1023]
    if (_nGates < 1 || _nGates > 1023) {
        std::cerr << "gates is " << _nGates <<
            "; it must be greater than 0 and less than 1024." << std::endl;
        valid = false;
    }
    
    // Tests for non-burst data channels
    if (_chanId != KA_BURST_CHANNEL) {
        // PRT must be a multiple of the pulse width
        if (_sd3c.prtCounts() % _sd3c.txPulseWidthCounts()) {
            std::cerr << "PRT is " << _sd3c.prt() << " (" << 
                _sd3c.prtCounts() << ") and pulse width is " << 
                _sd3c.txPulseWidth() << 
                " (" << _sd3c.txPulseWidthCounts() << 
                "): PRT must be an integral number of pulse widths." << std::endl;
            valid = false;
        }
        // PRT must be longer than (gates + 1) * pulse width
        if (_sd3c.prtCounts() <= ((_nGates + 1) * _sd3c.txPulseWidthCounts())) {
            std::cerr << "PRT must be greater than (gates+1)*(pulse width)." <<
                    std::endl;
            valid = false;
        }
        // Make sure the Pentek's FPGA is using DDC10DECIMATE
        if (_sd3c.ddcType() != Pentek::p7142sd3c::DDC10DECIMATE) {
            std::cerr << "The Pentek FPGA is using DDC type " << 
                    _sd3c.ddcTypeName() << 
                    ", but Ka requires DDC10DECIMATE." << std::endl;
            valid = false;
        }
    }
    return valid;
}

////////////////////////////////////////////////////////////////////////////////
void
KaDrxPub::_handleBurst(const int16_t * iqData, int64_t pulseSeqNum) {
    // initialize variables
    const double DIS_WT = 0.01;

    // Separate I and Q data
    double i[_nGates];
    double q[_nGates];
    double num = 0;
    double den = 0;
    for (unsigned int g = 0; g < _nGates; g++) {
        i[g] = iqData[2 * g];
        q[g] = iqData[2 * g + 1];
    }

    // Frequency discriminator -- runs every hit
    // Compute cross product over middle 16 samples (frequency discriminator) 
    // using moving coherent average to reduce variance
    // i contains inphase samples; q contains quadrature samples
    for (unsigned int g = 2; g <= 17; g++) {
        double a = i[g] + i[g + 1];
        double b = q[g] + q[g + 1];
        double c = i[g + 2] + i[g + 1];
        double d = q[g + 2] + q[g + 1];

        num += a * d - b * c; // cross product
        den += a * c + b * d; // normalization factor proportional to G0 magnitude
    }
    
    // _numerator and _denominator are weighted averages over time, with
    // recent data weighted highest
    _numerator *= (1 - DIS_WT);
    _numerator += DIS_WT * num;
    
    _denominator *= (1 - DIS_WT);
    _denominator += DIS_WT * den;

    double normCrossProduct = _numerator / _denominator;  // normalized cross product proportional to frequency change
    double freqCorrection = 8.0e6 * normCrossProduct; // experimentally determined scale factor to convert correction to Hz

    double ival = i[9] / _iqScaleForMw;
    double qval = q[9] / _iqScaleForMw;
    double g0Power = ival * ival + qval * qval; // mW
    double g0PowerDbm = 10 * log10(g0Power);
    if (! (pulseSeqNum % 5000)) {
      DLOG << "At pulse " << pulseSeqNum << ": freq corr. " <<
        freqCorrection << " Hz, g0 power " << g0Power << " (" <<
        g0PowerDbm << " dBm)";
    }
    _g0Magnitude = sqrt(g0Power);
    _g0PowerDbm = g0PowerDbm;
    _g0PhaseDeg = _argDeg(ival, qval);
    _g0IvalNorm = ival / _g0Magnitude;
    _g0QvalNorm = qval / _g0Magnitude;
    _g0FreqCorrHz = freqCorrection;
    _g0FreqHz = _config.rcvr_cntr_freq() + freqCorrection;
    
    // Pass stuff on to oscillator control if we're doing AFC
    if (_doAfc) {
        KaOscControl::theControl().newXmitSample(g0Power, freqCorrection, 
            pulseSeqNum);
    }
}

// compute arg in degrees

double KaDrxPub::_argDeg(double ival, double qval)
  
{
  double arg = atan2(qval, ival) * _RAD_TO_DEG;
  return arg;
}

