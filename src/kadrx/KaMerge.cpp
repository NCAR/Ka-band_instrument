#include "KaMerge.h"
#include "PulseData.h"
#include "BurstData.h"
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

}

/////////////////////////////////////////////////////////////////////////////
KaMerge::~KaMerge()

{

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
    usleep(100);
  }
}

/////////////////////////////////////////////////////////////////////////////
double KaMerge::_nowTime() {
  struct timeb timeB;
  ftime(&timeB);
  return timeB.time + timeB.millitm/1000.0;
}

/////////////////////////////////////////////////////////////////////////////
// 1970-01-01 00:00:00 UTC
static const ptime Epoch1970(boost::gregorian::date(1970, 1, 1), time_duration(0, 0, 0));

/////////////////////////////////////////////////////////////////////////////
// pop an H pulse data from queue
// returns NULL if none available (queue too small)

PulseData *KaMerge::popPulseDataH()

{

  boost::mutex::scoped_lock guard(_mutexH);

  if (_qH.size() < _queueSize) {
    return NULL;
  }

  PulseData *pdata = _qH.back();
  _qH.pop_back();
  return pdata;

}

