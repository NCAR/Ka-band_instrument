#include "KaMerge.h"
#include <logx/Logging.h>
#include <sys/timeb.h>
#include <cmath>
#include <cstdlib>
#include <unistd.h>
#include <iostream>
#include <iomanip>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <radar/iwrf_functions.hh>
#include <toolsa/pmu.h>
#include <toolsa/TaXml.hh>

using namespace boost::posix_time;
using namespace std;

LOGGING("KaMerge")

/////////////////////////////////////////////////////////////////////////////
// 1970-01-01 00:00:00 UTC
static const ptime Epoch1970(boost::gregorian::date(1970, 1, 1),
                             time_duration(0, 0, 0));

///////////////////////////////////////////////////////////////////////////

KaMerge::KaMerge(const KaDrxConfig& config, const KaMonitor& kaMonitor) :
        QThread(),
        _config(config),
        _kaMonitor(kaMonitor)
{

  // initialize

  _queueSize = _config.merge_queue_size();
  _iwrfServerTcpPort = _config.iwrf_server_tcp_port();

  // queues

  _qH = new CircBuffer<PulseData>(_queueSize);
  _qV = new CircBuffer<PulseData>(_queueSize);
  _qB = new CircBuffer<BurstData>(_queueSize);

  // pulse and burst data for reading from queues

  _pulseH = new PulseData;
  _pulseV = new PulseData;
  _burst = new BurstData;

  // iq data

  _nGates = 0;
  _pulseBuf = NULL;
  _iq = NULL;
  _nGatesAlloc = 0;
  _pulseIntervalPerIwrfMetaData =
    config.pulse_interval_per_iwrf_meta_data();
  _pulseBufLen = 0;

  // burst data

  _nSamplesBurst = 0;
  _nSamplesBurstAlloc = 0;
  _burstIq = NULL;
  _burstBuf = NULL;
  _burstBufLen = 0;

  // status xml

  _statusBuf = NULL;
  _statusLen = 0;
  _statusBufLen = 0;

  // I and Q count scaling factor to get power in mW easily:
  // mW = (I_count / _iqScaleForMw)^2 + (Q_count / _iqScaleForMw)^2
  _iqScaleForMw = _config.iqcount_scale_for_mw();

  _cohereIqToBurst = _config.cohere_iq_to_burst();
  _combineEverySecondGate = _config.combine_every_second_gate();

  // pulse seq num and times

  _pulseSeqNum = -1;
  _timeSecs = 0;
  _nanoSecs = 0;

  _prevPulseSeqNum = 0;
  _prevTimeSecs = 0;
  _prevNanoSecs = 0;

  // initialize IWRF radar_info struct from config

  _packetSeqNum = 0;

  iwrf_radar_info_init(_radarInfo);
  _radarInfo.latitude_deg = _config.latitude();
  _radarInfo.longitude_deg = _config.longitude();
  _radarInfo.altitude_m = _config.altitude();
  _radarInfo.platform_type = IWRF_RADAR_PLATFORM_FIXED;
  _radarInfo.beamwidth_deg_h = _config.ant_hbeam_width();
  _radarInfo.beamwidth_deg_v = _config.ant_vbeam_width();
  double freqHz = _config.tx_cntr_freq();
  double lightSpeedMps = 2.99792458e8;
  double wavelengthM = lightSpeedMps / freqHz;
  _radarInfo.wavelength_cm = wavelengthM * 100.0;
  _radarInfo.nominal_gain_ant_db_h = _config.ant_gain();
  _radarInfo.nominal_gain_ant_db_v = _config.ant_gain();
  strncpy(_radarInfo.radar_name, _config.radar_id().c_str(),
          IWRF_MAX_RADAR_NAME - 1);

  // initialize IWRF ts_processing struct from config

  iwrf_ts_processing_init(_tsProc);
  if (_config.ldr_mode()) {
    _tsProc.xmit_rcv_mode = IWRF_H_ONLY_FIXED_HV;
  } else {
    _tsProc.xmit_rcv_mode = IWRF_SIM_HV_FIXED_HV;
  }
  _tsProc.xmit_phase_mode = IWRF_XMIT_PHASE_MODE_FIXED;
  _tsProc.prf_mode = IWRF_PRF_MODE_FIXED;
  _tsProc.pulse_type = IWRF_PULSE_TYPE_RECT;
  _tsProc.prt_usec = _config.prt1() * 1.0e6;
  _tsProc.prt2_usec = _config.prt2() * 1.0e6;
  _tsProc.cal_type = IWRF_CAL_TYPE_CW_CAL;
  _tsProc.burst_range_offset_m =
    _config.burst_sample_delay() * lightSpeedMps / 2.0;
  _tsProc.pulse_width_us = _config.tx_pulse_width() * 1.0e6;
  _tsProc.gate_spacing_m = _config.rcvr_pulse_width() * 1.5e8;
  _tsProc.start_range_m = _config.range_to_gate0(); // center of gate 0
  if (_combineEverySecondGate) {
    _tsProc.start_range_m += _tsProc.gate_spacing_m / 2.0;
    _tsProc.gate_spacing_m *= 2.0;
  }
  _tsProc.pol_mode = IWRF_POL_MODE_HV_SIM;

  // initialize IWRF calibration struct from config

  iwrf_calibration_init(_calib);
  _calib.wavelength_cm = _radarInfo.wavelength_cm;
  _calib.beamwidth_deg_h = _radarInfo.beamwidth_deg_h;
  _calib.beamwidth_deg_v = _radarInfo.beamwidth_deg_v;
  _calib.gain_ant_db_h = _radarInfo.nominal_gain_ant_db_h;
  _calib.gain_ant_db_v = _radarInfo.nominal_gain_ant_db_v;
  _calib.pulse_width_us = _config.tx_pulse_width() * 1.0e6;
  if (_config.ldr_mode()) {
    _calib.xmit_power_dbm_h = _config.tx_peak_power();
    _calib.xmit_power_dbm_v = 0.0;
  } else {
    _calib.xmit_power_dbm_h = _config.tx_peak_power() / 2.0;
    _calib.xmit_power_dbm_v = _config.tx_peak_power() / 2.0;
  }
  _calib.two_way_waveguide_loss_db_h = _config.tx_waveguide_loss() + 3.0;
  _calib.two_way_waveguide_loss_db_v = _config.tx_waveguide_loss() + 3.0;
  _calib.power_meas_loss_db_h = _config.tx_peak_pwr_coupling();
  _calib.power_meas_loss_db_v = _config.tx_peak_pwr_coupling();

  // initialize IWRF scan segment for simulation angles

  iwrf_scan_segment_init(_simScan);
  _simScan.scan_mode = IWRF_SCAN_MODE_AZ_SUR_360;

  // initialize pulse header

  iwrf_pulse_header_init(_pulseHdr);

  // initialize burst IQ

  iwrf_burst_header_init(_burstHdr);
  _burstSampleFreqHz= _config.burst_sample_frequency();

  /// PRT mode

  _staggeredPrt = false;
  if (_config.staggered_prt() != KaDrxConfig::UNSET_BOOL) {
    _staggeredPrt = _config.staggered_prt();
  }
  _prt1 = 1.0e-3;
  if (_config.prt1() != KaDrxConfig::UNSET_DOUBLE) {
    _prt1 = _config.prt1();
  }

  /// simulation of antenna angles

  _simAntennaAngles = false;
  _simNElev = 10;
  _simStartElev = 0.5;
  _simDeltaElev = 1.0;
  _simAzRate = 10.0;
  if (_config.simulate_antenna_angles() != KaDrxConfig::UNSET_BOOL) {
    _simAntennaAngles = _config.simulate_antenna_angles();
  }
  if (_config.sim_n_elev() != KaDrxConfig::UNSET_INT) {
    _simNElev = _config.sim_n_elev();
  }
  if (_config.sim_start_elev() != KaDrxConfig::UNSET_DOUBLE) {
    _simStartElev = _config.sim_start_elev();
  }
  if (_config.sim_delta_elev() != KaDrxConfig::UNSET_DOUBLE) {
    _simDeltaElev = _config.sim_delta_elev();
  }
  if (_config.sim_az_rate() != KaDrxConfig::UNSET_DOUBLE) {
    _simAzRate = _config.sim_az_rate();
  }
  _simElev = _simStartElev;
  _simAz = 0.0;
  _simVolNum = 0;
  _simSweepNum = 0;

  // server

  _serverIsOpen = false;
  _sock = NULL;

}

