#include "PulseData.h"
#include <cstdio>
#include <cstring>

///////////////////////////////////////////////////////////////////////////

PulseData::PulseData()
{

  _pulseSeqNum = -9999;
  _timeSecs = 0;
  _nanoSecs = 0;
  _channel = -9999;
  _gates = 0;
  _gatesAlloc = 0;
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
                    int channel,
                    int gates,
                    const int16_t *iq)

{
  
  _pulseSeqNum = pulseSeqNum;
  _timeSecs = timeSecs;
  _nanoSecs = nanoSecs;

  _channel = channel;
  _gates = gates;
  _allocIq();
  memcpy(_iq, iq, _gates * 2 * sizeof(int16_t));

}

/////////////////////////////////////////////////////////////////////////////
// alloc or realloc space for the iq data

void PulseData::_allocIq()

{

  if (_gatesAlloc >= _gates) {
    return;
  }

  if (_iq) {
    delete[] _iq;
  }

  _iq = new int16_t[_gates * 2];

}

