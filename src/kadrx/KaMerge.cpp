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

