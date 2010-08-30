/*
 * ProductGenerator.cpp
 *
 *  Created on: Oct 14, 2009
 *      Author: burghart
 */

#include "ProductGenerator.h"

#include <iostream>
#include <cstdlib>
#include <ctime>

const int ProductGenerator::PRODGEN_MAX_GATES = 4096;

ProductGenerator::ProductGenerator(QtKaTSReader *source, KaProductWriter *sink,
        int nSamples) :
    _reader(source),
    _writer(sink),
    _momentsCalc(PRODGEN_MAX_GATES, false, false),
    _nSamples(nSamples),
    _fft(nSamples),
    _regFilter(),
    _samplesCached(0),
    _itemCount(0),
    _wrongChannelCount(0),
    _dwellCount(0),
    _dwellDiscardCount(0),
    _lastPulseRcvd(-1) {
    // Set the number of samples per dwell
    _momentsCalc.setNSamples(int(_nSamples));
    // Set the number of samples for the regression filter
    _regFilter.setup(_nSamples);
    //
    // Allocate our dwell-in-progress: _dwellIQ[PRODGEN_MAX_GATES][_nSamples]
    //
    _dwellIQ = new RadarComplex_t*[PRODGEN_MAX_GATES];
    for (int g = 0; g < PRODGEN_MAX_GATES; g++) {
        _dwellIQ[g] = new RadarComplex_t[_nSamples];
    }
    // Work space to hold a dwell of filtered IQ data for one gate
    _filteredGateIQ = new RadarComplex_t[_nSamples];
}

ProductGenerator::~ProductGenerator() {
    for (int gate = 0; gate < PRODGEN_MAX_GATES; gate++) {
        delete _dwellIQ[gate];
    }
    delete _dwellIQ;
    delete _filteredGateIQ;
}

void
ProductGenerator::run() {
    // Set an ongoing timer so that we print info on a regular basis.
    QTimer timer(this);
    connect(&timer, SIGNAL(timeout()), this, SLOT(showInfo()));
    timer.start(5000);  // every 5 seconds
    // Accept data via newItem() signals from our source, and free the
    // data pointers by sending returnItem() signals back.
    connect(_reader, SIGNAL(newItem(RadarDDS::TimeSeriesSequence*)),
            this, SLOT(handleItem(RadarDDS::TimeSeriesSequence*)));
    connect(this, SIGNAL(returnItem(RadarDDS::TimeSeriesSequence*)),
            _reader, SLOT(returnItemSlot(RadarDDS::TimeSeriesSequence*)));
    exec();
}

void
ProductGenerator::showInfo() {
    std::cerr << _itemCount << " samples rcvd, " <<
    	_ddsDrops << " samples missed, " <<
        _wrongChannelCount << " wrong channel, " <<
        _dwellCount << " rays generated and " <<
        _dwellDiscardCount << " could not be pub'd, " << std::endl;
    _itemCount = 0;
    _ddsDrops = 0;
    _wrongChannelCount = 0;
    _dwellCount = 0;
    _dwellDiscardCount = 0;
}

