#ifndef BURST_DATA_H_
#define BURST_DATA_H_

/// BurstData objects hold the IQ samples data for one burst.
/// 
/// These are used to transfer data from a KaDrxPub object to the
/// KaMerge object.

#include <sys/time.h>
#include <sys/types.h>

class BurstData {

public:
    
  /**
   * Constructor.
   */

  BurstData();
  
  /// Destructor

  virtual ~BurstData();
  
  // set the data
  
  void set(int64_t pulseSeqNum,
           time_t timeSecs,
           int nanoSecs,
           double g0Magnitude,
           double g0PowerDbm,
           double g0PhaseDeg,
           double g0IvalNorm,
           double g0QvalNorm,
           double g0FreqHz,
           double g0FreqCorrHz,
           int nSamples,
           const int16_t *iq);
    
  // get methods

  inline int64_t getPulseSeqNum() const { return _pulseSeqNum; }
  inline time_t getTimeSecs() const { return _timeSecs; }
  inline int getNanoSecs() const { return _nanoSecs; }
  inline double getG0Magnitude() const { return _g0Magnitude; }
  inline double getG0PowerDbm() const { return _g0PowerDbm; }
  inline double getG0PhaseDeg() const { return _g0PhaseDeg; }
  inline double getG0IvalNorm() const { return _g0IvalNorm; }
  inline double getG0QvalNorm() const { return _g0QvalNorm; }
  inline double getG0FreqHz() const { return _g0FreqHz; }
  inline double getG0FreqCorrHz() const { return _g0FreqCorrHz; }
  inline int getNSamples() const { return _nSamples; }
  inline const int16_t *getIq() const { return _iq; }
    
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
   * burst power and phase
   */

  double _g0Magnitude; // linear
  double _g0PowerDbm;
  double _g0PhaseDeg;
  double _g0IvalNorm;
  double _g0QvalNorm;

  /**
   * frequency and frequency correction
   */

  double _g0FreqHz;
  double _g0FreqCorrHz;

  /**
   * number of samples
   */

  int _nSamples;
  int _nSamplesAlloc;

  /**
   * IQ data
   */

  int16_t *_iq;

  // functions

  void _allocIq();
  
};

#endif

