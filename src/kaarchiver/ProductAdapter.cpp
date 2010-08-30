/*
 * ProductAdapter.cpp
 *
 *  Created on: Jun 7, 2010
 *      Author: burghart
 */
#include <iostream>
#include <iomanip>

#include "ProductAdapter.h"

#include <kaddsSysHskp.h>

#include <Radx/RadxGeoref.hh>

static const double SPEED_OF_LIGHT = 2.99792458e8; // m s-1


void ProductAdapter::RadxRayToDDS(const RadxRay& radxRay, const RadxVol& radxVol,
            RadarDDS::KaProductSet& productSet) {
    long long timetagUsecs = radxRay.getTimeSecs() * 1000000LL +
            (long long)(radxRay.getNanoSecs() / 1000);   // usecs since 1970-01-01 00:00:00 UTC

    // Get the RadxRcalib for this ray
    int calibIndex = radxRay.getCalibIndex();
    const RadxRcalib & calib = *(radxVol.getCalibs()[calibIndex]);
    
    // Build the RadarDDS::ProductSet product by product
    int nProducts = radxRay.getNFields();
    
    productSet.products.length(nProducts);
    RadarDDS::KaProduct *productList = productSet.products.get_buffer();
    
    for (int p = 0; p < nProducts; p++) {
        // Get the next RadxField
        const RadxField & radxField = *(radxRay.getField(p));
        
        // Get the available georeference, if any, or create an empty one
        const RadxGeoref* georef = radxRay.getGeoreference();
        
        // We can only handle SI16 (scaled 16-bit integer) data
        if (radxField.getDataType() != Radx::SI16) {
            std::cerr << __FUNCTION__ << ": at data time " << 
                std::setprecision(6) << 1.0e-6 * timetagUsecs << 
                ", field " << radxField.getName() << " data are not SI16!" << 
                std::endl;
            exit(1);
        }
        
        // Get references to the destination Product and its SysHousekeeping
        RadarDDS::KaProduct & product = productList[p];
        RadarDDS::SysHousekeeping & hskp = product.hskp;
        
        // SysHousekeeping.timetag
        hskp.timetag = timetagUsecs;
        
        // SysHousekeeping.gates
        int nGates = radxField.getNGates();
        hskp.gates = nGates;
        
        // SysHousekeeping.chanId: no match in Radx metadata
        
        // SysHouseekeping.radar_id
        hskp.radar_id = radxVol.getInstrumentName().c_str();
        
        // SysHousekeeping.staggered_prt: single PRT mode only for now!
        if (radxRay.getPrfMode() != Radx::PRF_MODE_FIXED) {
            std::cerr << __FUNCTION__ << ": at data time " << 
                std::setprecision(6) << 1.0e-6 * timetagUsecs << 
                ", PRF mode is not fixed!" << std::endl;
            exit(1);
        } else {
            hskp.staggered_prt = false;
        }
        
        // SysHousekeeping.pdpp: @TODO ...not sure how to handle this one

        // SysHousekeeping.prt1
        hskp.prt1 = radxRay.getPrtSec();
        
        // SysHousekeeping.prt2
        hskp.prt2 = radxRay.getPrt2Sec();
        
        // SysHousekeeping.tx_peak_power: horizontal only for now!
        hskp.tx_peak_power = calib.getXmitPowerDbmH();
        
        // SysHousekeeping.tx_cntr_freq
        hskp.tx_cntr_freq = radxVol.getWavelengthMeters() / SPEED_OF_LIGHT;
        
        // SysHousekeeping.tx_chirp_bandwidth: no match in Radx metadata
        
        // SysHousekeeping.tx_pulse_width
        hskp.tx_pulse_width = calib.getPulseWidthUsec() * 1.0e-6; // s
        
        // SysHousekeeping.tx_switching_network_loss: no match in Radx metadata
        
        // SysHousekeeping.tx_waveguide_loss: horizontal only!
        // NOTE: We halve the two-way waveguide loss to get the tx-side
        // loss, making the (dangerous) assumption that the waveguide rx & tx 
        // losses are the same...
        hskp.tx_waveguide_loss = calib.getTwoWayWaveguideLossDbH() / 2;
        
        // SysHousekeeping.tx_peak_pwr_coupling: no match in Radx metadata
        
        // SysHousekeeping.tx_turn_on_time: no match in Radx metadata
        
        // SysHousekeeping.tx_turn_off_time: no match in Radx metadata
        
        // SysHousekeeping.tx_upconverter_latency: no match in Radx metadata
        
        // SysHousekeeping.ant_gain
        hskp.ant_gain = radxVol.getRadarAntennaGainDbH(); // H only!
        
        // SysHousekeeping.ant_hbeam_width
        hskp.ant_hbeam_width = radxVol.getRadarBeamWidthDegH();
        
        // SysHousekeeping.ant_vbeam_width
        hskp.ant_vbeam_width = radxVol.getRadarBeamWidthDegV();
        
        // SysHousekeeping.ant_E_plane_angle: no match in Radx metadata
        
        // SysHousekeeping.ant_H_plane_angle: no match in Radx metadata
        
        // SysHousekeeping.ant_encoder_up: @TODO this may have a match in RadxCfactors...
        
        // SysHousekeeping.ant_pitch_up
        hskp.ant_pitch_up = georef ? georef->getPitchChange() : 0.0;
        
        // SysHousekeeping.az
        hskp.az = radxRay.getAzimuthDeg();
        
        // SysHousekeeping.el
        hskp.el = radxRay.getElevationDeg();
        
        // SysHousekeeping.rotation_angle
        hskp.rotation_angle = georef ? georef->getRotation() : 0.0;
        
        // SysHousekeeping.actual_num_rcvrs: no match in Radx metadata
        
        // SysHousekeeping.rcvr_cntr_freq: no match in Radx metadata
        
        // SysHousekeeping.rcvr_pulse_width
        double gateSpacing = radxRay.getGateSpacingKm() * 1000.0; // m
        hskp.rcvr_pulse_width = (2.0 * gateSpacing) / SPEED_OF_LIGHT; // s
        
        // SysHousekeeping.rcvr_switching_network_loss = @TODO;
        
        // SysHousekeeping.rcvr_waveguide_loss
        // NOTE: We halve the two-way waveguide loss to get the rx-side
        // loss, making the (dangerous) assumption that the waveguide rx & tx 
        // losses are the same...
        hskp.rcvr_waveguide_loss = calib.getTwoWayWaveguideLossDbH() / 2;
        
        
        // SysHousekeeping.rcvr_noise_figure: no match in Radx metadata
        
        // SysHousekeeping.rcvr_filter_mismatch
        hskp.rcvr_filter_mismatch = calib.getReceiverMismatchLossDb();
        
        // SysHousekeeping.rcvr_rf_gain
        // NOTE: Since Radx only has total receiver gain, put it all into 
        // RF gain, and set IF gain and digital gain to zero.
        hskp.rcvr_rf_gain = calib.getReceiverGainDbHc();
        
        // SysHousekeeping.rcvr_if_gain: see rcvr_rf_gain above!
        hskp.rcvr_if_gain = 0.0;
        
        // SysHousekeeping.rcvr_digital_gain: see rcvr_rf_gain above!
        hskp.rcvr_digital_gain = 0.0;
        
        // SysHousekeeping.rcvr_gate0_delay
        // Translate range to center of gate0 to receiver delay
        double gate0Start = 1.0e3 * radxRay.getStartRangeKm() - 0.5 * gateSpacing; // m
        hskp.rcvr_gate0_delay = (2.0 * gate0Start) / SPEED_OF_LIGHT;
        
        // SysHousekeeping.rcvr_bandwidth
        hskp.rcvr_bandwidth = radxVol.getRadarReceiverBandwidthMhz() * 1.0e6; // Hz
        
        // SysHousekeeping.latitude
        hskp.latitude = georef ? georef->getLatitude() : radxVol.getLatitudeDeg();
        
        // SysHousekeeping.longitude
        hskp.longitude = georef ? georef->getLongitude() : radxVol.getLongitudeDeg();
        
        // SysHousekeeping.altitude
        hskp.altitude = georef ? georef->getAltitudeMsl() : radxVol.getAltitudeKm() * 1.0e3;
        
        // SysHousekeeping.supply_voltage_ok:  no match in Radx metadata
        
        // SysHousekeeping.temp_sensor_1:  no match in Radx metadata
        
        // SysHousekeeping.temp_sensor_2:  no match in Radx metadata
        
        // SysHousekeeping.temp_sensor_3:  no match in Radx metadata
        
        // SysHousekeeping.pressure_sensor_1:  // no match in Radx metadata
        
        // SysHousekeeping.pressure_sensor_2:  // no match in Radx metadata
        
        product.name = radxField.getName().c_str();
        product.description = radxField.getLongName().c_str();
        product.units = radxField.getUnits().c_str();
        product.offset = radxField.getOffset();
        product.scale = radxField.getScale();
        product.bad_value = radxField.getMissingSi16();
        product.samples = radxRay.getNSamples();
        
        // Copy the 16-bit data from radxField into this RadarDDS::Product
        product.data.allocbuf(nGates);
        memcpy(product.data.get_buffer(), radxField.getDataSi16(), 2 * nGates);
    }
}