void
ProductGenerator::handleItem(RadarDDS::KaTimeSeriesSequence* tsSequence) {
    bool ok = true;

    _itemCount++;
    
    unsigned int nPulses = tsSequence->tsList.length();

    // We currently generate products only for channel zero
    if (tsSequence->chanId != 0) {
        _wrongChannelCount++;
        goto done;
    }

    // Run through the samples in the item
    for (unsigned int samp = 0; samp < nPulses; samp++) {
        RadarDDS::KaTimeSeries &ts = tsSequence->tsList[samp];
        // Get the housekeeping as a class which gives us access to derived
        // values (like radar constant, gate spacing, etc.) in addition to the 
        // raw metadata members.
        kaddsSysHskp hskp(ts.hskp);
        
        // Check pulse number
        long pulseNum = ts.pulseNum;
        long nMissed = pulseNum - _lastPulseRcvd - 1;
        if (_lastPulseRcvd > 0 && nMissed) {
        	// If it's the first pulse of a sequence and the number of
        	// dropped pulses is a multiple of the pulse collection size,
        	// we dropped one or more whole DDS packets.
        	if (samp == 0 && !(nMissed % nPulses)) {
        		_ddsDrops += (nMissed / nPulses);
        	}
        	
        	// Tell about missed pulses, and force starting a new dwell.
        	//std::cerr << __FUNCTION__ << ": after pulse " << _lastPulseRcvd <<
			//	": " << nMissed << " pulses missed";
            if (_samplesCached) {
                 //std::cerr << " in mid-dwell; dwell-in-progress abandoned." <<
					// std::endl;
                _samplesCached = 0;
            } else {
            	//std::cerr << "." << std::endl;
            }
        }
        _lastPulseRcvd = pulseNum;
        
        /*
         * If PRF, gate spacing, range to first gate, or gate count change in
         * mid-dwell, abandon the dwell-in-progress and start a new one.
         */
        if (_samplesCached > 0) {
            bool hskpChanged = false;

            if (_dwellHskp.prt1 != hskp.prt1) {
                std::cerr << "Mid-dwell PRT change from " << 
                        _dwellHskp.prt1 << " to " << hskp.prt1 <<
                        " @ " << hskp.timetag << std::endl;
                hskpChanged = true;
            }
            if (_dwellHskp.gate_spacing() != hskp.gate_spacing()) {
                std::cerr << "Mid-dwell gate spacing change from " << 
                        _dwellHskp.gate_spacing() << " to " << 
                        hskp.gate_spacing() << " @ " << hskp.timetag << std::endl;
                hskpChanged = true;
            }
            if (_dwellHskp.range_to_first_gate() != hskp.range_to_first_gate()) {
                std::cerr << "Mid-dwell range-to-first-gate change from " <<
                        _dwellHskp.range_to_first_gate() << " to " << 
                        hskp.range_to_first_gate() << " @ " << hskp.timetag << 
                        std::endl;
                hskpChanged = true;
            }
            if (_dwellHskp.gates != hskp.gates) {
                std::cerr << "Mid-dwell ngates change from " << 
                        _dwellHskp.gates << " to " << hskp.gates <<
                        " @ " << hskp.timetag << std::endl;
                hskpChanged = true;
            }
            if (hskpChanged) {
                _samplesCached = 0;
                std::cerr << "Dwell-in-progress abandoned" << std::endl;
            }
        }
        
        /*
         * At the start of a dwell, initialize the moments calculator and
         * save the housekeeping which should remain unchanged through the
         * dwell.
         */
        if (_samplesCached == 0) {
            // Calibrate using values from the first pulse of the dwell
            DsRadarCalib calib;
            calib.setReceiverGainDbHc(hskp.rcvr_gain());
            calib.setNoiseDbmHc(hskp.rcvr_noise_power() + 
                    hskp.rcvr_gain()); // we include receiver gain here
            calib.setBaseDbz1kmHc(hskp.rcvr_noise_power() + hskp.radar_constant_water());
            _momentsCalc.setCalib(calib);
            
            _momentsCalc.init(hskp.prt1, hskp.tx_wavelength(), 
                    hskp.range_to_first_gate() * 0.001 /* km */,
                    hskp.gate_spacing() * 0.001 /* km */);
            
            _dwellHskp = hskp;
        }
        /*
         * Save the azimuth and elevation from mid-dwell
         */
        if (_samplesCached == (_nSamples / 2)) {
            _dwellAz = hskp.az;
            _dwellEl = hskp.el;
        }

        // Gate count sanity check
        int nGates = hskp.gates;
        
        if (nGates > PRODGEN_MAX_GATES) {
            std::cerr << "ProductGenerator::handleItem: Got " << nGates <<
                " gates; PRODGEN_MAX_GATES is " << PRODGEN_MAX_GATES << std::endl;
            ok = false;
        }
        
        // Put the Is and Qs for this sample into the dwell-in-progress.
        double sqrtTwo = sqrt(2.0);
        double vMax = 1.0;	// Max signal voltage for Ka.  @todo make this changeable!

        for (int gate = 0; gate < nGates; gate++) {
            // Real part I, in volts = (I      /32768) * vMax / sqrt(2)
            //                           counts
            _dwellIQ[gate][_samplesCached].re =
                vMax * (ts.data[2 * gate] / 32768.0) / sqrtTwo;
            // Imaginary part Q, in volts = (Q      /32768) * vMax / sqrt(2)
            //                                counts
            _dwellIQ[gate][_samplesCached].im =
                vMax * (ts.data[2 * gate + 1] / 32768.0) / sqrtTwo;
        }
        _samplesCached++;
        /*
         * If this sample created a complete dwell, publish it
         */
        if (_samplesCached == _nSamples) {
            _dwellCount++;
            MomentsFields moments[nGates];
            MomentsFields filteredMoments[nGates];
            // Calculate products for our dwell, gate by gate
            for (int gate = 0; gate < nGates; gate++) {
                // First calculate moments from the unfiltered IQ data
                _momentsCalc.singlePol(_dwellIQ[gate], gate, false, moments[gate]);
//                // Filter IQ data for this gate
//                double filterRatio;
//                double spectralNoise;
//                double spectralSnr;
//                _momentsCalc.applyRegressionFilter(_nSamples, _fft, _regFilter,
//                    _dwellIQ[gate], _rcvrNoise, true, _filteredGateIQ,
//                    filterRatio, spectralNoise, spectralSnr);
//                // Calculate moments from the filtered IQ
//                _momentsCalc.singlePol(_filteredGateIQ, gate, true,
//                    filteredMoments[gate]);
            }
            // Publish the dwell
            publish_(moments, filteredMoments);
            // Start a new dwell-in-progress
            _samplesCached = 0;
        }
    }

done:
    // Tell our source we're done with this item
    emit returnItem(tsSequence);

    if (ok)
        return;
    else
        exit(1);
}

