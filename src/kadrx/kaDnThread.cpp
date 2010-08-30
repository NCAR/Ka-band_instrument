#include "kaDnThread.h"
#include <sys/timeb.h>
#include <cstdlib>
#include <iostream>
#include <iomanip>

using namespace boost::posix_time;

//////////////////////////////////////////////////////////////////////////////////
kaDnThread::kaDnThread(
                const KaDrxConfig& config,
                TSWriter* tsWriter,
                bool publish,
                int tsLength,
                std::string devName,
                int chanId,
                std::string gaussianFile,
                std::string kaiserFile,
                bool simulate,
                int simPauseMS,
                int simWavelength) :
    kaDn(devName,
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
             false,
             gaussianFile,
             kaiserFile,
             simulate,
             simPauseMS,
             simWavelength,
             false),
     _doCI(false),
     _publish(publish),
     _tsWriter(tsWriter),
     _tsDiscards(0),
     _lastPulse(0),
     _droppedPulses(0),
     _syncErrors(0),
     _gotFirstPulse(false),
     _ddsSamplePulses(tsLength),    // for now, set to the # of pulses we get from the 7142
     _ddsSampleInProgress(0),
     _ndxInDdsSample(0)
{
    // Bail out if we're not configured legally.
    if (! _configIsValid())
        abort();

    // Fill our DDS base housekeeping values from the configuration
    config.fillDdsSysHousekeeping(_baseDdsHskp);
}

//////////////////////////////////////////////////////////////////////////////////
kaDnThread::~kaDnThread() {

}

//////////////////////////////////////////////////////////////////////////////////
void kaDnThread::run() {

  static unsigned short int ttl_toggle = 0;

  // Per-pulse data size
  if (!_doCI) {
	  // non-integrated data simply has IQ pairs, as 2 byte shorts,
	  // for all gates. In free run mode, there is no tagging.
	  // In normal mode, there is pulse tagging.
      _pentekPulseLen = _gates * 2 * 2;
      // If not in free run mode, there will also be a 4-byte sync word and a 
      // 4-byte pulse tag at the beginning of each pulse.
      if (! _freeRun)
          _pentekPulseLen += 8;
  } else {
      // coherently integrated data has:
      // 4 4-byte tags followed by even IQ pairs followed by odd IQ pairs,
      // for all gates. Is and Qs are 4 byte integers.
      _pentekPulseLen = 16 + _gates * 2 * 2 * 4;
  }
  
  // We read _tsLength pulses at a time
  int bufferSize = _tsLength * _pentekPulseLen;

  // create the read buffer
  char* buf = new char[bufferSize];

  std::cout << "Channel " << _chanId << " block size is " << bufferSize << 
    ", waiting for data..." << std::endl;

  // How many unprocessed data bytes are at the head of buf?
  int nInBuf = 0;
  
  // start the loop. The thread will block on the read()
  while (1) {

    // Fill up our buffer
    int nToRead = bufferSize - nInBuf;
    int nRead = read(buf + nInBuf, nToRead);

    ttl_toggle = ~ttl_toggle;
    TTLOut(ttl_toggle);

    if (nRead != nToRead) {
        std::cerr << "read returned " << nRead << " instead of " << 
                nToRead << " ";
        if (nRead < 0) {
            perror("");
            nRead = 0;
        }
        std::cerr << std::endl;
    }
    nInBuf += nRead;

    // Break into component pulses, stuffing them into DDS samples which are 
    // published as they are filled.
    int nUnprocessed;
    if (_doCI) {
        nUnprocessed = _decodeAndPublishCI(buf, nInBuf);
    } else {
        nUnprocessed = _decodeAndPublishRaw(buf, nInBuf);
    }
    
    // Move the unprocessed bytes to the beginning of buf, to be handled
    // on the next go 'round.
    if (nUnprocessed)
        memmove(buf, buf + bufferSize - nUnprocessed, nUnprocessed);

    nInBuf = nUnprocessed;
  }
}

///////////////////////////////////////////////////////////
double kaDnThread::_nowTime() {
  struct timeb timeB;
  ftime(&timeB);
  return timeB.time + timeB.millitm/1000.0;
}

///////////////////////////////////////////////////////////

// 1970-01-01 00:00:00 UTC
static const ptime Epoch1970(boost::gregorian::date(1970, 1, 1), time_duration(0, 0, 0));

