#ifndef PULSE_DATA_H_
#define PULSE_DATA_H_

/// PulseData objects hold the IQ data for one pulse and one channel.
/// 
/// These are used to transfer data from a KaDrxPub object to the
/// KaMerge object.

#include <sys/time.h>

class PulseData {

public:
    
  /**
   * Constructor.
   * @param config KaDrxConfig defining the desired configuration.
   * @param publish should we publish data via DDS?
   */

  PulseData();
  
  /// Destructor

  virtual ~PulseData();

  // set the data
  
  void set(long long pulseSeqNum,
           int channel,
           int gates,
           time_t timeSecs,
           int nanoSecs,
           const unsigned short *iq);
    
private:

  /**
   * pulse sequence number since start of ops
   */
  
  long long _pulseSeqNum;
  
  /**
   * channel number
   */

  int _channel;
  
  /**
   * number of gates
   */

  int _gates;
  int _gatesAlloc;

  /**
   * time of pulse - seconds and nano-secs
   */

  time_t _timeSecs;
  int _nanoSecs;
  
  /**
   * IQ data
   */

  unsigned short *_iq;

  // functions

  void _allocIq();
  
};

#endif /*KADRXPUB_H_*/
