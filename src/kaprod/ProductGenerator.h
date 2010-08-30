/*
 * ProductGenerator.h
 *
 *  Created on: Oct 14, 2009
 *      Author: burghart
 */

#ifndef PRODUCTGENERATOR_H_
#define PRODUCTGENERATOR_H_

#include <vector>
#include <QThread>
#include <QtKaTSReader.h>
#include <KaProductWriter.h>
#include <radar/RadarMoments.hh>
#include <kaddsSysHskp.h>

class ProductGenerator : public QThread {
    
    Q_OBJECT
    
public:
	/**
	 * Constructor.
	 * @param source the QtTSReader source for time series data
	 * @param sink the ProductWriter sink for resulting products
	 * int nSamples the number of pulses to use per dwell
	 */
    ProductGenerator(QtKaTSReader *source, KaProductWriter *sink, int nSamples);
    virtual ~ProductGenerator();
    void run();
    /**
     * Maximum number of gates which can be handled
     */
    static const int PRODGEN_MAX_GATES;
    
    /**
     * Bad value flag
     */
    static const short PRODUCT_BAD_VALUE = -32768;
    
protected slots:
    /**
     * Handle one TimeSeries item from our source.  If a complete ray of
     * products becomes available after handling, it will be published.
     * @param item pointer to the new RadarDDS::TimeSeries item
     */
    virtual void handleItem(RadarDDS::KaTimeSeriesSequence *item);
    /**
     * Show information.
     */
    void showInfo();
    
signals:
    /**
     * This signal is emitted when we are finished with an item received
     * from our source.
     * @param item pointer to the RadarDDS::TimeSeries item being returned
     */
    void returnItem(RadarDDS::KaTimeSeriesSequence *item);
    
private:
    /**
     * Publish a ray of data
     */
    void publish_(const MomentsFields *moments, const MomentsFields *filteredMoments);
    void addProductHousekeeping_(RadarDDS::KaProduct & p);
            
    /**
     * QtTSReader source of time series data
     */
    QtKaTSReader *_reader;
    /**
     * ProductWriter sink for our products
     */
    KaProductWriter *_writer;
    /**
     * The RAP radar moments calculator
     */
    RadarMoments _momentsCalc;
    /**
     * Number of samples to integrate when generating products
     */
    int _nSamples;
    /**
     * RadarFft object to be used in filtering.
     */
    RadarFft _fft;
    /**
     * RegressionFilter object to be used in filtering.
     */
    RegressionFilter _regFilter;
    /**
     * Accumulated time series IQ data for a dwell in progress.
     * This is sized to hold _nSamples * _nGates sets of I and Q.
     * The ordering is _dwell[gate][sample], which allows easy use
     * of RadarMoments methods.
     */
    RadarComplex_t **_dwellIQ;
    RadarComplex_t *_filteredGateIQ;    // work space to hold filtered IQ data for one gate
    kaddsSysHskp _dwellHskp;    // housekeeping from first pulse of dwell
    double _dwellAz;
    double _dwellEl;
    int _samplesCached;         // samples in the dwell so far
    int _itemCount;         	// TimeSeriesSequence-s received
    int _wrongChannelCount; 	// TSS-s with the wrong channel
    int _dwellCount;        	// dwells built
    int _dwellDiscardCount;		// dwells that could not be published
    long _lastPulseRcvd;		// last pulse number we received
    int _ddsDrops;				// how many DDS packets have we lost?
};

#endif /* PRODUCTGENERATOR_H_ */