/////////////////////////////////////////////////////////////////////////////
KaMerge::~KaMerge()

{

  _closeSocketToClient();

  delete _qH;
  delete _qV;
  delete _qB;

  delete _pulseH;
  delete _pulseV;
  delete _burst;

  if (_pulseBuf) {
    delete[] _pulseBuf;
  }

  if (_burstBuf) {
    delete[] _burstBuf;
  }

}

/////////////////////////////////////////////////////////////////////////////
//
// Thread run method

void KaMerge::run()

{
  // Time of the last status packet we generated
  time_t lastStatusTime = 0;
  
  // Interval between generating status packets, in seconds
  const int StatusInterval = 2;
  
  // Since we have no event loop,
  // allow thread termination via the terminate() method.

  setTerminationEnabled(true);
  
  // start the loop

  while (true) {

    // read in next pulse

    _readNextPulse();

    // determine number of gates

    int nGates = _pulseH->getNGates();
    if (nGates < _pulseV->getNGates()) {
      nGates = _pulseV->getNGates();
    }
    
    // should we send meta-data?
    
    bool sendMeta = false;
    if (nGates != _nGates) {
      sendMeta = true;
      _nGates = nGates;
    }
    if (_pulseSeqNum % _pulseIntervalPerIwrfMetaData == 0) {
      sendMeta = true;
    }
    
    if (sendMeta) {
      _sendIwrfMetaData();
    }
    
    // cohere the IQ data to the burst phase
    
    if (_cohereIqToBurst) {
      _cohereIqToBurstPhase();
    }
    
    // assemble the IWRF burst packet
    
    _assembleIwrfBurstPacket();
    
    // send out the IWRF burst packet
    
    _sendIwrfBurstPacket();
    
    // assemble the IWRF pulse packet
    
    _assembleIwrfPulsePacket();
    
    // send out the IWRF pulse packet
    
    _sendIwrfPulsePacket();
    
    // If it's been long enough since our last status packet, generate a new
    // one now.
    time_t now = time(0);
    if ((now - lastStatusTime) >= StatusInterval) {
        _assembleStatusPacket();
        _sendIwrfStatusXmlPacket();
        lastStatusTime = now;
    }
    
  } // while

}