void
ProductGenerator::publish_(const MomentsFields *moments,
        const MomentsFields *filteredMoments) {
    RadarDDS::KaProductSet *productSet;
    int nGates = _dwellHskp.gates;

    productSet = _writer->getEmptyItem();
    if (! productSet) {
        _dwellDiscardCount++;
        return;
    }

    productSet->radarId = 0;
    // The number of products is currently fixed at 8: DM, DMRAW, DZ, VE,
    // SW, SNR, DZ_F, VE_F
    productSet->products.length(8);
    RadarDDS::KaProduct *product = productSet->products.get_buffer();

    // RAP moments "dbm" is really:
    //
    //                        ngates
    //                        ----
    //                         \   2    2
    //  10 * log10( 1/ngates *  | I  + Q  )
    //                         /   g    g
    //                        ----
    //                        g = 1
    //
    // where I and Q are values in volts. To convert from dB(volts^2) to 
    // dBm [i.e., dB(mW)], we need to account for the input impedance to get 
    // to db(W), and then convert from dB(W) to dB(mW).
    double rcvrInputImpedance = 50.0;
    double dbmCorr = -10.0 * log10(rcvrInputImpedance);
    dbmCorr += 30.0; // dB(W) -> dB(mW)

    // DM: coherent power
    addProductHousekeeping_(*product);
    product->name = "DM";
    product->description = "coherent power";
    product->units = "dBm";
    product->offset = -50.0;
    product->scale = 100.0 / 32768;
    product->bad_value = PRODUCT_BAD_VALUE;
    product->data.length(nGates);
    for (int g = 0; g < nGates; g++) {
        // Catch and flag bad values now
        if (moments[g].dbm == MomentsFields::missingDouble) {
            product->data[g] = PRODUCT_BAD_VALUE;
            continue;
        }
    	double dbm = moments[g].dbm + dbmCorr;
        product->data[g] =
            short((dbm - product->offset) / product->scale);
    }
    // DMRAW: coherent power (including receiver gain)
    product++;
    addProductHousekeeping_(*product);
    product->name = "DMRAW";
    product->description = "coherent power (including rcvr gain)";
    product->units = "dBm";
    product->offset = 0.0;
    product->scale = 100.0 / 32768;
    product->bad_value = PRODUCT_BAD_VALUE;
    product->data.length(nGates);
    for (int g = 0; g < nGates; g++) {
        // Catch and flag bad values now
        if (moments[g].dbm == MomentsFields::missingDouble) {
            product->data[g] = PRODUCT_BAD_VALUE;
            continue;
        }
    	// add back in the RF receiver gain
    	double rawDbm = moments[g].dbm + dbmCorr + _dwellHskp.rcvr_rf_gain;
        product->data[g] =
            short((rawDbm - product->offset) / product->scale);
    }
    // DZ: reflectivity
    product++;
    addProductHousekeeping_(*product);
    product->name = "DZ";
    product->description = "reflectivity";
    product->units = "dBZ";
    product->offset = 0.0;
    product->scale = 100.0 / 32768;
    product->bad_value = PRODUCT_BAD_VALUE;
    product->data.length(nGates);
    for (int g = 0; g < nGates; g++) {
        // Catch and flag bad values now
        if (moments[g].dbz == MomentsFields::missingDouble) {
            product->data[g] = PRODUCT_BAD_VALUE;
            continue;
        }
        product->data[g] =
            short((moments[g].dbz - product->offset) / product->scale);
    }
    // VE: radial velocity
    product++;
    addProductHousekeeping_(*product);
    product->name = "VE";
    product->description = "radial velocity";
    product->units = "m s-1";
    product->offset = 0.0;
    product->scale = 100.0 / 32768;
    product->bad_value = PRODUCT_BAD_VALUE;
    product->data.length(nGates);
    for (int g = 0; g < nGates; g++) {
        // Catch and flag bad values now
        if (moments[g].vel == MomentsFields::missingDouble) {
            product->data[g] = PRODUCT_BAD_VALUE;
            continue;
        }
        product->data[g] =
            short((moments[g].vel - product->offset) / product->scale);
    }
    // SW: spectrum width
    product++;
    addProductHousekeeping_(*product);
    product->name = "SW";
    product->description = "spectrum width";
    product->units = "m s-1";
    product->offset = 0.0;
    product->scale = 50.0 / 32768;
    product->bad_value = PRODUCT_BAD_VALUE;
    product->data.length(nGates);
    for (int g = 0; g < nGates; g++) {
        // Catch and flag bad values now
        if (moments[g].width == MomentsFields::missingDouble) {
            product->data[g] = PRODUCT_BAD_VALUE;
            continue;
        }
        product->data[g] =
            short((moments[g].width - product->offset) / product->scale);
    }
    // SNR: signal-to-noise ratio
    product++;
    addProductHousekeeping_(*product);
    product->name = "SNR";
    product->description = "signal-to-noise ratio";
    product->units = "dB";
    product->offset = 0.0;
    product->scale = 100.0 / 32768;
    product->bad_value = PRODUCT_BAD_VALUE;
    product->data.length(nGates);
    for (int g = 0; g < nGates; g++) {
        // Catch and flag bad values now
        if (moments[g].snr == MomentsFields::missingDouble) {
            product->data[g] = PRODUCT_BAD_VALUE;
            continue;
        }
        product->data[g] =
            short((moments[g].snr - product->offset) / product->scale);
    }
    // DZ_F: filtered reflectivity
    product++;
    addProductHousekeeping_(*product);
    product->name = "DZ_F";
    product->description = "filtered reflectivity";
    product->units = "dBZ";
    product->offset = 0.0;
    product->scale = 100.0 / 32768;
    product->bad_value = PRODUCT_BAD_VALUE;
    product->data.length(nGates);
    for (int g = 0; g < nGates; g++) {
        // Catch and flag bad values now
        if (moments[g].dbz == MomentsFields::missingDouble) {
            product->data[g] = PRODUCT_BAD_VALUE;
            continue;
        }
        product->data[g] =
            short((filteredMoments[g].dbz - product->offset) / product->scale);
    }
    // VE_F: filtered radial velocity
    product++;
    addProductHousekeeping_(*product);
    product->name = "VE_F";
    product->description = "filtered radial velocity";
    product->units = "m s-1";
    product->offset = 0.0;
    product->scale = 100.0 / 32768;
    product->bad_value = PRODUCT_BAD_VALUE;
    product->data.length(nGates);
    for (int g = 0; g < nGates; g++) {
        // Catch and flag bad values now
        if (moments[g].vel == MomentsFields::missingDouble) {
            product->data[g] = PRODUCT_BAD_VALUE;
            continue;
        }
        product->data[g] =
            short((filteredMoments[g].vel - product->offset) / product->scale);
    }

    // publish it
   _writer->publishItem(productSet);
}

void
ProductGenerator::addProductHousekeeping_(RadarDDS::KaProduct & p) {
    // Copy housekeeping from the first pulse of the dwell
    p.hskp = _dwellHskp;
    // replace az and el with values from mid-dwell
    p.hskp.az = _dwellAz;   // saved from mid-dwell housekeeping
    p.hskp.el = _dwellEl;   // saved from mid-dwell housekeeping
    // Insert the sample count
    p.samples = _nSamples;
}
