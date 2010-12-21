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
#include <QMutexLocker>
#include <QWaitCondition>

#include <cmath>
#include <iostream>
#include <logx/Logging.h>

LOGGING("KaAfc")

// Pointer to our singleton instance
KaAfc * KaAfc::_theAfc = 0;

/// KaAfcPrivate is the private implementation class for KaAfc, subclassed from
/// QThread. The object gets new xmit samples via newXmitSample() calls (from 
/// another thread), adds them to sums, and when sufficient samples have
/// been summed releases the averages which will be processed by the local 
/// thread. The local thread takes the averages and adjusts the three 
/// programmable oscillators it controls.
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
    /// @param pulsenum pulse number, counted since transmitter startup
    void newXmitSample(double g0Mag, double freqOffset, unsigned int pulsenum);
    
    /// Set the G0 threshold power for reliable calculated frequencies. 
    /// Value is in dB relative to maximum receiver output.
    /// @param thresh G0 threshold power, in dB relative to maximum receiver 
    ///     output
    void setG0ThresholdDb(double thresh);
    
    /// Set the AFC fine step in Hz. 
    /// @param step AFC fine step in Hz
    void setFineStep(unsigned int step);

    /// Set the AFC coarse step in Hz. 
    /// @param step AFC coarse step in Hz
    void setCoarseStep(unsigned int step);

private:
    /// Actually process averaged xmit info to perform the AFC for three 
    /// oscillators. Note that the caller must hold a lock on _mutex when this
    /// method is called!
    /// @param timetag  long long time of the pulse average, in microseconds 
    ///     since 1970-01-01 00:00:00 UTC
    /// @param g0MagDb  relative g0 power magnitude
    /// @param freqOffset   measured frequency offset, in Hz
    void _processXmitAverage();
    
    /// Clear our sums
    void _clearSum();
    
    /// Set frequencies for all three oscillators. Frequencies are in units
    /// of the oscillators' frequency steps.
    /// @param osc1ScaledFreq frequency for oscillator 1 in units of its 
    ///     frequency step
    /// @param osc2ScaledFreq frequency for oscillator 2 in units of its 
    ///     frequency step
    /// @param osc3ScaledFreq frequency for oscillator 3 in units of its 
    ///     frequency step
    void _setOscillators(unsigned int osc1ScaledFreq, 
        unsigned int osc2ScaledFreq, unsigned int osc3ScaledFreq);
    
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

    /// Oscillator 0: 1.4-1.5 GHz (1.4400 GHz nominal), 100 kHz step
    TtyOscillator _osc0;
    
    /// Oscillator 1: 132-133 MHz (132.5 MHz nominal), 10 kHz step. 
    /// Adjustments to this oscillator track those of oscillator 3. E.g., if 
    /// oscillator 3's frequency is reduced 300 kHz, then oscillator 1's 
    /// frequency is also reduced 300 kHz.
    TtyOscillator _osc1;
    
    /// oscillator 3: 107-108 MHz (107.50 MHz nominal), 50 kHz step
    KaOscillator3 _osc3;
    
    /// G0 threshold power for reliable calculated frequencies, in dB relative
    /// to maximum receiver output
    double _g0ThreshDb;
    
    /// Coarse AFC adjustment step, Hz
    unsigned int _coarseStep;
    
    /// Fine AFC adjustment step, Hz
    unsigned int _fineStep;
    
    /// Number of g0 powers to sum before passing average for application
    unsigned int _nToSum;
    
    /// Number of g0 powers currently summed
    unsigned int _nSummed;
    
    /// g0 relative power sum
    double _g0MagSum;
    
    /// g0 relative power averaged over _nToSum samples
    double _g0MagAvg;
    
    /// offset frequency
    double _freqOffset;
    
    /// last pulse received by newXmitSample()
    unsigned int _lastRcvdPulse;
    
    /// number of pulses received by newXmitSample()
    unsigned int _pulsesRcvd;
    
    /// number of pulses dropped by newXmitSample()
    unsigned int _pulsesDropped;
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
KaAfc::newXmitSample(double g0Mag, double freqOffset, unsigned int pulsenum) {
    _afcPrivate->newXmitSample(g0Mag, freqOffset, pulsenum);
}

void
KaAfc::setG0ThresholdDb(double thresh)  { 
    theAfc()._afcPrivate->setG0ThresholdDb(thresh);
}

void
KaAfc::setCoarseStep(unsigned int step)  { 
    theAfc()._afcPrivate->setCoarseStep(step);
}

void
KaAfc::setFineStep(unsigned int step)  { 
    theAfc()._afcPrivate->setFineStep(step);
}