/////////////////////////////////////////////////////////////////////////////
// read the next set of pulse data

void KaMerge::_readNextPulse()
{

  PMU_auto_register("reading pulses");

  // read next pulse data for each channel

  _readNextH();
  _readNextV();
  _readNextB();

  // synchronize the pulses and burst to have same sequence number,
  // reading extra puses as required
  
  _syncPulsesAndBurst();

  if (_pulseSeqNum < 0) {
    // first time
    _prevPulseSeqNum = _pulseH->getPulseSeqNum() - 1;
    _prevTimeSecs = _pulseH->getTimeSecs();
    _prevNanoSecs = _pulseH->getNanoSecs();
  } else {
    _prevPulseSeqNum = _pulseSeqNum;
    _prevTimeSecs = _timeSecs;
    _prevNanoSecs = _nanoSecs;
  }

  _pulseSeqNum = _pulseH->getPulseSeqNum();
  _timeSecs = _pulseH->getTimeSecs();
  _nanoSecs = _pulseH->getNanoSecs();
  
  if (! (_pulseSeqNum % 1000)) {
    DLOG << "got 1000 pulses, seqNum, secs, nanoSecs: "
         << _pulseSeqNum << ", " << _timeSecs << ", " << _nanoSecs;
  }

  if (_pulseSeqNum != _prevPulseSeqNum + 1) {
    int nMissing = _pulseSeqNum - _prevPulseSeqNum - 1;
    cerr << "Missing pulses - nmiss, prevNum, thisNum: "
         << nMissing << ", "
         << _prevPulseSeqNum << ", "
         << _pulseSeqNum << endl;
  }

  if (_combineEverySecondGate) {
    _pulseH->combineEverySecondGate();
    _pulseV->combineEverySecondGate();
  }

}