///////////////////////////////////////////////////////////
int
kaDnThread::_decodeAndPublishCI(char* buf, int n) {
    return 0;
//
//    // decode data from coherent integrator. It is packed as follows:
//    // <TAG_I_EVEN><TAG_Q_EVEN><TAG_I_ODD><TAG_Q_ODD><IQpairs,even pulse><IQpairs,odd pulse>
//    //
//    // The tags are formatted as:
//    //   bits 31:28  Format number  0-15 (4 bits)
//    //   bits 27:26  Channel number  0-3 (2 bits)
//    //   bits    25  0=even, 1=odd   0-1 (1 bit)
//    //   bit     24  0=I, 1=Q        0-1 (1 bit)
//    //   bits 23:00  Sequence number     (24 bits)
//    int in = 0;
//    int out = 0;
//    int32_t* data = (int32_t*)buf;
//    for (int t = 0; t < _tsLength; t++) {
//        // decode the leading four tags
//        for (int tag = 0; tag < 4; tag++) {
//            //int format = (data[in] >> 28) & 0xf;
//            //int seq = data[in] & 0xffffff;
//            //std::cout << std::dec << format << " 0x" << std::hex <<  key << " " << std::dec << seq << "   ";
//            int key = (data[in] >> 24) & 0xf;
//            if (key != ((_chanId << 2) + tag)) {
//                _syncErrors++;
//            }
//            in++;
//        }
//        //std::cout << std::dec << std::endl;
//        for (int g = 0; g < _gates; g++) {
//            for (int iq = 0; iq < 2; iq++) {
//                // logic below shows where even and odd Is and Qs are located:
//                //     double even = data[in];
//                //     double odd = data[in+2*_gates];
////                 std::cout << g << "  " << data[in] << "  " << data[in+2*_gates] << std::endl;
//                in++;
//                out++;
//            }
//        }
//        // we have advanced the in pointer to the end of the even Is and Qs.
//        // Now space past the odd Is and Qs.
//        in += 2*_gates;
//    }
}



