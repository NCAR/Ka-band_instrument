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

using namespace boost::posix_time;

LOGGING("KaMerge")

///////////////////////////////////////////////////////////////////////////

KaMerge::KaMerge(const KaDrxConfig& config) :
        QThread(),
        _config(config)
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
  _nPulsesSent = 0;
  _pulseIntervalPerIwrfMetaData =
    config.pulse_interval_per_iwrf_meta_data();

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
  _tsProc.start_range_m = (_tsProc.gate_spacing_m / 2.0) +
    _config.rcvr_gate0_delay() * 1.5e8;
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

  if (_pulseBuf) {
    delete[] _pulseBuf;
  }

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
    // usleep(100);
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
  
  cerr << "_readNextPulse, seqNum, secs, nanoSecs: "
       << _pulseSeqNum << ", " << _timeSecs << ", " << _nanoSecs << endl;

  if (_pulseSeqNum != _prevPulseSeqNum + 1) {
    int nMissing = _prevPulseSeqNum - _pulseSeqNum;
    cerr << "Missing pulses - nmiss, prevNum, thisNum: "
         << nMissing << ", "
         << _prevPulseSeqNum << ", "
         << _pulseSeqNum << endl;
  }

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

  _cohereIqToBurstPhase();

  // assemble the IWRF pulse packet

  _assembleIwrfPulsePacket();

  // send out the IWRF pulse packet

  _sendIwrfPulsePacket();

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
    *QQ = (int16_t) (ivalCohered + 0.5);
    
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

  // pulse header

  _pulseHdr.packet.seq_num = _packetSeqNum++;
  _pulseHdr.packet.time_secs_utc = _timeSecs;
  _pulseHdr.packet.time_nano_secs = _nanoSecs;

  _pulseHdr.pulse_seq_num = _pulseSeqNum;
  _pulseHdr.prt =
    (double) (_timeSecs - _prevTimeSecs) +
    (double) (_nanoSecs - _prevNanoSecs) * 1.0e-9;

  _pulseHdr.pulse_width_us = _tsProc.pulse_width_us;
  _pulseHdr.n_gates = _nGates;
  _pulseHdr.n_channels = 2;
  _pulseHdr.iq_encoding = IWRF_IQ_ENCODING_SCALED_SI16;
  _pulseHdr.hv_flag = 1;
  _pulseHdr.phase_cohered = 1;
  _pulseHdr.n_data = _nGates * 4;
  _pulseHdr.iq_offset[0] = 0;
  _pulseHdr.iq_offset[1] = _nGates * 2;
  _pulseHdr.burst_mag[0] = _burst->getG0Magnitude();
  _pulseHdr.burst_mag[1] = _burst->getG0Magnitude();
  _pulseHdr.burst_arg[0] = _burst->getG0PhaseDeg();
  _pulseHdr.burst_arg[1] = _burst->getG0PhaseDeg();
  _pulseHdr.scale = 1.0 / 65536.0;
  _pulseHdr.offset = 0.0;
  _pulseHdr.n_gates_burst = 0;
  _pulseHdr.start_range_m = _tsProc.start_range_m;
  _pulseHdr.gate_spacing_m = _tsProc.gate_spacing_m;

  memcpy(_pulseBuf, &_pulseHdr, sizeof(_pulseHdr));
  
}

/////////////////////////////////////////////////////////////////////////////
// send out the IWRF pulse packet

void KaMerge::_sendIwrfPulsePacket()
{

}

/////////////////////////////////////////////////////////////////////////////
// allocate pulse buffer

void KaMerge::_allocPulseBuf()
{

  if (_nGates > _nGatesAlloc) {

    if (_pulseBuf) {
      delete[] _pulseBuf;
    }

    int nBytes = sizeof(iwrf_pulse_header) + (_nGates * 4 * sizeof(int16_t));
    _pulseBuf = new char[nBytes];
    _iq = reinterpret_cast<int16_t *>(_pulseBuf + sizeof(iwrf_pulse_header));
    _nGatesAlloc = _nGates;

  }

  memset(_iq, 0, _nGates * 4 * sizeof(int16_t));

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