/////////////////////////////////////////////////////////////////////////////
// synchronize the pulses and burst to have same sequence number,
// reading extra puses as required

void KaMerge::_syncPulsesAndBurst()
{

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
    tmp = _qH->read(_pulseH);
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
    tmp = _qV->read(_pulseV);
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
    tmp = _qB->read(_burst);
    if (tmp == NULL) {
      usleep(50);
    }
  }
  _burst = tmp;

}

/////////////////////////////////////////////////////////////////////////////
// send the IWRF meta data

void KaMerge::_sendIwrfMetaData()
{

  // set seq num and time in packet headers

  _radarInfo.packet.seq_num = _packetSeqNum++;
  _radarInfo.packet.time_secs_utc = _timeSecs;
  _radarInfo.packet.time_nano_secs = _nanoSecs;

  _tsProc.packet.seq_num = _packetSeqNum++;
  _tsProc.packet.time_secs_utc = _timeSecs;
  _tsProc.packet.time_nano_secs = _nanoSecs;

  _calib.packet.seq_num = _packetSeqNum++;
  _calib.packet.time_secs_utc = _timeSecs;
  _calib.packet.time_nano_secs = _nanoSecs;

  // check that socket to client is open

  if (_openSocketToClient()) {
    return;
  }

  // write individual messages for each struct

  if (_sock->writeBuffer(&_radarInfo, sizeof(_radarInfo))) {
    cerr << "ERROR - KaMerge::_sendIwrfMetaData()" << endl;
    cerr << "  Writing IWRF_RADAR_INFO" << endl;
    cerr << "  " << _sock->getErrStr() << endl;
    _closeSocketToClient();
    return;
  }
  
  if (_sock->writeBuffer(&_tsProc, sizeof(_tsProc))) {
    cerr << "ERROR - KaMerge::_sendIwrfMetaData()" << endl;
    cerr << "  Writing IWRF_TS_PROCESSING" << endl;
    cerr << "  " << _sock->getErrStr() << endl;
    _closeSocketToClient();
    return;
  }
  
  if (_sock->writeBuffer(&_calib, sizeof(_calib))) {
    cerr << "ERROR - KaMerge::_sendIwrfMetaData()" << endl;
    cerr << "  Writing IWRF_CALIBRATION" << endl;
    cerr << "  " << _sock->getErrStr() << endl;
    _closeSocketToClient();
    return;
  }

}

/////////////////////////////////////////////////////////////////////////////
// cohere the IQ data to the burst phase

void KaMerge::_cohereIqToBurstPhase()
{

  _cohereIqToBurstPhase(*_pulseH, *_burst);
  _cohereIqToBurstPhase(*_pulseV, *_burst);

}

void KaMerge::_cohereIqToBurstPhase(PulseData &pulse,
                                    const BurstData &burst)
{

  double g0IvalNorm = burst.getG0IvalNorm();
  double g0QvalNorm = burst.getG0QvalNorm();

  int nGates = pulse.getNGates();
  int16_t *II = pulse.getIq();
  int16_t *QQ = pulse.getIq() + 1;
  
  for (int igate = 0; igate < nGates; igate++, II += 2, QQ += 2) {
    
    double ival = *II;
    double qval = *QQ;

    double ivalCohered = ival * g0IvalNorm + qval * g0QvalNorm;
    double qvalCohered = qval * g0IvalNorm - ival * g0QvalNorm;

    if (ivalCohered < -32767.0) {
      ivalCohered = -32767.0;
    } else if (ivalCohered > 32767.0) {
      ivalCohered = 32767.0;
    }

    if (qvalCohered < -32767.0) {
      qvalCohered = -32767.0;
    } else if (qvalCohered > 32767.0) {
      qvalCohered = 32767.0;
    }

    *II = (int16_t) (ivalCohered + 0.5);
    *QQ = (int16_t) (qvalCohered + 0.5);
    
  } // igate

}

/////////////////////////////////////////////////////////////////////////////
// assemble IWRF pulse packet

