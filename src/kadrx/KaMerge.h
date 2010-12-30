#ifndef KA_MERGE_H_
#define KA_MERGE_H_

#include "KaDrxConfig.h"
#include <boost/thread/mutex.hpp>
#include <QThread>
#include <deque>
class PulseData;
class BurstData;

/// KaMerge merges data from the H and V channels, and the burst channel,
/// converts to IWRF time series format and writes the IWRF data to a client
/// via TCP.
///
/// The bulk of the work done in this object is run in a separate thread.
///
/// The 3 instances of the KaDrxPub class each write their data to this
/// object, inserting it into buffers (deques). The merge operation is
/// carried out by removing items from the queues, synchronizing on the
/// pulse numbers, and merging the data.
///
/// This thread acts as a server, listening for a client to connect. Only
/// a single client is supported, given the high bandwidth of the data
/// stream. If no client is connected, the data is discarded.
///
/// Since KaMerge does not use a Qt event loop at this point, the thread
/// should be stopped by calling its terminate() method.

class KaMerge : public QThread {

  Q_OBJECT

public:
    
  /**
   * Constructor.
   * @param config KaDrxConfig defining the desired configuration.
   * @param publish should we publish data via DDS?
   */
  KaMerge(const KaDrxConfig& config);
  
  /// Destructor
  virtual ~KaMerge();

  /// thread run method
  void run();

  /////////////////////////////////////////////////////////////////////////////
  // pop an H pulse data from queue
  // returns NULL if none available (queue too small)

  PulseData *popPulseDataH();

private:

  /// configuration
  const KaDrxConfig& _config;

  /// Return the current time in seconds since 1970/01/01 00:00:00 UTC.
  /// Returned value has 1 ms precision.
  /// @return the current time in seconds since 1970/01/01 00:00:00 UTC
  double _nowTime();
  
  /**
   * The queue size - for buffering IQ data
   */
  size_t _queueSize;
  
  /**
   * Port for IWRF TCP server
   */
  int _iwrfServerTcpPort;

  /**
   * Queues for data from channels
   */

  std::deque<PulseData *> _qH;
  std::deque<PulseData *> _qV;
  std::deque<BurstData *> _qB;

  /// Mutexes for thread safety

  mutable boost::mutex _mutexH;
  mutable boost::mutex _mutexV;
  mutable boost::mutex _mutexB;

};

#endif /*KADRXPUB_H_*/
