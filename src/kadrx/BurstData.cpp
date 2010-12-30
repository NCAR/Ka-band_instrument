#include "BurstData.h"
#include <cstdio>
#include <cstring>

///////////////////////////////////////////////////////////////////////////

BurstData::BurstData()
{

  _pulseSeqNum = -9999;
  _timeSecs = 0;
  _nanoSecs = 0;
  _g0MagnitudeV = -9999.0;
  _g0PhaseDeg = -9999.0;
  _freqHz = -9999.0;
  _freqCorrHz = -9999.0;
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

void BurstData::set(long long pulseSeqNum,
                    time_t timeSecs,
                    int nanoSecs,
                    double g0MagnitudeV,
                    double g0PhaseDeg,
                    double freqHz,
                    double freqCorrHz,
                    int samples,
                    const unsigned short *iq)

{
  
  _pulseSeqNum = pulseSeqNum;
  _timeSecs = timeSecs;
  _nanoSecs = nanoSecs;

  _g0MagnitudeV = g0MagnitudeV;
  _g0PhaseDeg = g0PhaseDeg;
  _freqHz = freqHz;
  _freqCorrHz = freqCorrHz;

  _samples = samples;
  _allocIq();
  memcpy(_iq, iq, _samples * 2 * sizeof(unsigned short));

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

  _iq = new unsigned short[_samples * 2];

}

