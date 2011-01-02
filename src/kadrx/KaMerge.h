#ifndef KA_MERGE_H_
#define KA_MERGE_H_

#include "KaDrxConfig.h"
#include "CircBuffer.h"
#include "PulseData.h"
#include "BurstData.h"
#include <radar/iwrf_data.h>
#include <QThread>

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

  // write data for next H pulse
  // called by KaDrxPub threads
  // Returns pulse object for recycling
  
  PulseData *writePulseH(PulseData *val);

  // write data for next V pulse
  // called by KaDrxPub threads
  // Returns pulse object for recycling
  
  PulseData *writePulseV(PulseData *val);

  // write data for next burst
  // called by KaDrxPub threads
  // Returns burst data object for recycling

  BurstData *writeBurst(BurstData *val);

private:

  /// configuration

  const KaDrxConfig &_config;

  /// The queue size - for buffering IQ data

  size_t _queueSize;
  
  ///  Port for IWRF TCP server
  
  int _iwrfServerTcpPort;

  ///  Queues for data from channels
  
  CircBuffer<PulseData> *_qH;
  CircBuffer<PulseData> *_qV;
  CircBuffer<BurstData> *_qB;

  /// objects for reading the buffered data
  
  PulseData *_pulseH;
  PulseData *_pulseV;
  BurstData *_burst;

  /// IWRF data

  iwrf_radar_info_t _radarInfo;
  iwrf_ts_processing_t _tsProc;
  iwrf_calibration_t _calib;
  iwrf_pulse_header_t _pulseHdr;
  
  int _nGates;
  int16_t *_iq;
  int _nGatesAlloc;
  int64_t _nPulsesSent;
  int _pulseIntervalPerIwrfMetaData;

  /// methods

  void _readNextPulse();
  void _syncPulsesAndBurst();
  void _readNextH();
  void _readNextV();
  void _readNextB();
  void _sendIwrfMetaData();
  void _cohereIqToBurstPhase();
  void _assembleIwrfPulsePacket();
  void _sendIwrfPulsePacket();
  void _allocIq();
  double _nowTime();
  
};

#endif /*KADRXPUB_H_*/