void ProductAdapter::DDSToRadxRay(const RadarDDS::KaProductSet& productSet, 
        RadxRay& radxRay, RadxVol& radxVol, RadxRcalib& radxRcalib) {
    // Will this be the first ray added to RadxVol?
    bool firstRayInVol = (radxVol.getNRays() == 0);
    
    // Get common housekeeping from the first product in the ProductSet
    const RadarDDS::KaProduct &firstProduct = productSet.products[0];
    const kaddsSysHskp hskp(firstProduct.hskp);
    
    // If the platform type is not Radx::PLATFORM_TYPE_FIXED, we need to
    // associate a RadxGeoref with the RadxRay. The georef is populated below.
    RadxGeoref* georef = 0;
    if (radxVol.getPlatformType() != Radx::PLATFORM_TYPE_FIXED) {
        georef = new RadxGeoref();
        radxRay.addGeoref(*georef);
    }
    
    // Copy the metadata from the RadarDDS::ProductSet's housekeeping into
    // the matching Radx metadata. We follow the ordering from the
    // RadarDDS::SysHousekeeping struct to make it easier to sync changes
    // with the RadxRayToDDS() method above.
    
    // SysHousekeeping.timetag
    long long timetagUsecs = hskp.timetag;
    time_t rayTimeSecs = timetagUsecs / 1000000;
    int rayTimeNanos = (timetagUsecs % 1000000) * 1000;
    radxRay.setTime(rayTimeSecs, rayTimeNanos);
    
    // SysHousekeeping.gates
    int nGates = hskp.gates;
    
    // SysHousekeeping.chanId: no match in Radx metadata
    
    // SysHousekeeping.radar_id
    if (firstRayInVol)
        radxVol.setInstrumentName(hskp.radar_id.in());
    
    // SysHousekeeping.staggered_prt: fixed PRT only (for now)
    if (hskp.staggered_prt) {
        std::cerr << __FUNCTION__ << 
                ": cannot handle DDS data with staggered PRT!" << std::endl;
        exit(1);
    }
    radxRay.setPrfMode(Radx::PRF_MODE_FIXED);
    
    // SysHousekeeping.pdpp: @TODO ...not sure how to handle this one
    
    // SysHousekeeping.prt1
    // SysHousekeeping.prt2
    radxRay.setPrtSec(hskp.prt1);
    radxRay.setPrt2Sec(hskp.prt2);
    radxRay.setUnambigRange(); // calculate unambiguous range from PRT
    
    // SysHousekeeping.tx_peak_power: horizontal only for now!
    radxRcalib.setXmitPowerDbmH(hskp.tx_peak_power);
    
    // SysHousekeeping.tx_cntr_freq
    if (firstRayInVol)
        radxVol.setWavelengthMeters(hskp.tx_wavelength());
    
    // SysHousekeeping.tx_chirp_bandwidth: no match in Radx metadata
    
    // SysHousekeeping.tx_pulse_width
    radxRcalib.setPulseWidthUsec(1.0e6 * hskp.tx_pulse_width);

    // SysHousekeeping.tx_switching_network_loss: no match in Radx metadata
    
    // SysHousekeeping.tx_waveguide_loss: horizontal only!
    // We add the tx_waveguide_loss and rcvr_waveguide_loss to get two-way loss.
    radxRcalib.setTwoWayWaveguideLossDbH(hskp.tx_waveguide_loss + 
            hskp.rcvr_waveguide_loss);
    
    // SysHousekeeping.tx_peak_pwr_coupling: no match in Radx metadata
    
    // SysHousekeeping.tx_turn_on_time: no match in Radx metadata
    
    // SysHousekeeping.tx_turn_off_time: no match in Radx metadata
    
    // SysHousekeeping.tx_upconverter_latency: no match in Radx metadata
    
    // SysHousekeeping.ant_gain: horizontal polarization only for now!
    if (firstRayInVol)
        radxVol.setRadarAntennaGainDbH(hskp.ant_gain);
    
    // SysHousekeeping.ant_hbeam_width
    if (firstRayInVol)
        radxVol.setRadarBeamWidthDegH(hskp.ant_hbeam_width);
    
    // SysHousekeeping.ant_vbeam_width
    if (firstRayInVol)
        radxVol.setRadarBeamWidthDegV(hskp.ant_vbeam_width);

    // SysHousekeeping.ant_E_plane_angle: no match in Radx metadata
    
    // SysHousekeeping.ant_H_plane_angle: no match in Radx metadata
    
    // SysHousekeeping.ant_encoder_up: @TODO this may have a match in RadxCfactors...
    
    // SysHousekeeping.ant_pitch_up
    if (georef)
        georef->setPitchChange(hskp.ant_pitch_up);
    
    // SysHousekeeping.az
    radxRay.setAzimuthDeg(hskp.az);
    
    // SysHousekeeping.el
    radxRay.setElevationDeg(hskp.el);
    
    // SysHousekeeping.rotation_angle
    if (georef)
        georef->setRotation(hskp.rotation_angle);
    
    // SysHousekeeping.actual_num_rcvrs: no match in Radx metadata
    
    // SysHousekeeping.rcvr_cntr_freq: no match in Radx metadata
    
    // SysHousekeeping.rcvr_pulse_width: calculate gate spacing,
    // which is used below in setting gate ranges in the RadxRay.
    double gateSpacing = 0.5 * SPEED_OF_LIGHT * hskp.rcvr_pulse_width; // m
    
    // SysHousekeeping.rcvr_switching_network_loss = @TODO;
    
    // SysHousekeeping.rcvr_waveguide_loss
    // We add the tx_waveguide_loss and rcvr_waveguide_loss to get two-way loss.
    // See tx_waveguide_loss above.
    
    // SysHousekeeping.rcvr_noise_figure: no match in Radx metadata
    
    // SysHousekeeping.rcvr_filter_mismatch
    radxRcalib.setReceiverMismatchLossDb(hskp.rcvr_filter_mismatch);

    // SysHousekeeping.rcvr_rf_gain
    // NOTE: Radx only has total receiver gain, which is set to (hskp.rcvr_rf_gain + 
    // hskp.rcvr_digital_gain)
    radxRcalib.setReceiverGainDbHc(hskp.rcvr_rf_gain + hskp.rcvr_digital_gain);

    // SysHousekeeping.rcvr_if_gain: see rcvr_rf_gain above!

    // SysHousekeeping.rcvr_digital_gain: see rcvr_rf_gain above!
    
    // SysHousekeeping.rcvr_gate0_delay
    // Translate receiver delay to range to start of gate0, then set the
    // fixed gate ranges for the RadxRay. Note that RadxRay.setConstantGeom()
    // takes its ranges in km, and expects the start value to be to the *center*
    // of the first gate.
    double gate0Start = 0.5 * hskp.rcvr_gate0_delay * SPEED_OF_LIGHT; // m
    radxRay.setConstantGeom(nGates, 1.0e-3 * (gate0Start + 0.5 * gateSpacing), 
            1.0e-3 * gateSpacing); // ranges in km!

    // SysHousekeeping.rcvr_bandwidth
    if (firstRayInVol)
        radxVol.setRadarReceiverBandwidthMhz(1.0e-6 * hskp.rcvr_bandwidth);
    
    // SysHousekeeping.latitude
    // SysHousekeeping.longitude
    // SysHousekeeping.altitude
    if (georef) {
        georef->setLatitude(hskp.latitude);
        georef->setLongitude(hskp.longitude);
        georef->setAltitudeMsl(hskp.altitude);
    }
    
    if (firstRayInVol) {
        radxVol.setLatitudeDeg(hskp.latitude);
        radxVol.setLongitudeDeg(hskp.longitude);
        radxVol.setAltitudeKm(1.0e-3 * hskp.altitude);
    }

    // SysHousekeeping.supply_voltage_ok: no match in Radx metadata
    
    // SysHousekeeping.temp_sensor_1: no match in Radx metadata
    
    // SysHousekeeping.temp_sensor_2: no match in Radx metadata
    
    // SysHousekeeping.temp_sensor_3: no match in Radx metadata
    
    // SysHousekeeping.pressure_sensor_1: no match in Radx metadata
    
    // SysHousekeeping.pressure_sensor_2: no match in Radx metadata

    // SysHousekeeping.tx_pulse_width
    radxRay.setPulseWidthUsec(1.0e6 * hskp.tx_pulse_width);
    
    // Set a couple of things which come from SysHousekeeping derived
    // value methods.
    radxRcalib.setBaseDbz1kmHc(hskp.rcvr_noise_power() + hskp.radar_constant_water());
    radxRcalib.setNoiseDbmHc(hskp.rcvr_noise_power());
    
    // Set values we can't get from the SysHousekeeping
    radxRay.setVolumeNumber(radxVol.getVolumeNumber());
    radxRay.setSweepNumber(0);
    radxRay.setCalibIndex(0);
    radxRay.setSweepMode(Radx::SWEEP_MODE_POINTING);   // HCR is pointing only for now
    radxRay.setPolarizationMode(Radx::POL_MODE_HORIZONTAL);
    radxRay.setFollowMode(Radx::FOLLOW_MODE_NONE);
    radxRay.setFixedAngleDeg(0.0); // @TODO
    radxRay.setIsIndexed(false);
    radxRay.setAntennaTransition(false);
    radxRay.setNSamples(firstProduct.samples);
    
    float wavelength = radxVol.getWavelengthMeters();
    float prf = 1.0 / radxRay.getPrtSec();  // Hz
    float nyquist = (prf * wavelength) / 4;     // Nyquist max speed
    radxRay.setNyquistMps(nyquist);
    
    // Add each product in productSet as a new field in radxRay
    int nProducts = productSet.products.length();
    for (int p = 0; p < nProducts; p++) {
        const RadarDDS::KaProduct& product = productSet.products[p];
        radxRay.addField(std::string(product.name), 
                std::string(product.units), product.data.length(), 
                (Radx::si16)product.bad_value, 
                (Radx::si16*)product.data.get_buffer(),
                product.scale, product.offset, true);
        // Long name has to be set separately
        radxRay.getField(p)->setLongName(std::string(product.description));
    }
}
