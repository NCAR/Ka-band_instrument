#include "KaDrxPub.h"
#include <sys/timeb.h>
#include <cstdlib>
#include <iostream>
#include <iomanip>

using namespace boost::posix_time;

////////////////////////////////////////////////////////////////////////////////
KaDrxPub::KaDrxPub(
                Pentek::p7142sd3c& sd3c,
                KaChannel chanId,
                const KaDrxConfig& config,
                KaTSWriter* tsWriter,
                bool publish,
                int tsLength,
                std::string gaussianFile,
                std::string kaiserFile,
                bool simulate,
                double simPauseMS,
                int simWavelength) :
     QThread(),
     _sd3c(sd3c),
     _chanId(chanId),
     _down(0),
     _gates(config.gates()),
     _publish(publish),
     _tsWriter(tsWriter),
     _tsDiscards(0),
     _ddsSamplePulses(tsLength),    // for now, set to the # of pulses we get from the 7142
     _ddsSeqInProgress(0),
     _ndxInDdsSample(0)
{
    // Bail out if we're not configured legally.
    if (! _configIsValid())
        abort();

    // Create our associated downconverter.
    double delay = config.rcvr_gate0_delay();
    double width = config.rcvr_pulse_width();
    if (_chanId == KA_BURST_CHANNEL) {
        _gates = -1; // have downconverter calculate # of gates for burst
        delay = config.burst_sample_delay();
        width = config.burst_sample_width();
    }
    _down = sd3c.addDownconverter(_chanId, _gates, 1, tsLength,
        delay, width, gaussianFile, kaiserFile, simPauseMS, simWavelength);

    if (_chanId == KA_BURST_CHANNEL) {
        // Get the burst gate count calculated by the downconverter
        _gates = _down->gates();
        std::cout << "Burst channel sampling " << _gates << 
            " gates" << std::endl;
    }

    // Fill our DDS base housekeeping values from the configuration
    config.fillDdsSysHousekeeping(_baseDdsHskp);
}

////////////////////////////////////////////////////////////////////////////////
KaDrxPub::~KaDrxPub() {

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
  while (1) {
	unsigned int pulsenum;
	char* buf = _down->getBeam(pulsenum);
    publishDDS(buf, pulsenum); 
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

// @TODO remove this when we're done with the temporary burst file stuff below
static FILE* BurstFile = 0;

////////////////////////////////////////////////////////////////////////////////
void
KaDrxPub::publishDDS(char* buf, unsigned int pulsenum) {
    

	// bufPos is now pointing to the pulse data
	// data length in bytes: 2-byte I and 2-byte Q for each gate
	int datalen = 4 * _gates;

	// If we're publishing, copy this pulse into our DDS sample-in-progress,
	// and publish it when it's full.
	if (_publish) {
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
		ts.data.length(_gates * 2);   // I and Q for each gate, length is count of shorts
		ts.hskp.chanId = _chanId;
		ts.prt_seq_num = 1;   // single-PRT only for now
		ts.pulseNum = pulsenum;
		time_duration timeFromEpoch = _down->timeOfPulse(pulsenum) - Epoch1970;
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

		// @TODO remove this
		// For the burst channel, write data directly to a simple CSV text file
		if (_chanId == KA_BURST_CHANNEL) {
		    if (! BurstFile) {
		        char ofilename[80];
		        sprintf(ofilename, "Ka_burst_%d.csv", timeFromEpoch.total_seconds());
		        BurstFile = fopen(ofilename, "a");
		        if (! BurstFile) {
		            std::cerr << "Error opening burst CSV file: " << ofilename <<
		                    std::endl;
		            exit(1);
		        }
		    }
		    double dtime = ts.hskp.timetag * 1.0e-6;
		    fprintf(BurstFile, "%.6f", dtime);
		    int16_t * shortdata = (int16_t *)buf;
		    for (unsigned int g = 0; g < _gates; g++) {
		        fprintf(BurstFile, ",%d,%d", shortdata[2 * g], shortdata[2 * g + 1]);
		    }
		    fprintf(BurstFile, "\n");
		    fflush(BurstFile);
		}
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
    if (_gates < 1 || _gates > 1023) {
        std::cerr << "gates is " << _gates <<
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
        if (_sd3c.prtCounts() <= ((_gates + 1) * _sd3c.txPulseWidthCounts())) {
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
