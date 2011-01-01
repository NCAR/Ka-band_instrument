#include "KaMerge.h"
#include <logx/Logging.h>
#include <sys/timeb.h>
#include <cmath>
#include <cstdlib>
#include <unistd.h>
#include <iostream>
#include <iomanip>
#include <boost/date_time/posix_time/posix_time.hpp>

using namespace boost::posix_time;

LOGGING("KaMerge")

///////////////////////////////////////////////////////////////////////////

KaMerge::KaMerge(const KaDrxConfig& config) :
        QThread(),
        _config(config)
{

  _queueSize = _config.merge_queue_size();
  _iwrfServerTcpPort = _config.iwrf_server_tcp_port();

  _qH = new CircBuffer<PulseData>(_queueSize);
  _qV = new CircBuffer<PulseData>(_queueSize);
  _qB = new CircBuffer<BurstData>(_queueSize);

  _pulseH = new PulseData;
  _pulseV = new PulseData;
  _burst = new BurstData;

}

/////////////////////////////////////////////////////////////////////////////
KaMerge::~KaMerge()

{

  delete _qH;
  delete _qV;
  delete _qB;

  delete _pulseH;
  delete _pulseV;
  delete _burst;

}

/////////////////////////////////////////////////////////////////////////////
//
// Thread run method

void KaMerge::run()

{

  // Since we have no event loop, allow thread termination via the terminate()
  // method.
  setTerminationEnabled(true);
  
  // start the loop
  while (true) {
    _readNextPulse();
    usleep(100);
  }

}

/////////////////////////////////////////////////////////////////////////////
// read the next set of pulse data

void KaMerge::_readNextPulse()
{

  // read next pulse data for each channel

  _readNextH();
  _readNextV();
  _readNextB();

  // compute the max pulse seq num

  int64_t seqNumH = _pulseH->getPulseSeqNum();
  int64_t seqNumV = _pulseV->getPulseSeqNum();
  int64_t seqNumB = _burst->getPulseSeqNum();
  int64_t maxSeqNum = seqNumH;
  if (maxSeqNum < seqNumV) {
    maxSeqNum = seqNumV;
  }
  if (maxSeqNum < seqNumB) {
    maxSeqNum = seqNumB;
  }

  // read until all sequence numbers match

  bool done = false;
  while (!done) {

    done = true;

    // H channel

    if (seqNumH < maxSeqNum) {
      _readNextH();
      seqNumH = _pulseH->getPulseSeqNum();
      if (maxSeqNum < seqNumH) {
        maxSeqNum = seqNumH;
        done = false;
      }
    }

    // V channel

    if (seqNumV < maxSeqNum) {
      _readNextV();
      seqNumV = _pulseV->getPulseSeqNum();
      if (maxSeqNum < seqNumV) {
        maxSeqNum = seqNumV;
        done = false;
      }
    }

    // burst channel

    if (seqNumB < maxSeqNum) {
      _readNextB();
      seqNumB = _burst->getPulseSeqNum();
      if (maxSeqNum < seqNumB) {
        maxSeqNum = seqNumB;
        done = false;
      }
    }

  } // while

  cerr << "_readNextPulse, seq num: " << maxSeqNum << endl;

}

/////////////////////////////////////////////////////////////////////////////
// write data for next H pulse
// called by KaDrxPub threads
// Returns pulse data object for recycling

PulseData *KaMerge::writePulseH(PulseData *val)
{
  return _qH->write(val);
}

/////////////////////////////////////////////////////////////////////////////
// write data for next V pulse
// called by KaDrxPub threads
// Returns pulse data object for recycling

PulseData *KaMerge::writePulseV(PulseData *val)
{
  return _qV->write(val);
}

/////////////////////////////////////////////////////////////////////////////
// write data for next burst
// called by KaDrxPub threads
// Returns burst data object for recycling

BurstData *KaMerge::writeBurst(BurstData *val)
{
  return _qB->write(val);
}

/////////////////////////////////////////////////////////////////////////////
// read the next H data

void KaMerge::_readNextH()
{

  PulseData *tmp = NULL;
  while (tmp == NULL) {
    _qH->read(_pulseH);
    if (tmp == NULL) {
      usleep(50);
    }
  }
  _pulseH = tmp;

}

/////////////////////////////////////////////////////////////////////////////
// read the next V data

void KaMerge::_readNextV()
{

  PulseData *tmp = NULL;
  while (tmp == NULL) {
    _qV->read(_pulseV);
    if (tmp == NULL) {
      usleep(50);
    }
  }
  _pulseV = tmp;

}

/////////////////////////////////////////////////////////////////////////////
// read the next burst data

void KaMerge::_readNextB()
{

  BurstData *tmp = NULL;
  while (tmp == NULL) {
    _qB->read(_burst);
    if (tmp == NULL) {
      usleep(50);
    }
  }
  _burst = tmp;

}

/////////////////////////////////////////////////////////////////////////////
/// Return the current time in seconds since 1970/01/01 00:00:00 UTC.
/// Returned value has 1 ms precision.
/// @return the current time in seconds since 1970/01/01 00:00:00 UTC

double KaMerge::_nowTime()
{
  struct timeb timeB;
  ftime(&timeB);
  return timeB.time + timeB.millitm/1000.0;
}

/////////////////////////////////////////////////////////////////////////////
// 1970-01-01 00:00:00 UTC
static const ptime Epoch1970(boost::gregorian::date(1970, 1, 1), time_duration(0, 0, 0));

