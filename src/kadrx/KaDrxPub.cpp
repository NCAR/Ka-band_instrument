#include "KaDrxPub.h"
#include "KaAfc.h"
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

////////////////////////////////////////////////////////////////////////////////
KaDrxPub::KaDrxPub(
                Pentek::p7142sd3c& sd3c,
                KaChannel chanId,
                const KaDrxConfig& config,
                KaMerge *merge,
                KaTSWriter* tsWriter,
                bool publish,
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
     _publish(publish),
     _tsWriter(tsWriter),
     _tsDiscards(0),
     _ddsSamplePulses(tsLength),    // for now, set to the # of pulses we get from the 7142
     _ddsSeqInProgress(0),
     _ndxInDdsSample(0),
     _sampleNumber(0),
     _baseDdsHskp(),
     _numerator(0),
     _denominator(0),
     _g0Magnitude(-9999.0),
     _g0PowerDbm(-9999.0),
     _g0PhaseDeg(-9999.0),
     _g0IvalNorm(-9999.0),
     _g0QvalNorm(-9999.0),
     _g0FreqHz(-9999.0),
     _g0FreqCorrHz(-9999.0)
{
    // Bail out if we're not configured legally.
    if (! _configIsValid())
        abort();

    // Create our associated downconverter.
    double delay = _config.rcvr_gate0_delay();
    double width = _config.rcvr_pulse_width();
    
    // Special handling for the burst channel
    bool burstSampling = (_chanId == KA_BURST_CHANNEL);
    if (burstSampling) {
        delay = _config.burst_sample_delay();
        width = _config.burst_sample_width();
    }
    _down = sd3c.addDownconverter(_chanId, burstSampling, tsLength,
        delay, width, gaussianFile, kaiserFile, simPauseMS, simWavelength);

    if (burstSampling) {
        // Get the burst gate count calculated by the downconverter
        _nGates = _down->gates();
        std::cout << "Burst channel sampling " << _nGates << 
            " gates" << std::endl;
    }

    // Fill our DDS base housekeeping values from the configuration
    _config.fillDdsSysHousekeeping(_baseDdsHskp);
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

    // add data to the merge queue

    _addToMerge(reinterpret_cast<const int16_t *>(buf), pulseSeqNum);

    // publish to DDS if required
    if (_publish) {
      _publishDDS(buf, pulseSeqNum); 
    }

  }

}

////////////////////////////////////////////////////////////////////////////////
double KaDrxPub::_nowTime() {
  struct timeb timeB;
  ftime(&timeB);
  return timeB.time + timeB.millitm/1000.0;
}

////////////////////////////////////////////////////////////////////////////////

// 1970-01-01 00:00:00 UTC
static const ptime Epoch1970(boost::gregorian::date(1970, 1, 1), time_duration(0, 0, 0));

////////////////////////////////////////////////////////////////////////////////
void
  KaDrxPub::_addToMerge(const int16_t *iq, int64_t pulseSeqNum)
  
