#include "PulseData.h"
#include <cstdio>
#include <cstring>

///////////////////////////////////////////////////////////////////////////

PulseData::PulseData()
{

  _pulseSeqNum = -9999;
  _timeSecs = 0;
  _nanoSecs = 0;
  _channelId = -9999;
  _nGates = 0;
  _nGatesAlloc = 0;
  _iq = NULL;

}

/////////////////////////////////////////////////////////////////////////////
PulseData::~PulseData()

{

  if (_iq) {
    delete[] _iq;
  }

}

/////////////////////////////////////////////////////////////////////////////
// set the data

void PulseData::set(int64_t pulseSeqNum,
                    time_t timeSecs,
                    int nanoSecs,
                    int channelId,
                    int nGates,
                    const int16_t *iq)

{
  
  _pulseSeqNum = pulseSeqNum;
  _timeSecs = timeSecs;
  _nanoSecs = nanoSecs;

  _channelId = channelId;
  _nGates = nGates;
  _allocIq();
  memcpy(_iq, iq, _nGates * 2 * sizeof(int16_t));

}

/////////////////////////////////////////////////////////////////////////////
// combine every second gate in the IQ data
// to reduce the number of gates to half

void PulseData::combineEverySecondGate()

{

  int nGatesHalf = _nGates / 2;

  int16_t *iq = _iq;
  int16_t *iqHalf = _iq;

  for (int ii = 0; ii < nGatesHalf; ii++) {

    memcpy(iqHalf, iq, 2 * sizeof(int16_t));
    iq += 4;
    iqHalf += 2;

  }

  _nGates = nGatesHalf;

}

/////////////////////////////////////////////////////////////////////////////
// alloc or realloc space for the iq data

void PulseData::_allocIq()

{

  if (_nGatesAlloc >= _nGates) {
    return;
  }

  if (_iq) {
    delete[] _iq;
  }

  _iq = new int16_t[_nGates * 2];

}

