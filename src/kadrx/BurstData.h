#ifndef BURST_DATA_H_
#define BURST_DATA_H_

/// BurstData objects hold the IQ samples data for one burst.
/// 
/// These are used to transfer data from a KaDrxPub object to the
/// KaMerge object.

#include <sys/time.h>

class BurstData {

public:
    
  /**
   * Constructor.
   */

  BurstData();
  
  /// Destructor

  virtual ~BurstData();
  
  // set the data
  
  void set(long long pulseSeqNum,
           time_t timeSecs,
           int nanoSecs,
           double g0PowerDbm,
           double g0PhaseDeg,
           double g0FreqHz,
           double g0FreqCorrHz,
           int samples,
           const unsigned short *iq);
    
  // get methods

  long long getPulseSeqNum() const { return _pulseSeqNum; }
  time_t getTimeSecs() const { return _timeSecs; }
  int getNanoSecs() const { return _nanoSecs; }
  double getG0PowerDbm() const { return _g0PowerDbm; }
  double getG0PhaseDeg() const { return _g0PhaseDeg; }
  double getG0FreqHz() const { return _g0FreqHz; }
  double getG0FreqCorrHz() const { return _g0FreqCorrHz; }
  int getSamples() const { return _samples; }
  const unsigned short *getIq() const { return _iq; }
    
private:

  /**
   * pulse sequence number since start of ops
   */
  
  long long _pulseSeqNum;
  
  /**
   * time of pulse - seconds and nano-secs
   */

  time_t _timeSecs;
  int _nanoSecs;
  
  /**
   * burst power and phase
   */

  double _g0PowerDbm;
  double _g0PhaseDeg;

  /**
   * frequency and frequency correction
   */

  double _g0FreqHz;
  double _g0FreqCorrHz;

  /**
   * number of samples
   */

  int _samples;
  int _samplesAlloc;

  /**
   * IQ data
   */

  unsigned short *_iq;

  // functions

  void _allocIq();
  
};

#endif