///////////////////////////////////////////////////////////
int
kaDnThread::_decodeAndPublishRaw(char* buf, int buflen) {
    char* bufPos = buf;
    char* endOfBuf = buf + buflen;
    int pulsesUnpacked = 0;
    
    // Move through the buffer, unpacking pulses
    while (bufPos < endOfBuf) {
        // If we don't have enough data for a full pulse, return now
        if (endOfBuf - bufPos < _pentekPulseLen) {
            return(endOfBuf - bufPos);
        }
        
        unsigned int pulseNum = 0;
        
        // If we're not free-running, find the next sync word, then unpack
        // channel number and pulse sequence number.
        if (!_freeRun) {
            // Find the next sync word. Normally the index will be zero.
            int syncPos = indexOfNextSync(bufPos, endOfBuf - bufPos);
            if (syncPos != 0) {
                _syncErrors++;
                std::cerr << "Channel " << _chanId << ": skipped " << syncPos << 
                    " bytes to find sync after pulse " << _lastPulse << std::endl;
                if (syncPos < 0) {
                    // A sync word was not found. Save the last 3 bytes (in case
                    // the buffer ends in a partial sync word) and return.
                    return(sizeof(SD3C_SYNCWORD) - 1);
                }
            }
            // Move to the beginning of the next sync word
            bufPos += syncPos;
            
            // If we don't have enough data left for a full pulse, return
            // now with the remaining data unprocessed. Otherwise, skip past 
            // the sync word and go on.
            if ((endOfBuf - bufPos) < _pentekPulseLen)
                return(endOfBuf - bufPos);
            else
                bufPos += sizeof(SD3C_SYNCWORD);  // move past the sync word
            
            // Unpack the 4-byte channel id/pulse number
            unsigned int chan;
            _unpackChannelAndPulse(bufPos, chan, pulseNum);
            bufPos += 4;    // move past the channel/pulse num word

            if (int(chan) != _chanId) {
                std::cerr << "kaDnThread for channel " << _chanId <<
                        " got data for channel " << chan << "!" << std::endl;
                abort();
            }
            
            // Initialize _lastPulse if this is the first pulse we've seen
            if (! _gotFirstPulse) {
                _lastPulse = pulseNum - 1;
                _gotFirstPulse = true;
            }
            
            // How many pulses since the last one we saw?
            int delta = pulseNum - _lastPulse;
            if (delta < (-MAX_PULSE_NUM / 2))
                delta += (MAX_PULSE_NUM + 1); // pulse number wrapped

            if (delta == 0) {
                std::cerr << "Channel " << _chanId << ": got repeat of pulse " <<
                        pulseNum << "!" << std::endl;
                abort();
            } else if (delta != 1) {
                std::cerr << _lastPulse << "->" << pulseNum << ": ";
                if (delta < 0) {
                    std::cerr << "Channel " << _chanId << " went BACKWARD " << 
                        -delta << " pulses" << std::endl;
                } else {
                    std::cerr << "Channel " << _chanId << " dropped " << 
                        delta - 1 << " pulses" << std::endl;
                }
            }
            _droppedPulses += (delta - 1);
            _lastPulse = pulseNum;
        }
        
        // bufPos is now pointing to the pulse data
        // data length in bytes: 2-byte I and 2-byte Q for each gate
        int datalen = 4 * _gates;
        
        // If we're publishing, copy this pulse into our DDS sample-in-progress,
        // and publish it when it's full.
        if (_publish) {
            // Make sure we have a DDS sample to fill
            if (! _ddsSampleInProgress) {
                // Get a free sample
                _ddsSampleInProgress = _tsWriter->getEmptyItem();
                // If no sample was available, discard everything we have and
                // return.
                if (! _ddsSampleInProgress) {
                    std::cerr << "Channel " << _chanId << 
                        " dropped data with no DDS samples available!" << std::endl;
                    _tsDiscards++;
                    return 0;
                }
                _ddsSampleInProgress->sampleNumber = _sampleNumber++;
                _ddsSampleInProgress->chanId = _chanId;
                _ddsSampleInProgress->tsList.length(_ddsSamplePulses);
                _ndxInDdsSample = 0;
            }

            //
            // Copy this pulse into our DDS sample in progress
            //
            RadarDDS::TimeSeries & ts = _ddsSampleInProgress->tsList[_ndxInDdsSample++];
            // Copy our fixed metadata into this pulse
            ts.hskp = _baseDdsHskp;
            // Then fill the non-fixed metadata
            ts.data.length(_gates * 2);   // I and Q for each gate, length is count of shorts
            ts.hskp.chanId = _chanId;
            ts.prt_seq_num = 1;   // single-PRT only for now
            ts.pulseNum = pulseNum;
            time_duration timeFromEpoch = timeOfPulse(pulseNum) - Epoch1970;
            // Calculate the timetag, which is usecs since 1970-01-01 00:00:00 UTC
            ts.hskp.timetag = timeFromEpoch.total_seconds() * 1000000LL +
                    (timeFromEpoch.fractional_seconds() * 1000000LL) /
                    time_duration::ticks_per_second();
            // Copy the per-gate data from the incoming buffer to the DDS 
            // sample data space.
            memmove(ts.data.get_buffer(), bufPos, datalen);

            // Publish _ddsSampleInProgress if it's full
            if (_ndxInDdsSample == _ddsSamplePulses) {
                // publish it
                _tsWriter->publishItem(_ddsSampleInProgress);
                _ddsSampleInProgress = 0;
            }
        }
        
        // Move past the data for this pulse
        bufPos += datalen;
        pulsesUnpacked++;
    }
    
    if (bufPos != endOfBuf)
        std::cerr << __FUNCTION__ << ": Oops, we should be at the end " <<
            "of data, but we're not!" << std::endl;
    return 0;
}

//////////////////////////////////////////////////////////////////////////////////
unsigned long
kaDnThread::tsDiscards() {
	unsigned long retval = _tsDiscards;
	_tsDiscards = 0;
	return retval;
}

//////////////////////////////////////////////////////////////////////////////////
unsigned long
kaDnThread::droppedPulses() {
	unsigned long retval = _droppedPulses;
	return retval;
}

//////////////////////////////////////////////////////////////////////////////////
unsigned long
kaDnThread::syncErrors() {
	unsigned long retval = _syncErrors;
	return retval;
}

//////////////////////////////////////////////////////////////////////////////////
void
kaDnThread::_unpackChannelAndPulse(const char* buf, unsigned int & chan, 
        unsigned int & pulseNum) {
    // Channel number is the upper two bits of the channel/pulse num word, which
    // is stored in little-endian byte order
    const unsigned char *ubuf = (const unsigned char*)buf;
    chan = (ubuf[3] >> 6) & 0x3;
    
    // Pulse number is the lower 30 bits of the channel/pulse num word, which is
    // stored in little-endian byte order
    pulseNum = (ubuf[3] & 0x3f) << 24 | ubuf[2] << 16 | ubuf[1] << 8 | ubuf[0];
}

//////////////////////////////////////////////////////////////////////////////////
bool
kaDnThread::_configIsValid() const {
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
    return valid;
}
