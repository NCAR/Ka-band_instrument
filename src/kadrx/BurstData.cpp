#include "BurstData.h"
#include <cstdio>
#include <cstring>

///////////////////////////////////////////////////////////////////////////

BurstData::BurstData()
{

  _pulseSeqNum = -9999;
  _timeSecs = 0;
  _nanoSecs = 0;
  _g0PowerDbm = -9999.0;
  _g0PhaseDeg = -9999.0;
  _g0IvalNorm = -9999.0;
  _g0QvalNorm = -9999.0;
  _g0FreqHz = -9999.0;
  _g0FreqCorrHz = -9999.0;
  _samples = 0;
  _samplesAlloc = 0;
  _iq = NULL;

}

/////////////////////////////////////////////////////////////////////////////
BurstData::~BurstData()

{

  if (_iq) {
    delete[] _iq;
  }

}

/////////////////////////////////////////////////////////////////////////////
// set the data

void BurstData::set(int64_t pulseSeqNum,
                    time_t timeSecs,
                    int nanoSecs,
                    double g0PowerDbm,
                    double g0PhaseDeg,
                    double g0IvalNorm,
                    double g0QvalNorm,
                    double g0FreqHz,
                    double g0FreqCorrHz,
                    int samples,
                    const int16_t *iq)

{
  
  _pulseSeqNum = pulseSeqNum;
  _timeSecs = timeSecs;
  _nanoSecs = nanoSecs;

  _g0PowerDbm = g0PowerDbm;
  _g0PhaseDeg = g0PhaseDeg;
  _g0IvalNorm = g0IvalNorm;
  _g0QvalNorm = g0QvalNorm;
  _g0FreqHz = g0FreqHz;
  _g0FreqCorrHz = g0FreqCorrHz;

  _samples = samples;
  _allocIq();
  memcpy(_iq, iq, _samples * 2 * sizeof(int16_t));

}

/////////////////////////////////////////////////////////////////////////////
// alloc the iq data

void BurstData::_allocIq()

{

  if (_samplesAlloc >= _samples) {
    return;
  }

  if (_iq) {
    delete[] _iq;
  }

  _iq = new int16_t[_samples * 2];

}