void KaMerge::_assembleIwrfPulsePacket()
{

  // allocate space for IQ data
  
  _allocPulseBuf();

  // load up IQ data, H followed by V

  memcpy(_iq, _pulseH->getIq(), _pulseH->getNGates() * 2 * sizeof(int16_t));
  memcpy(_iq + (_nGates * 2),
         _pulseV->getIq(), _pulseV->getNGates() * 2 * sizeof(int16_t));
  int nSamples = _burst->getNSamples();
  if (nSamples > _nGates) nSamples = _nGates;
  memcpy(_iq + (_nGates * 4),
         _burst->getIq(), nSamples * 2 * sizeof(int16_t));

  // pulse header

  _pulseHdr.packet.len_bytes = _pulseBufLen;
  _pulseHdr.packet.seq_num = _packetSeqNum++;
  _pulseHdr.packet.time_secs_utc = _timeSecs;
  _pulseHdr.packet.time_nano_secs = _nanoSecs;

  _pulseHdr.pulse_seq_num = _pulseSeqNum;

  if (_staggeredPrt) {
     _pulseHdr.prt =
       (double) (_timeSecs - _prevTimeSecs) +
       (double) (_nanoSecs - _prevNanoSecs) * 1.0e-9;
  } else {
    _pulseHdr.prt = _prt1;
  }

  _pulseHdr.pulse_width_us = _tsProc.pulse_width_us;
  _pulseHdr.n_gates = _nGates;
  _pulseHdr.n_channels = NCHANNELS;
  _pulseHdr.iq_encoding = IWRF_IQ_ENCODING_SCALED_SI16;
  _pulseHdr.hv_flag = 1;
  _pulseHdr.phase_cohered = _cohereIqToBurst;
  _pulseHdr.n_data = _nGates * NCHANNELS * 2;
  _pulseHdr.iq_offset[0] = 0;
  _pulseHdr.iq_offset[1] = _nGates * 2;
  _pulseHdr.iq_offset[2] = _nGates * 4;
  _pulseHdr.burst_mag[0] = _burst->getG0Magnitude();
  _pulseHdr.burst_mag[1] = _burst->getG0Magnitude();
  _pulseHdr.burst_arg[0] = _burst->getG0PhaseDeg();
  _pulseHdr.burst_arg[1] = _burst->getG0PhaseDeg();
  _pulseHdr.scale = 1.0 / _iqScaleForMw;
  _pulseHdr.offset = 0.0;
  _pulseHdr.n_gates_burst = 0;
  _pulseHdr.start_range_m = _tsProc.start_range_m;
  _pulseHdr.gate_spacing_m = _tsProc.gate_spacing_m;

  if (_simAntennaAngles) {
    double dAz = _pulseHdr.prt * _simAzRate;
    _simAz += dAz;
    if (_simAz >= 360.0) {
      _simAz = 0.0;
      _simSweepNum++;
      _simElev += _simDeltaElev;
      if (_simSweepNum == 10) {
        _simElev = _simStartElev;
        _simSweepNum = 0;
        _simVolNum++;
      }
    }
    _pulseHdr.scan_mode = IWRF_SCAN_MODE_AZ_SUR_360;
    _pulseHdr.volume_num = _simVolNum;
    _pulseHdr.sweep_num = _simSweepNum;
    _pulseHdr.fixed_el = _simElev;
    _pulseHdr.elevation = _simElev;
    _pulseHdr.azimuth = _simAz;
  }
  
  memcpy(_pulseBuf, &_pulseHdr, sizeof(_pulseHdr));

}

/////////////////////////////////////////////////////////////////////////////
// send out the IWRF pulse packet

void KaMerge::_sendIwrfPulsePacket()
{

  // check that socket to client is open

  if (_openSocketToClient()) {
    return;
  }
  
  if (_sock->writeBuffer(_pulseBuf, _pulseBufLen)) {
    cerr << "ERROR - KaMerge::_sendIwrfPulsePacket()" << endl;
    cerr << "  Writing pulse packet" << endl;
    cerr << "  " << _sock->getErrStr() << endl;
    _closeSocketToClient();
    return;
  }
  
}

/////////////////////////////////////////////////////////////////////////////
// allocate pulse buffer