{

  time_duration timeFromEpoch = _down->timeOfPulse(pulseSeqNum) - Epoch1970;
  time_t timeSecs = timeFromEpoch.total_seconds();
  int nanoSecs = timeFromEpoch.fractional_seconds() * 1000;

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
void
KaDrxPub::_publishDDS(char* buf, int64_t pulseSeqNum) {
    
	// bufPos is now pointing to the pulse data
	// data length in bytes: 2-byte I and 2-byte Q for each gate
	int datalen = 4 * _nGates;

	// copy this pulse into our DDS sample-in-progress,
	// and publish it when it's full.
        // Make sure we have a DDS sample to fill
        if (! _ddsSeqInProgress) {
          // Get a free sample
          _ddsSeqInProgress = _tsWriter->getEmptyItem();
          // If no sample was available, discard everything we have and
          // return.
          if (! _ddsSeqInProgress) {
            std::cerr << "Channel " << _chanId <<
              " dropped data with no DDS samples available!" << std::endl;
            _tsDiscards++;
            return;
          }
          _ddsSeqInProgress->sampleNumber = _sampleNumber++;
          _ddsSeqInProgress->chanId = _chanId;
          _ddsSeqInProgress->tsList.length(_ddsSamplePulses);
          _ndxInDdsSample = 0;
        }
        
        //
        // Copy this pulse into our DDS sample in progress
        //
        RadarDDS::KaTimeSeries & ts = _ddsSeqInProgress->tsList[_ndxInDdsSample++];
        // Copy our fixed metadata into this pulse
        ts.hskp = _baseDdsHskp;
        // Then fill the non-fixed metadata
        ts.data.length(_nGates * 2);   // I and Q for each gate, length is count of shorts
        ts.hskp.chanId = _chanId;
        ts.prt_seq_num = 1;   // single-PRT only for now
        ts.pulseNum = pulseSeqNum;
        time_duration timeFromEpoch = _down->timeOfPulse(pulseSeqNum) - Epoch1970;
        // Calculate the timetag, which is usecs since 1970-01-01 00:00:00 UTC
        ts.hskp.timetag = timeFromEpoch.total_seconds() * 1000000LL +
          (timeFromEpoch.fractional_seconds() * 1000000LL) /
          time_duration::ticks_per_second();
        // Copy the per-gate data from the incoming buffer to the DDS
        // sample data space.
        memmove(ts.data.get_buffer(), buf, datalen);
        
        // Publish _ddsSeqInProgress if it's full
        if (_ndxInDdsSample == _ddsSamplePulses) {
          // publish it
          _tsWriter->publishItem(_ddsSeqInProgress);
          _ddsSeqInProgress = 0;
        }
        
}

////////////////////////////////////////////////////////////////////////////////
unsigned long
KaDrxPub::tsDiscards() {
	unsigned long retval = _tsDiscards;
	_tsDiscards = 0;
	return retval;
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

// @TODO remove this when we're done with the temporary BurstFile stuff below
//static FILE* BurstFile = 0;

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
    // Compute cross product over middle 20 samples (frequency discriminator) 
    // using moving coherent average to reduce variance
    // i contains inphase samples; q contains quadrature samples
    for (unsigned int g = 2; g <= 20; g++) {
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

    double ival = i[0];
    double qval = q[0];
    double g0Mag = (ival * ival + qval * qval) / (65536. * 65536.); // units of V^2
    double g0MagDb = 10 * log10(g0Mag);
    double g0Power = g0Mag * g0Mag;
    if (! (pulseSeqNum % 5000)) {
        DLOG << "At pulse " << pulseSeqNum << ": freq corr. " <<
            freqCorrection << " Hz, g0 magnitude " << g0Mag << " (" <<
            g0MagDb << " dB)";
    }
    _g0Magnitude = g0Mag;
    _g0PowerDbm = 10.0 * log10(g0Power);
    _g0PhaseDeg = _argDeg(ival, qval);
    _g0IvalNorm = ival / g0Mag;
    _g0QvalNorm = qval / g0Mag;
    _g0FreqCorrHz = freqCorrection;
    _g0FreqHz = _config.rcvr_cntr_freq() + freqCorrection;
    
    // Ship the G0 power and frequency offset values to the AFC
    KaAfc::theAfc().newXmitSample(g0Mag, freqCorrection, pulseSeqNum);

//    -----------------------------------------------------------------------------------
//
//    // Phase correction -- runs every hit
//    // Correct Phase of original signal by normalized initial vector
//    // Ih and Qh contain Horizontally received data; Iv and Qv contained vertically received data
//
//    for(i=1:1:ngates)
//    // Correct Horizontal Channel    
//
//        ibcorrh(i) = ib(1)*invg0mag*Ih(i) - qb(1)*invg0mag*Qh(i);
//        qbcorrh(i) = qb(1)*invg0mag*Ih(i) + ib(1)*invg0mag*Qh(i);
//
//    // Correct Vertical Channel  
//
//        ibcorrv(i) = ib(1)*invg0mag*Iv(i) - qb(1)*invg0mag*Qv(i);
//        qbcorrv(i) = qb(1)*invg0mag*Iv(i) + ib(1)*invg0mag*Qv(i);
//
//    end
    
//    // Write data directly to a simple CSV text file
//    if (! BurstFile) {
//        char ofilename[80];
//        sprintf(ofilename, "Ka_burst_%lld.csv", timetag / 1000000);
//        BurstFile = fopen(ofilename, "a");
//        if (! BurstFile) {
//            std::cerr << "Error opening burst CSV file: " << ofilename <<
//                    std::endl;
//            exit(1);
//        }
//    }
//    double dtime = timetag * 1.0e-6;
//    fprintf(BurstFile, "%.6f", dtime);
//    for (unsigned int g = 0; g < _nGates; g++) {
//        fprintf(BurstFile, ",%d,%d", iqData[2 * g], iqData[2 * g + 1]);
//    }
//    fprintf(BurstFile, "\n");
//    fflush(BurstFile);
}

/*
 * Angle conversions
 */

#ifndef RAD_TO_DEG
#define RAD_TO_DEG 57.29577951308092
#endif

#ifndef DEG_PER_RAD
#define DEG_PER_RAD 57.29577951308092
#endif

#ifndef DEG_TO_RAD
#define DEG_TO_RAD 0.01745329251994372
#endif

#ifndef RAD_PER_DEG
#define RAD_PER_DEG 0.01745329251994372
#endif

// compute arg in degrees

double KaDrxPub::_argDeg(double ival, double qval)
  
{
  double arg = 0.0;
  if (ival != 0.0 || qval != 0.0) {
    arg = atan2(qval, ival);
  }
  arg *= RAD_TO_DEG;
  return arg;
}

