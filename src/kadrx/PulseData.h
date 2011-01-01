#ifndef PULSE_DATA_H_
#define PULSE_DATA_H_

/// PulseData objects hold the IQ data for one pulse and one channel.
/// 
/// These are used to transfer data from a KaDrxPub object to the
/// KaMerge object.

#include <sys/time.h>
#include <sys/types.h>

class PulseData {

public:
    
  /**
   * Constructor.
   */

  PulseData();
  
  /// Destructor

  virtual ~PulseData();

  // set the data
  
  void set(int64_t pulseSeqNum,
           time_t timeSecs,
           int nanoSecs,
           int channel,
           int gates,
           const int16_t *iq);

  // get methods

  int64_t getPulseSeqNum() const { return _pulseSeqNum; }
  time_t getTimeSecs() const { return _timeSecs; }
  int getNanoSecs() const { return _nanoSecs; }
  int getChannel() const { return _channel; }
  int getGates() const { return _gates; }
  const int16_t *getIq() const { return _iq; }
    
private:

  /**
   * pulse sequence number since start of ops
   */
  
  int64_t _pulseSeqNum;
  
  /**
   * time of pulse - seconds and nano-secs
   */

  time_t _timeSecs;
  int _nanoSecs;
  
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
   * IQ data
   */

  int16_t *_iq;

  // functions

  void _allocIq();
  
};

#endif /*KADRXPUB_H_*/
