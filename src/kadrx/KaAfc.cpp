/*
 * KaAfc.cpp
 *
 *  Created on: Oct 29, 2010
 *      Author: burghart
 */

#include "KaAfc.h"
#include "KaPmc730.h"
#include "TtyOscillator.h"
#include "KaOscillator3.h"

#include <QThread>
#include <QMutex>
#include <QWaitCondition>

#include <cmath>
#include <iostream>
#include <logx/Logging.h>

LOGGING("KaAfc")

KaAfc * KaAfc::_theAfc = 0;

/// KaAfcPrivate is the private implementation class for KaAfc, subclassed from
/// QThread. The object gets new xmit samples via newXmitSample() calls (from 
/// another thread), adding them to sums, and when sufficient samples have
/// been summed releases the averages to be processed by the local thread.
/// The local thread takes the averages and adjusts the three programmable 
/// oscillators it controls.
class KaAfcPrivate : public QThread {
public:
    KaAfcPrivate();
    
    ~KaAfcPrivate();
    
    void run();
    
    /// Accept an incoming set of averaged transmit pulse information comprising
    /// relative g0 power, and calculated frequency offset. This information 
    /// will be used to adjust oscillator frequencies.
    /// If this AfcThread is currently in the process of a frequency adjustment,
    /// the given sample will be ignored. @see adjustmentInProgress()
    /// @param g0Mag relative g0 power magnitude, in range [0.0,1.0]
    /// @param freqOffset measured frequency offset, in Hz
    void newXmitSample(double g0Mag, double freqOffset);

private:
    /// Actually process averaged xmit info to perform the AFC for three 
    /// oscillators.
    /// @param timetag  long long time of the pulse average, in microseconds 
    ///     since 1970-01-01 00:00:00 UTC
    /// @param g0MagDb  relative g0 power magnitude
    /// @param freqOffset   measured frequency offset, in Hz
    void _processXmitAverage();
    
    /// Thread access mutex
    QMutex _mutex;
    
    /// Wait condition which wakes the thread when new average values are 
    /// available.
    QWaitCondition _newAverage;
    
    /// AFC state
    ///     AFC_SEARCHING (coarse adjustment) while looking for more than 
    ///         threshold power in G0
    ///     AFC_TRACKING (finer adjustment) while tracking to minimize frequency
    ///         offset
    typedef enum { AFC_SEARCHING, AFC_TRACKING } AfcMode_t;
    AfcMode_t _afcMode;

    /// Oscillator 0: 1.4-1.5 GHz (1.4400 GHz nominal), 100 KHz step
    TtyOscillator _osc0;
    
    /// Oscillator 1: 132-133 MHz (132.5 MHz nominal), 10 KHz step. 
    /// Adjustments to this oscillator track those of oscillator 3. E.g., if 
    /// oscillator 3's frequency is reduced 300 KHz, then oscillator 1's 
    /// frequency is also reduced 300 KHz.
    TtyOscillator _osc1;
    
    /// oscillator 2: 16.500 GHz nominal, 1 MHz step.
    TtyOscillator _osc2;
    
    /// oscillator 3: 107-108 MHz (107.50 MHz nominal), 50 KHz step
    KaOscillator3 _osc3;
    
    /// Number of xmit samples to sum before passing average for application
    unsigned int _nToSum;
    
    /// Number of xmit samples currently summed
    unsigned int _nSummed;
    
    /// g0 relative power sum
    double _g0MagSum;
    
    /// offset frequency sum
    double _freqOffsetSum;
    
    /// g0 relative power averaged over _nToSum samples
    double _g0MagAvg;
    
    /// offset frequency averaged over _nToSum samples
    double _freqOffsetAvg;
};

KaAfc::KaAfc() {
    // Instantiate our private thread and start it.
    _afcPrivate = new KaAfcPrivate();
    _afcPrivate->start();
}

KaAfc::~KaAfc() {
    _afcPrivate->quit();
    if (! _afcPrivate->wait(5000)) {
        ELOG << "KaAfcPrivate thread failed to stop in 5 seconds. Exiting anyway.";
    }
}

void
KaAfc::newXmitSample(double g0Mag, double freqOffset) {
    _afcPrivate->newXmitSample(g0Mag, freqOffset);
}