KaAfcPrivate::KaAfcPrivate() :
    QThread(),
    _mutex(QMutex::NonRecursive),   // must be non-recursive for QWaitCondition!
    _afcMode(AFC_SEARCHING),
    _osc0("/dev/ttyS0", 0, 100000, 14400, 15000),
    _osc1(TtyOscillator::SIM_OSCILLATOR, 1, 10000, 12750, 13750), 
    _osc3(KaPmc730::thePmc730()),
    _g0ThreshDb(-25.0),     // -25.0 dB G0 threshold relative power
    _coarseStep(500000),    // 500 kHz coarse step (SEARCHING)
    _fineStep(100000),      // 100 kHz fine step (TRACKING)
    _nToSum(10),
    _nSummed(0),
    _g0MagSum(0.0),
    _g0MagAvg(0.0),
    _freqOffset(0.0),
    _lastRcvdPulse(0),
    _pulsesRcvd(0),
    _pulsesDropped(0) {
    // Set the initial oscillator frequencies
    unsigned int osc0ScaledFreq = (_osc0.getScaledMinFreq());   // 1.4400 GHz
    unsigned int osc1ScaledFreq = (132500000 / _osc1.getFreqStep());    // 132.50 MHz
    unsigned int osc3ScaledFreq = (107500000 / _osc3.getFreqStep());    // 107.50 MHz
    _setOscillators(osc0ScaledFreq, osc1ScaledFreq, osc3ScaledFreq);
}

