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
           double g0MagnitudeV,
           double g0PhaseDeg,
           double freqHz,
           double freqCorrHz,
           int samples,
           const unsigned short *iq);
    
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
   * burst magnitude and phase
   */

  double _g0MagnitudeV;
  double _g0PhaseDeg;

  /**
   * frequency and frequency correction
   */

  double _freqHz;
  double _freqCorrHz;

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

