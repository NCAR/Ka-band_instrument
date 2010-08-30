/*
 * TimeSeriesAdapter.cpp
 *
 *  Created on: May 20, 2010
 *      Author: burghart
 */

#include <iostream>

#include "TimeSeriesAdapter.h"

static const double SPEED_OF_LIGHT = 2.99792458e8; // m s-1

void
TimeSeriesAdapter::IwrfToDDS(const IwrfTsPulse& iwrfPulse,
        RadarDDS::TimeSeries& ddsPulse) {
    // Shortcut to the IwrfTsInfo from the incoming IwrfTsPulse
    const IwrfTsInfo iwrfInfo = iwrfPulse.getTsInfo();
    
    // Assign from the SysHousekeeping members in order
    RadarDDS::SysHousekeeping& hskp = ddsPulse.hskp;
    
    // SysHousekeeping.timetag - convert from seconds + nanoseconds to 
    // microseconds
    hskp.timetag = 1000000LL * iwrfPulse.getTime() + iwrfPulse.getNanoSecs() / 1000;
    
    // SysHousekeeping.gates
    int nGates = iwrfPulse.getNGates();
    hskp.gates = nGates;

    // SysHousekeeping.chanId - We can handle only channel zero at this point!
    hskp.chanId = 0;
    
    // SysHousekeeping.radar_id
    strcpy(hskp.radar_id.out(), iwrfInfo.get_radar_name().c_str());
    
    // SysHousekeeping.staggered_prt - @TODO only single-PRT handled at this point!
    if (iwrfInfo.get_xmit_info_prf_mode() != IWRF_PRF_MODE_FIXED) {
        std::cerr << "TimeSeriesAdapter::" << __FUNCTION__ << 
                " can currently only handle single-PRT data!" << std::endl;
        abort();
    } else {
        hskp.staggered_prt = false;
    }
    
    // SysHousekeeping.pdpp - @TODO ...not sure how to handle this one
    
    // SysHousekeeping.prt1 - @TODO only single-PRT handled at this point!
    // SysHousekeeping.prt2
    hskp.prt1 = iwrfInfo.get_xmit_info_prt_usec() * 1.0e-6;
    hskp.prt2 = iwrfInfo.get_xmit_info_prt2_usec() * 1.0e-6;
    
    // SysHousekeeping.tx_peak_power - horizontal only, convert dBm -> W
    hskp.tx_peak_power = pow(10.0, 
            (iwrfInfo.get_calib_xmit_power_dbm_h() - 30.0) / 10.0);
    
    // SysHousekeeping.tx_cntr_freq
    hskp.tx_cntr_freq = SPEED_OF_LIGHT / (0.01 * iwrfInfo.get_radar_wavelength_cm());
    
    // SysHousekeeping.tx_chirp_bandwidth - No match in IWRF metadata (?)
    
    // SysHousekeeping.tx_pulse_width
    hskp.tx_pulse_width = 1.0e-6 * iwrfPulse.get_pulse_width_us();
    
    // SysHousekeeping.tx_switching_network_loss - no match in IWRF metadata

    // SysHousekeeping.tx_waveguide_loss - horizontal only!
    // IWRF only has two-way waveguide loss. We make the (probably bad) 
    // assumption that it's equal parts tx and rx waveguide loss...
    hskp.tx_waveguide_loss = iwrfInfo.get_calib_two_way_waveguide_loss_db_h() / 2;
    
    // SysHousekeeping.tx_peak_pwr_coupling - no match in IWRF metadata (?)

    // SysHousekeeping.tx_turn_on_time - no match in IWRF metadata

    // SysHousekeeping.tx_turn_off_time - no match in IWRF metadata
    
    // SysHousekeeping.tx_upconverter_latency - no match in IWRF metadata

    // SysHousekeeping.ant_gain
    hskp.ant_gain = iwrfInfo.get_calib_gain_ant_db_h();

    // SysHousekeeping.ant_hbeam_width
    hskp.ant_hbeam_width = iwrfInfo.get_calib_beamwidth_deg_h();
    
    // SysHousekeeping.ant_vbeam_width
    hskp.ant_vbeam_width = iwrfInfo.get_calib_beamwidth_deg_v();

    // SysHousekeeping.ant_E_plane_angle - no match in IWRF metadata

    // SysHousekeeping.ant_H_plane_angle - no match in IWRF metadata

    // SysHousekeeping.ant_encoder_up - @todo: could maybe use IWRF antenna
    // az correction for this?
    
    // SysHousekeeping.ant_pitch_up - @todo: could maybe use IWRF antenna
    // el correction for this?

    // SysHousekeeping.az
    hskp.az = iwrfPulse.get_azimuth();
    
    // SysHousekeeping.el
    hskp.el = iwrfPulse.get_elevation();

    // SysHousekeeping.rotation_angle - KLUGE: in DDSToIwrf(), we hijack
    // the IWRF antenna azimuth to store the rotation angle.
    hskp.rotation_angle = iwrfPulse.get_azimuth();

    // SysHousekeeping.actual_num_rcvrs
    hskp.actual_num_rcvrs = iwrfPulse.get_n_channels();

    // SysHousekeeping.rcvr_cntr_freq - no match in IWRF metadata
    
    // SysHousekeeping.rcvr_pulse_width
    // gate spacing = 0.5 * <speed of light> * rcvr_pulse_width
    hskp.rcvr_pulse_width = 2 * iwrfPulse.get_gate_spacing_m() / SPEED_OF_LIGHT;

    // SysHousekeeping.rcvr_switching_network_loss - no match in IWRF metadata

    // SysHousekeeping.rcvr_waveguide_loss - horizontal only!
    // IWRF only has two-way waveguide loss. We make the (probably bad) 
    // assumption that it's equal parts tx and rx waveguide loss...
    hskp.rcvr_waveguide_loss = iwrfInfo.get_calib_two_way_waveguide_loss_db_h() / 2;
    
    // SysHousekeeping.rcvr_noise_figure - no match in IWRF metadata

    // SysHousekeeping.rcvr_filter_mismatch
    hskp.rcvr_filter_mismatch = iwrfInfo.get_calib_receiver_mismatch_loss_db();
    
    // SysHousekeeping.rcvr_rf_gain (NOTE: this includes IF gain)
    //
    // SysHousekeeping.rcvr_if_gain
    //
    // SysHousekeeping.rcvr_digital_gain
    //
    // IWRF only has total receiver gain, so we just stuff it all into 
    // rcvr_rf_gain and set rcvr_if_gain and rcvr_digital_gain to zero.
    hskp.rcvr_rf_gain = iwrfInfo.get_calib_receiver_gain_db_hc();
    hskp.rcvr_if_gain = 0.0;        // XXX
    hskp.rcvr_digital_gain = 0.0;   // XXX

    // SysHousekeeping.rcvr_gate0_delay
    // Translate receiver delay to range to start of gate0.
    hskp.rcvr_gate0_delay = 2 * iwrfPulse.get_start_range_m() / SPEED_OF_LIGHT;
    
    // SysHousekeeping.rcvr_bandwidth - no match in IWRF metadata

    // SysHousekeeping.latitude
    hskp.latitude = iwrfInfo.get_radar_latitude_deg();

    // SysHousekeeping.longitude
    hskp.longitude = iwrfInfo.get_radar_longitude_deg();

    // SysHousekeeping.altitude
    hskp.altitude = iwrfInfo.get_radar_altitude_m();

    // SysHousekeeping.supply_voltage_ok - no match in IWRF metadata

    // SysHousekeeping.temp_sensor_1 - no match in IWRF metadata

    // SysHousekeeping.temp_sensor_2 - no match in IWRF metadata

    // SysHousekeeping.temp_sensor_3 - no match in IWRF metadata

    // SysHousekeeping.pressure_sensor_1 - no match in IWRF metadata

    // SysHousekeeping.pressure_sensor_2 - no match in IWRF metadata

    // Get a pointer to the iwrfPulse data, which must be floating point.
    // We may need to make a copy of iwrfPulse to use a non-const operation
    // to get floating point data.
    IwrfTsPulse *iwrfPulseCopy = 0;

    const fl32 *iwrfIq = iwrfPulse.getIq0();
    if (iwrfPulse.get_iq_encoding() != IWRF_IQ_ENCODING_FL32) {
        // Since convertToFL32() is a non-const method for IwrfTsPulse,
        // we need to copy iwrfPulse, then convert the data to fl32, then
        // get a pointer to the data. Keep around the copy until we're done
        // with its data.
        iwrfPulseCopy = new IwrfTsPulse(iwrfPulse);
        iwrfPulseCopy->convertToFL32();
        iwrfIq = iwrfPulseCopy->getIq0();
    }
    
    // set data space in ddsPulse for the right number of gates
    ddsPulse.data.length(2 * nGates);
    
    // The scale factor used by RadarDDS::TimeSeries is:
    //     I       = (I      - ddsOffset) / ddsScale
    //      counts     volts
    // Similar formula for Q.
    float ddsScale = 1.0 / (32768 * sqrt(2.0));
    float ddsOffset = 0.0;
    
    for (int g = 0; g < ddsPulse.hskp.gates; g++) {
        // I
        ddsPulse.data[2 * g] = (iwrfIq[2 * g] - ddsOffset) / ddsScale;
        // Q
        ddsPulse.data[2 * g + 1] = (iwrfIq[2 * g + 1] - ddsOffset) / ddsScale;
    }
    // We're done with the data, so now we can delete our copy of iwrfPulse
    // (if we needed to make one)
    if (iwrfPulseCopy)
        delete(iwrfPulseCopy);
}