void
KaAfcPrivate::_setOscillators(unsigned int osc0ScaledFreq, 
    unsigned int osc1ScaledFreq, unsigned int osc3ScaledFreq) {
    bool osc0_OK = false;
    bool osc1_OK = false;
    bool osc3_OK = false;
    // Set starting frequencies for all three oscillators. We try as many times
    // as necessary...
    for (int attempt = 0; !(osc0_OK && osc1_OK && osc3_OK); attempt++) {
        // Initiate the frequency settings for the three osillators in parallel,
        // since it's a slow asynchronous process...
        if (! osc0_OK) {
            if (attempt > 0)
                WLOG << "...try again to set oscillator 0 frequency";
            _osc0.setScaledFreqAsync(osc0ScaledFreq);
        }
            
        if (! osc1_OK) {
            if (attempt > 0)
                WLOG << "...try again to set oscillator 1 frequency";
            _osc1.setScaledFreqAsync(osc1ScaledFreq);
        }
            
        if (! osc3_OK) {
            _osc3.setScaledFreq(osc3ScaledFreq);
            osc3_OK = true; // KaOscillator3::setScaledFreq() always works!
        }
        
        // Now complete the asynchronous process for the two serial oscillators
        osc0_OK = _osc0.freqAttained();
        osc1_OK = _osc1.freqAttained();
    }
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
KaAfcPrivate::setG0ThresholdDb(double thresh) {
    QMutexLocker locker(&_mutex);
    ILOG << "Setting AFC G0 threshold at " << thresh << " dB";
    _g0ThreshDb = thresh;
}

void
KaAfcPrivate::setCoarseStep(unsigned int step) {
    QMutexLocker locker(&_mutex);
    // Make sure the requested step is a multiple of the frequency steps of
    // all oscillators we're controlling
    if ((step % _osc0.getFreqStep()) ||
        (step % _osc1.getFreqStep()) ||
        (step % _osc3.getFreqStep())) {
        ELOG << "Ignoring requested AFC coarse step of " << step << " Hz";
        ELOG << "It must be a multiple of " << _osc0.getFreqStep() << ", " <<
            _osc1.getFreqStep() << ", and " << _osc3.getFreqStep() <<
            " Hz!";
        abort();
    }
    ILOG << "Setting AFC coarse step to " << step << " Hz";
    _coarseStep = step;
}

void
KaAfcPrivate::setFineStep(unsigned int step) {
    QMutexLocker locker(&_mutex);
    // Make sure the requested step is a multiple of the frequency steps of
    // all oscillators we're controlling
    if ((step % _osc0.getFreqStep()) ||
        (step % _osc1.getFreqStep()) ||
        (step % _osc3.getFreqStep())) {
        ELOG << "Ignoring requested AFC coarse step of " << step << " Hz";
        ELOG << "It must be a multiple of " << _osc0.getFreqStep() << ", " <<
            _osc1.getFreqStep() << ", and " << _osc3.getFreqStep() <<
            " Hz!";
        abort();
    }
    ILOG << "Setting AFC fine step to " << step << " Hz";
    _fineStep = step;
}

void
KaAfcPrivate::newXmitSample(double g0Mag, double freqOffset, 
    unsigned int pulsenum) {
    if (!(_pulsesRcvd % 5000))
        ILOG << _pulsesRcvd << " pulses received, " << _pulsesDropped << " dropped";
    _pulsesRcvd++;
    // If a frequency adjustment is in progress, just drop this sample
    if (! _mutex.tryLock()) {
        _pulsesDropped++;
        return;
    }
    
    assert(_nSummed < _nToSum);
    
    // Look for pulse gaps
    int pulseGap = pulsenum - _lastRcvdPulse - 1;
    if (pulseGap) {
        WLOG << __PRETTY_FUNCTION__ << ": " << pulseGap << 
            " pulse gap (_nSummed = " << _nSummed << ")";
    }
    _lastRcvdPulse = pulsenum;
    
    // Add to our sums
    _g0MagSum += g0Mag;
    _nSummed++;
    
    // If we've summed the required number of pulses, calculate the averages
    // and wake our thread waiting for the new averages
    if (_nSummed == _nToSum) {
        _g0MagAvg = _g0MagSum / _nToSum;
        _freqOffset = freqOffset;
        _clearSum();
        _newAverage.wakeAll();
    }

    _mutex.unlock();
}

void
KaAfcPrivate::_clearSum() {
    _g0MagSum = 0.0;
    _nSummed = 0;
}

void
KaAfcPrivate::_processXmitAverage() {
    double g0MagDb = 10.0 * log10(_g0MagAvg);

    ILOG << "New " << _nToSum << "-pulse average: G0 " << g0MagDb << 
        " dB, freq offset " << _freqOffset;

    // Set mode based on whether g0MagDb is less than our threshold
    AfcMode_t newMode = (g0MagDb < _g0ThreshDb) ? AFC_SEARCHING : AFC_TRACKING;
    
    switch (newMode) {
      // In AFC_SEARCHING mode, step upward in coarse steps through frequencies 
      // for oscillator 0 until we find sufficient G0 power.
      case AFC_SEARCHING:
      {
          // If mode just changed to searching, log it and apply the change
          if (newMode != _afcMode) {
              WLOG << "G0 relative power " << g0MagDb <<
                      " dB has dropped below min of " << 
                      _g0ThreshDb << " db. Returning to SEARCH mode.";
              // Only sum 10 pulses at a time when in searching mode
              _afcMode = AFC_SEARCHING;
              _nToSum = 10;
              // Start searching at minimum oscillator 0 frequency
              DLOG << "SEARCH starting oscillator 0 frequency at (" << 
                   _osc0.getScaledMinFreq() << " x " << _osc0.getFreqStep() << 
                   ") Hz";
              _osc0.setScaledFreq(_osc0.getScaledMinFreq());
              break;
          }
          // If we can, increase oscillator 0 frequency by our coarse step, 
          // otherwise go back to minimum oscillator 0 frequency.
          unsigned int newScaledFreq0 = _osc0.getScaledFreq();
          if (newScaledFreq0 != _osc0.getScaledMaxFreq()) {
              newScaledFreq0 += _coarseStep / _osc0.getFreqStep();
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
              _clearSum();
              return;
          }
          // If the frequency offset is less than our threshold, return now
          if (fabs(_freqOffset) < 6.0e4) {
              ILOG << "Frequency offset < 60 kHz, nothing to do!";
              return;
          }
          
          // How many AFC fine steps to correct? How many steps is that for
          // each oscillator?
          int afcFineSteps = int(round(_freqOffset / _fineStep));
          int osc0Steps = 0;
          int osc1Steps = afcFineSteps * (_fineStep / _osc1.getFreqStep());
          int osc3Steps = afcFineSteps * (_fineStep / _osc3.getFreqStep());
          
          // If possible without exceeding oscillator 3's frequency range,
          // change just oscillator 3 (and 1 which tracks it)... Otherwise, we
          // have to change all three oscillators.
          unsigned int osc3NewScaledFreq = _osc3.getScaledFreq() + osc3Steps;
          if ((osc3NewScaledFreq < _osc3.getScaledMinFreq()) ||
              (osc3NewScaledFreq > _osc3.getScaledMaxFreq())) {
              // We have to adjust oscillator 0. So, let's set oscillators 3
              // and 1 back to their center frequencies and take up the 
              // remaining adjustment in oscillator 0.
              unsigned int osc3ScaledCenter = 
                  (_osc3.getScaledMinFreq() + _osc3.getScaledMaxFreq()) / 2;
              osc3Steps = osc3ScaledCenter - _osc3.getScaledFreq();
              osc1Steps = osc3Steps * int(_osc3.getFreqStep() / _osc1.getFreqStep());
              
              double freqRemainder = _freqOffset - (osc3Steps * int(_osc3.getFreqStep()));
              afcFineSteps = int(round(freqRemainder / _fineStep));
              osc0Steps = afcFineSteps * int(_fineStep / _osc0.getFreqStep());
          }
          
          int freqChange0 = osc0Steps * int(_osc0.getFreqStep());
          int freqChange3 = osc3Steps * int(_osc3.getFreqStep());
          ILOG << "TRACKING: Changing 0 by " << freqChange0 << 
              " Hz, 1 and 3 by " << freqChange3 << " Hz";
          
          _setOscillators(_osc0.getScaledFreq() + osc0Steps,
              _osc1.getScaledFreq() + osc1Steps, 
              _osc3.getScaledFreq() + osc3Steps);
          break;
      }
    }
}
