#include "PulseData.h"
#include <cstdio>
#include <cstring>
#include <cmath>

#include <iostream>
using namespace std;

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
  const int DecimationFactor = 2;  // every 2nd gate (for now?)
  int newNGates = _nGates / DecimationFactor;

  int16_t *iq = _iq;
  int16_t *newIq = _iq;

  for (int newG = 0; newG < newNGates; newG++) {

    // Save I and Q of the first gate being averaged for this new gate
    double firstI = iq[0];
    double firstQ = iq[1];
    double firstPower = firstI * firstI + firstQ * firstQ;

    // Sum powers for the gates going into the new gate
    double powerSum = 0.0;
    for (int subG = 0; subG < DecimationFactor; subG++) {
        double i = iq[0];
        double q = iq[1];
        
        powerSum += i * i + q * q;
        
        iq += 2;    // step to the next original gate
    }

    // Average power for the two gates
    double newPower = powerSum / DecimationFactor;
    
    // Scale factor for firstI and firstQ so that they keep their original phase 
    // but will yield the average power of the combined gates.

    double iqScale = 1.0;
    if (firstPower > 0) {
      sqrt(newPower / firstPower);
    }
    
    // Scale firstI and firstQ to generate values for our new gate.

    
    int newI = firstI * iqScale;
    int newQ = firstQ * iqScale;

    if (newI < -32768) {
      newI = -32768;
    } else if (newI > 32767) {
      newI = 32767;
    }
    
    if (newQ < -32768) {
      newQ = -32768;
    } else if (newQ > 32767) {
      newQ = 32767;
    }
    
    newIq[0] = (int16_t) newI;
    newIq[1] = (int16_t) newQ;
    
    newIq += 2; // step to the next new gate

  }

  _nGates = newNGates;

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