void KaMerge::_allocPulseBuf()
{

  if (_nGates > _nGatesAlloc) {

    if (_pulseBuf) {
      delete[] _pulseBuf;
    }

    _pulseBufLen =
      sizeof(iwrf_pulse_header) + (_nGates * NCHANNELS * 2 * sizeof(int16_t));
    _pulseBuf = new char[_pulseBufLen];
    _iq = reinterpret_cast<int16_t *>(_pulseBuf + sizeof(iwrf_pulse_header));

    _nGatesAlloc = _nGates;

  }

  memset(_iq, 0, _nGates * NCHANNELS * 2 * sizeof(int16_t));

}

/////////////////////////////////////////////////////////////////////////////
// assemble IWRF burst packet

void KaMerge::_assembleIwrfBurstPacket()
{

  // allocate space for Burst IQ samples

  _nSamplesBurst = _burst->getNSamples();
  _allocBurstBuf();
  
  // load up IQ data
  
  memcpy(_burstIq, _burst->getIq(), _nSamplesBurst * 2 * sizeof(int16_t));
  
  // burst header

  _burstHdr.packet.len_bytes = _burstBufLen;
  _burstHdr.packet.seq_num = _packetSeqNum++;
  _burstHdr.packet.time_secs_utc = _timeSecs;
  _burstHdr.packet.time_nano_secs = _nanoSecs;
  
  _burstHdr.pulse_seq_num = _pulseSeqNum;
  _burstHdr.n_samples = _nSamplesBurst;
  _burstHdr.channel_id = 0;
  _burstHdr.iq_encoding = IWRF_IQ_ENCODING_SCALED_SI16;
  _burstHdr.scale = 1.0 / _iqScaleForMw;
  _burstHdr.offset = 0.0;
  _burstHdr.power_dbm = _burst->getG0PowerDbm();
  _burstHdr.phase_deg = _burst->getG0PhaseDeg();
  _burstHdr.freq_hz = _burst->getG0FreqHz();
  _burstHdr.sampling_freq_hz = _burstSampleFreqHz;

  memcpy(_burstBuf, &_burstHdr, sizeof(_burstHdr));

}

/////////////////////////////////////////////////////////////////////////////
// send out the IWRF burst packet

void KaMerge::_sendIwrfBurstPacket()
{

  // check that socket to client is open

  if (_openSocketToClient()) {
    return;
  }
  
  if (_sock->writeBuffer(_burstBuf, _burstBufLen)) {
    cerr << "ERROR - KaMerge::_sendIwrfBurstPacket()" << endl;
    cerr << "  Writing burst packet" << endl;
    cerr << "  " << _sock->getErrStr() << endl;
    _closeSocketToClient();
    return;
  }

}

/////////////////////////////////////////////////////////////////////////////
// allocate burst buffer

void KaMerge::_allocBurstBuf()
{
  
  if (_nSamplesBurst > _nSamplesBurstAlloc) {
    
    if (_burstBuf) {
      delete[] _burstBuf;
    }

    _burstBufLen =
      sizeof(iwrf_burst_header_t) + (_nSamplesBurst * 2 * sizeof(int16_t));
    _burstBuf = new char[_burstBufLen];
    _burstIq = reinterpret_cast<int16_t *>
      (_burstBuf + sizeof(iwrf_burst_header_t));
    
    _nSamplesBurstAlloc = _nSamplesBurst;

  }

  memset(_burstIq, 0, _nSamplesBurst * 2 * sizeof(int16_t));

}

/////////////////////////////////////////////////////////////////////////////
// assemble IWRF status packet

void KaMerge::_assembleStatusPacket()

{

  // assemble the xml

  string xmlStr = _assembleStatusXml();
  int xmlLen = xmlStr.size() + 1; // null terminated

  DLOG << "========= Status XML ========";
  DLOG << xmlStr;
  DLOG << "======= End status XML ======";

  // allocate buffer

  _allocStatusBuf(xmlLen);

  // set header

  iwrf_status_xml_t hdr;
  iwrf_status_xml_init(hdr);
  hdr.packet.len_bytes = _statusLen;
  hdr.xml_len = xmlLen;

  // copy data into buffer

  memcpy(_statusBuf, &hdr, sizeof(iwrf_status_xml_t));
  memcpy(_statusBuf + sizeof(iwrf_status_xml_t), xmlStr.c_str(), xmlLen);

}