void
TimeSeriesAdapter::DDSToIwrf(const RadarDDS::TimeSeries& ddsPulse,
        IwrfTsPulse& iwrfPulse, si64 packetSequenceNum) {
    // Get a reference to the IwrfTsPulse's IwrfTsInfo member
    IwrfTsInfo & iwrfInfo = iwrfPulse.getTsInfo();
    
    // Assign from the SysHousekeeping members in order
    const RadarDDS::SysHousekeeping& hskp = ddsPulse.hskp;
        
    // SysHousekeeping.timetag
    int sec = hskp.timetag / 1000000; // secs since 1970-01-01 00:00:00 UTC
    int nanosec = (hskp.timetag % 1000000) * 1000; // usecs -> nanosecs
    iwrfPulse.setTime(sec, nanosec);
    
    // SysHousekeeping.gates
    iwrfPulse.set_n_gates(hskp.gates);
    // SysHousekeeping.chanId - We can handle only channel zero at this point!
    if (hskp.chanId != 0) {
        std::cerr << "TimeSeriesAdapter::" << __FUNCTION__ <<  " got channel" <<
                hskp.chanId << ", but can only handle channel 0!" << std::endl;
        abort();
    }
    // SysHousekeeping.radar_id 
    iwrfPulse.setRadarId(0);
    iwrfInfo.set_radar_name(hskp.radar_id.in());
    
    // SysHousekeeping.staggered_prt - @TODO only single-PRT handled at this point!
    if (hskp.staggered_prt) {
        std::cerr << "TimeSeriesAdapter::" << __FUNCTION__ << 
                " can currently only handle single-PRT data!" << std::endl;
        abort();
    } else {
        iwrfInfo.set_xmit_info_prf_mode(IWRF_PRF_MODE_FIXED);
    }
    
    // SysHousekeeping.pdpp - @TODO ...not sure how to handle this one
    
    // SysHousekeeping.prt1 - @TODO only single-PRT handled at this point!
    // SysHousekeeping.prt2
    iwrfPulse.set_prt(hskp.prt1);
    iwrfPulse.set_prt_next(hskp.prt1);
    iwrfInfo.set_xmit_info_prt_usec(1.0e6 * hskp.prt1);
    
    // SysHousekeeping.tx_peak_power - convert W -> dBm
    float pwr_dBm = 30.0 + 10.0 * log10(hskp.tx_peak_power);
    iwrfInfo.set_calib_xmit_power_dbm_h(pwr_dBm);
    
    // SysHousekeeping.tx_cntr_freq
    float wavelength_m = SPEED_OF_LIGHT / hskp.tx_cntr_freq;
    iwrfInfo.set_radar_wavelength_cm(100 * wavelength_m);
    
    // SysHousekeeping.tx_chirp_bandwidth - No match in IWRF metadata (?)
    
    // SysHousekeeping.tx_pulse_width
    iwrfPulse.set_pulse_width_us(1.0e6 * hskp.tx_pulse_width);
    iwrfInfo.set_calib_pulse_width_us(1.0e6 * hskp.tx_pulse_width);
    
    // SysHousekeeping.tx_switching_network_loss - no match in IWRF metadata

    // SysHousekeeping.tx_waveguide_loss - horizontal only!
    // Set the IWRF two-way waveguide loss here using the tx_waveguide_loss and
    // the rcvr_waveguide_loss.
    iwrfInfo.set_calib_two_way_waveguide_loss_db_h(hskp.tx_waveguide_loss +
            hskp.rcvr_waveguide_loss);
    
    // SysHousekeeping.tx_peak_pwr_coupling - no match in IWRF metadata (?)

    // SysHousekeeping.tx_turn_on_time - no match in IWRF metadata

    // SysHousekeeping.tx_turn_off_time - no match in IWRF metadata
    
    // SysHousekeeping.tx_upconverter_latency - no match in IWRF metadata

    // SysHousekeeping.ant_gain
    iwrfInfo.set_calib_gain_ant_db_h(hskp.ant_gain);

    // SysHousekeeping.ant_hbeam_width
    iwrfInfo.set_calib_beamwidth_deg_h(hskp.ant_hbeam_width);
    
    // SysHousekeeping.ant_vbeam_width
    iwrfInfo.set_calib_beamwidth_deg_v(hskp.ant_vbeam_width);

    // SysHousekeeping.ant_E_plane_angle - no match in IWRF metadata

    // SysHousekeeping.ant_H_plane_angle - no match in IWRF metadata

    // SysHousekeeping.ant_encoder_up - @todo: could maybe use IWRF antenna
    // az correction for this?
    
    // SysHousekeeping.ant_pitch_up - @todo: could maybe use IWRF antenna
    // el correction for this?

    // SysHousekeeping.az
    iwrfPulse.set_azimuth(hskp.az);
    
    // SysHousekeeping.el
    iwrfPulse.set_elevation(hskp.el);

    // SysHousekeeping.rotation_angle - KLUGE: We hijack IWRF antenna azimuth to 
    // store antenna rotation angle.
    iwrfPulse.set_azimuth(hskp.rotation_angle);

    // SysHousekeeping.actual_num_rcvrs
    iwrfPulse.set_n_channels(hskp.actual_num_rcvrs);

    // SysHousekeeping.rcvr_cntr_freq - no match in IWRF metadata
    
    // SysHousekeeping.rcvr_pulse_width
    // gate spacing = 0.5 * <speed of light> * rcvr_pulse_width
    iwrfPulse.set_gate_spacing_m(0.5 * SPEED_OF_LIGHT * hskp.rcvr_pulse_width);

    // SysHousekeeping.rcvr_switching_network_loss - no match in IWRF metadata

    // SysHousekeeping.rcvr_waveguide_loss - horizontal only!
    // This has been incorporated into the IWRF two-way waveguide loss. See
    // SysHousekeeping.tx_waveguide_loss above.

    // SysHousekeeping.rcvr_noise_figure - no match in IWRF metadata

    // SysHousekeeping.rcvr_filter_mismatch
    iwrfInfo.set_calib_receiver_mismatch_loss_db(hskp.rcvr_filter_mismatch);
    
    // SysHousekeeping.rcvr_rf_gain (NOTE: this includes IF gain)
    //
    // SysHousekeeping.rcvr_if_gain
    //
    // SysHousekeeping.rcvr_digital_gain
    //
    // IWRF only has total receiver gain, so we add up the gains.
    iwrfInfo.set_calib_receiver_gain_db_hc(hskp.rcvr_rf_gain + 
            hskp.rcvr_digital_gain);

    // SysHousekeeping.rcvr_gate0_delay
    // Translate receiver delay to range to start of gate0.
    double gate0Start = 0.5 * hskp.rcvr_gate0_delay * SPEED_OF_LIGHT; // m
    iwrfPulse.set_start_range_m(gate0Start);
    
    // SysHousekeeping.rcvr_bandwidth - no match in IWRF metadata

    // SysHousekeeping.latitude
    iwrfInfo.set_radar_latitude_deg(hskp.latitude);

    // SysHousekeeping.longitude
    iwrfInfo.set_radar_longitude_deg(hskp.longitude);

    // SysHousekeeping.altitude
    iwrfInfo.set_radar_altitude_m(hskp.altitude);

    // SysHousekeeping.supply_voltage_ok - no match in IWRF metadata

    // SysHousekeeping.temp_sensor_1 - no match in IWRF metadata

    // SysHousekeeping.temp_sensor_2 - no match in IWRF metadata

    // SysHousekeeping.temp_sensor_3 - no match in IWRF metadata

    // SysHousekeeping.pressure_sensor_1 - no match in IWRF metadata

    // SysHousekeeping.pressure_sensor_2 - no match in IWRF metadata

    //
    // Other metadata which do not come directly from RadarDDS::SysHousekeeping
    //
    iwrfPulse.set_pulse_seq_num(packetSequenceNum);
    iwrfPulse.set_scan_mode(IWRF_SCAN_MODE_POINTING);
    iwrfPulse.set_follow_mode(IWRF_FOLLOW_MODE_NOT_SET);
    iwrfPulse.set_volume_num(1);
    iwrfPulse.set_sweep_num(1);    // @TODO
    iwrfPulse.set_fixed_el(0.0);
    iwrfPulse.set_fixed_az(0.0);
    iwrfPulse.set_elevation(0.0);
    
    // Make certain that ddsPulse.data.get_buffer() contains 2-byte values, 
    // since we count on it when passing it to setIqPacked() below...
    assert(sizeof(ddsPulse.data[0]) == sizeof(si16));
    
    // Our IQ data are already scaled 16-bit signed ints, and in the same
    // order that IwrfTsPulse wants. So we stuff them directly into iwrfPulse.
    
    // The scale factor used by RadarDDS::TimeSeries is:
    //     I      = I       * iqScale + iqOffset
    //      volts    counts
    // Same formula for Q.
    float iqScale = 1.0 / (32768 * sqrt(2.0));
    float iqOffset = 0.0;
    iwrfPulse.setIqPacked(ddsPulse.hskp.gates, 1, IWRF_IQ_ENCODING_SCALED_SI16,
            ddsPulse.data.get_buffer(), iqScale, iqOffset);

}
