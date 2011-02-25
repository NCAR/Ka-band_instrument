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
           int channelId,
           int nGates,
           const int16_t *iq);

  // combine every second gate in the IQ data
  // to reduce the number of gates to half

  void combineEverySecondGate();

  // get methods

  inline int64_t getPulseSeqNum() const { return _pulseSeqNum; }
  inline time_t getTimeSecs() const { return _timeSecs; }
  inline int getNanoSecs() const { return _nanoSecs; }
  inline int getChannelId() const { return _channelId; }
  inline int getNGates() const { return _nGates; }
  inline const int16_t *getIq() const { return _iq; }
  inline int16_t *getIq() { return _iq; }
  
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

  int _channelId;
  
  /**
   * number of gates
   */

  int _nGates;
  int _nGatesAlloc;

  /**
   * IQ data
   */

  int16_t *_iq;

  // functions

  void _allocIq();
  
};

#endif /*KADRXPUB_H_*/