/////////////////////////////////////////////////////////////////////////////
// assemble status XML
// returns the XML string

string KaMerge::_assembleStatusXml()

{

  string xml;

  // main block

  xml += TaXml::writeStartTag("KaStatus", 0);

  // transmit block
  
  xml += TaXml::writeStartTag("KaTransmitterStatus", 1);

  const XmitClient::XmitStatus &xs = _kaMonitor.transmitterStatus();

  xml += TaXml::writeBoolean
    ("SerialConnected", 2, xs.serialConnected());
  xml += TaXml::writeBoolean
    ("FaultSummary", 2, xs.faultSummary());
  xml += TaXml::writeBoolean
    ("HvpsRunup", 2, xs.hvpsRunup());
  xml += TaXml::writeBoolean
    ("Standby", 2, xs.standby());
  xml += TaXml::writeBoolean
    ("HeaterWarmup", 2, xs.heaterWarmup());
  xml += TaXml::writeBoolean
    ("Cooldown", 2, xs.cooldown());
  xml += TaXml::writeBoolean
    ("UnitOn", 2, xs.unitOn());
  xml += TaXml::writeBoolean
    ("MagnetronCurrentFault", 2, xs.magnetronCurrentFault());
  xml += TaXml::writeBoolean
    ("BlowerFault", 2, xs.blowerFault());
  xml += TaXml::writeBoolean
    ("HvpsOn", 2, xs.hvpsOn());
  xml += TaXml::writeBoolean
    ("RemoteEnabled", 2, xs.remoteEnabled());
  xml += TaXml::writeBoolean
    ("SafetyInterlock", 2, xs.safetyInterlock());
  xml += TaXml::writeBoolean
    ("ReversePowerFault", 2, xs.reversePowerFault());
  xml += TaXml::writeBoolean
    ("PulseInputFault", 2, xs.pulseInputFault());
  xml += TaXml::writeBoolean
    ("HvpsCurrentFault", 2, xs.hvpsCurrentFault());
  xml += TaXml::writeBoolean
    ("WaveguidePressureFault", 2, xs.waveguidePressureFault
     ());
  xml += TaXml::writeBoolean
    ("HvpsUnderVoltage", 2, xs.hvpsUnderVoltage());
  xml += TaXml::writeBoolean
    ("HvpsOverVoltage", 2, xs.hvpsOverVoltage());
  
  xml += TaXml::writeDouble
    ("HvpsVoltage", 2, xs.hvpsVoltage());
  xml += TaXml::writeDouble
    ("MagnetronCurrent", 2, xs.magnetronCurrent());
  xml += TaXml::writeDouble
    ("HvpsCurrent", 2, xs.hvpsCurrent());
  xml += TaXml::writeDouble
    ("Temperature", 2, xs.temperature());
  
  xml += TaXml::writeInt
    ("MagnetronCurrentFaultCount", 2, xs.magnetronCurrentFaultCount());
  xml += TaXml::writeInt
    ("BlowerFaultCount", 2, xs.blowerFaultCount());
  xml += TaXml::writeInt
    ("SafetyInterlockCount", 2, xs.safetyInterlockCount());
  xml += TaXml::writeInt
    ("ReversePowerFaultCount", 2, xs.reversePowerFaultCount());
  xml += TaXml::writeInt
    ("PulseInputFaultCount", 2, xs.pulseInputFaultCount());
  xml += TaXml::writeInt
    ("HvpsCurrentFaultCount", 2, xs.hvpsCurrentFaultCount());
  xml += TaXml::writeInt
    ("WaveguidePressureFaultCount", 2, xs.waveguidePressureFaultCount());
  xml += TaXml::writeInt
    ("HvpsUnderVoltageCount", 2, xs.hvpsUnderVoltageCount());
  xml += TaXml::writeInt
    ("HvpsOverVoltageCount", 2, xs.hvpsOverVoltageCount());
  xml += TaXml::writeInt
    ("AutoPulseFaultResets", 2, xs.autoPulseFaultResets());

  xml += TaXml::writeEndTag("KaTransmitterStatus", 1);

  // receive block

  const KaMonitor &km = _kaMonitor;

  xml += TaXml::writeStartTag("KaReceiverStatus", 1);

  xml += TaXml::writeDouble
    ("ProcEnclosureTemp", 2, km.procEnclosureTemp());
  xml += TaXml::writeDouble
    ("ProcDrxTemp", 2, km.procDrxTemp());
  xml += TaXml::writeDouble
    ("TxEnclosureTemp", 2, km.txEnclosureTemp());
  xml += TaXml::writeDouble
    ("RxTopTemp", 2, km.rxTopTemp());
  xml += TaXml::writeDouble
    ("RxBackTemp", 2, km.rxBackTemp());
  xml += TaXml::writeDouble
    ("RxFrontTemp", 2, km.rxFrontTemp());
  xml += TaXml::writeDouble
    ("HTxPowerVideo", 2, km.hTxPowerVideo());
  xml += TaXml::writeDouble
    ("VTxPowerVideo", 2, km.vTxPowerVideo());
  xml += TaXml::writeDouble
    ("TestTargetPowerVideo", 2, km.testTargetPowerVideo());
  xml += TaXml::writeDouble
    ("PsVoltage", 2, km.psVoltage());
  
  xml += TaXml::writeBoolean
    ("WgPressureGood", 2, km.wgPressureGood());
  xml += TaXml::writeBoolean
    ("Locked100MHz", 2, km.locked100MHz());
  xml += TaXml::writeBoolean
    ("GpsTimeServerGood", 2, km.gpsTimeServerGood());
  
  xml += TaXml::writeEndTag("KaReceiverStatus", 1);

  xml += TaXml::writeEndTag("KaStatus", 0);

  return xml;

}