KaAfcPrivate::KaAfcPrivate() :
    QThread(),
    _mutex(QMutex::Recursive),
    _afcMode(AFC_SEARCHING),
    _osc0(TtyOscillator::SIM_OSCILLATOR, 0, 100000, 14000, 14400, 15000),
    _osc1(TtyOscillator::SIM_OSCILLATOR, 1, 10000, 13250, 12750, 13750), 
    _osc2(TtyOscillator::SIM_OSCILLATOR, 2, 1000000, 16500, 16000, 17000),
    _osc3(KaPmc730::thePmc730()),
    _nToSum(10),
    _nSummed(0),
    _g0MagSum(0.0),
    _freqOffsetSum(0.0),
    _g0MagAvg(0.0),
    _freqOffsetAvg(0.0) {
}

KaAfcPrivate::~KaAfcPrivate() {
}

void
KaAfcPrivate::run() {
    while (true) {
        _mutex.lock();
        _newAverage.wait(&_mutex);
        _processXmitAverage();
        _mutex.unlock();
    }
}

void
KaAfcPrivate::newXmitSample(double g0Mag, double freqOffset) {
    // If a frequency adjustment is in progress, just drop this sample
    if (! _mutex.tryLock())
        return;
    
    assert(_nSummed < _nToSum);
    
    // Add to our sums
    _g0MagSum += g0Mag;
    _freqOffsetSum += freqOffset;
    _nSummed++;
    
    // If we've summed the required number of pulses, calculate the averages
    // and wake thread(s) waiting for them
    if (_nSummed == _nToSum) {
        _g0MagAvg = _g0MagSum / _nToSum;
        _freqOffsetAvg = _freqOffsetSum / _nToSum;
        _nSummed = 0;
        _newAverage.wakeAll();
    }

    _mutex.unlock();
}

void
KaAfcPrivate::_processXmitAverage() {
    QMutexLocker locker(&_mutex);

    static const double G0MAG_DB_THRESHOLD = -25.0;
    double g0MagDb = 10.0 * log10(_g0MagAvg);

    // Set mode based on whether g0MagDb is less than our threshold
    AfcMode_t newMode = (g0MagDb < G0MAG_DB_THRESHOLD) ? AFC_SEARCHING : AFC_TRACKING;
    
    switch (newMode) {
      // In AFC_SEARCHING mode, step upward quickly through frequencies for 
      // oscillator 0 until we find sufficient G0 power.
      case AFC_SEARCHING:
      {
          // If mode just changed to searching, log it and apply the change
          if (newMode != _afcMode) {
              WLOG << "G0 relative power " << g0MagDb <<
                      " dB has dropped below minimum threshold of " << 
                      G0MAG_DB_THRESHOLD << ", so entering AFC_SEARCHING mode.";
              // Only sum 10 pulses at a time when in searching mode
              _afcMode = AFC_SEARCHING;
              _nToSum = 10;
          }
          // If we can, increase oscillator 0 frequency by 500 KHz, otherwise 
          // go back to minimum oscillator 0 frequency.
          unsigned int newScaledFreq0 = _osc0.getScaledFreq();
          if (newScaledFreq0 != _osc0.getScaledMaxFreq()) {
              newScaledFreq0 += 500000 / _osc0.getFreqStep();
              DLOG << "SEARCH stepping oscillator 0 frequency to (" << 
                      newScaledFreq0 << " x " << _osc0.getFreqStep() << ") Hz";
          } else {
              newScaledFreq0 = _osc0.getScaledMinFreq();
              WLOG << "SEARCH starting again at min frequency (" <<
                      newScaledFreq0 << " x " << _osc0.getFreqStep() << ") Hz";
          }
          _osc0.setScaledFreq(newScaledFreq0);
          break;
      }
      // In AFC_TRACKING mode, adjust frequency for oscillator 3 and/or 0
      // based on the measured frequency offset. Whatever offset is applied to
      // oscillator 3 is also applied to oscillator 1.
      case AFC_TRACKING:
      {
          // If mode just changed to tracking, log it and return to get the next
          // sample over more pulses
          if (newMode != _afcMode) {
              ILOG << "G0 relative power is high enough, entering tracking mode";
              // Sum 50 pulses at a time while in tracking mode
              _afcMode = AFC_TRACKING;
              _nToSum = 50;
              // Return now to go get a new average over more pulses
              return;
          }
          // Change just oscillator 3 if possible, otherwise change both 0
          // and 3.  Oscillator 1 tracks changes in oscillator 3 regardless.
          ILOG << "TRACKING...";
          break;
      }
    }

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
}
