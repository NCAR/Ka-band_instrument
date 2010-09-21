#include "KaDrxPub.h"
#include <sys/timeb.h>
#include <cstdlib>
#include <iostream>
#include <iomanip>

using namespace boost::posix_time;

//////////////////////////////////////////////////////////////////////////////////
KaDrxPub::KaDrxPub(
                const KaDrxConfig& config,
                KaTSWriter* tsWriter,
                bool publish,
                int tsLength,
                std::string devName,
                int chanId,
                std::string gaussianFile,
                std::string kaiserFile,
                bool freeRun,
                bool simulate,
                double simPauseMS,
                int simWavelength) :
    p7142sd3cdn(devName,
             chanId,
             config.gates(),
             1,
             tsLength,
             config.rcvr_gate0_delay(),
             config.tx_delay(),
             config.prt1(),
             config.prt2(),
             config.rcvr_pulse_width(),
             (config.staggered_prt() == KaDrxConfig::UNSET_BOOL) ? false : config.staggered_prt(),
             config.timer_delays(),
             config.timer_widths(),
             freeRun,
             gaussianFile,
             kaiserFile,
             simulate,
             simPauseMS,
             simWavelength,
             false),
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

    // Fill our DDS base housekeeping values from the configuration
    config.fillDdsSysHousekeeping(_baseDdsHskp);
}

//////////////////////////////////////////////////////////////////////////////////
KaDrxPub::~KaDrxPub() {

}

//////////////////////////////////////////////////////////////////////////////////
void KaDrxPub::run() {

  static unsigned short int ttl_toggle = 0;

  int bl = beamLength();
  
  std::cout << "Channel " << _chanId << " beam length is " << bl <<
    ", waiting for data..." << std::endl;

  // start the loop. The thread will block on getBeam()
  while (1) {

	unsigned int pulsenum;
	char* buf = getBeam(pulsenum);

    ttl_toggle = ~ttl_toggle;
    TTLOut(ttl_toggle);

    publishDDS(buf, pulsenum);
    
  }
}

///////////////////////////////////////////////////////////
double KaDrxPub::_nowTime() {
  struct timeb timeB;
  ftime(&timeB);
  return timeB.time + timeB.millitm/1000.0;
}

///////////////////////////////////////////////////////////

// 1970-01-01 00:00:00 UTC
static const ptime Epoch1970(boost::gregorian::date(1970, 1, 1), time_duration(0, 0, 0));

///////////////////////////////////////////////////////////
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
		time_duration timeFromEpoch = timeOfPulse(pulsenum) - Epoch1970;
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
}

//////////////////////////////////////////////////////////////////////////////////
unsigned long
KaDrxPub::tsDiscards() {
	unsigned long retval = _tsDiscards;
	_tsDiscards = 0;
	return retval;
}

//////////////////////////////////////////////////////////////////////////////////
bool
KaDrxPub::_configIsValid() const {
    bool valid = true;
    // gate count must be in the interval [1,511]
    if (_gates < 1 || _gates > 511) {
        std::cerr << "gates is " << _gates <<
            "; it must be greater than 0 and less than 512." << std::endl;
        valid = false;
    }
    // PRT must be a multiple of the pulse width
    if (_prt % _timer_widths[2]) {
        std::cerr << "PRT is " << _prt * 2 / adcFrequency() << " (" << _prt <<
            ") and pulse width is " << _timer_widths[2] * 2 / adcFrequency() << 
            " (" << _timer_widths[2] << 
            "): PRT must be an integral number of pulse widths." << std::endl;
        valid = false;
    }
    // PRT must be longer than (gates + 1) * pulse width
    if (_prt <= ((_gates + 1) * _timer_widths[2])) {
        std::cerr << 
            "PRT must be greater than (gates+1)*(pulse width)." << std::endl;
        valid = false;
    }
    // Make sure the Pentek's FPGA is using DDC10DECIMATE
    if (_ddcType != DDC10DECIMATE) {
        std::cerr << "The Pentek FPGA is using DDC type " << 
                ddcTypeName(_ddcType) << 
                ", but Ka requires DDC10DECIMATE." << std::endl;
        valid = false;
    }
    return valid;
}