/////////////////////////////////////////////////////////////////////////////
// send out the IWRF status packet

void KaMerge::_sendIwrfStatusXmlPacket()

{

  // check that socket to client is open

  if (_openSocketToClient()) {
    return;
  }
  
  if (_sock->writeBuffer(_statusBuf, _statusLen)) {
    cerr << "ERROR - KaMerge::_sendIwrfStatusXmlPacket()" << endl;
    cerr << "  Writing status xml packet" << endl;
    cerr << "  " << _sock->getErrStr() << endl;
    _closeSocketToClient();
    return;
  }

}

/////////////////////////////////////////////////////////////////////////////
// allocate status buffer

void KaMerge::_allocStatusBuf(size_t xmlLen)
{
  _statusLen = xmlLen + sizeof(iwrf_status_xml_t);
  if (_statusLen > _statusBufLen) {
    if (_statusBuf) {
      delete[] _statusBuf;
    }
    _statusBuf = new char[_statusLen];
    _statusBufLen = _statusLen;
  }
}

//////////////////////////////////////////////////
// open server
// Returns 0 on success, -1 on failure

int KaMerge::_openServer()

{

  if (_serverIsOpen) {
    return 0;
  }

  if (_server.openServer(_iwrfServerTcpPort)) {
    cerr << "ERROR - KaMerge::_openServer" << endl;
    cerr << "  Cannot open server, port: " << _iwrfServerTcpPort << endl;
    cerr << "  " << _server.getErrStr() << endl;
    return -1;
  }

  DLOG << "====>> TCP server opened <<====";
  _serverIsOpen = true;
  return 0;

}

//////////////////////////////////////////////////
// open socket to client
// Returns 0 on success, -1 on failure

int KaMerge::_openSocketToClient()

{

  if (_openServer()) {
    return -1;
  }

  // check status

  if (_sock && _sock->isOpen()) {
    return 0;
  }

  // get a client if one is out there

  _sock = _server.getClient(0);

  if (_sock == NULL) {
    return -1;
  }

  DLOG << "====>> Connected to client <<====";

  return 0;
  
}

//////////////////////////////////////////////////
// close socket to client

void KaMerge::_closeSocketToClient()

{
  
  if (_sock == NULL) {
    return;
  }

  if (_sock->isOpen()) {
    _sock->close();
  }

  delete _sock;
  _sock = NULL;

}
