/*
 * ProductPlayback.h
 *
 *  Created on: Oct 14, 2009
 *      Author: burghart
 *      
 * This class reads one or more files in CfRadial format, reformats
 * the contents into RadarDDS::ProductSet form, and publishes
 * the results via DDS using the given writer. The write rate is controlled
 * to match the original data rate (or some user-chosen multiple of
 * the original rate).
 */

#ifndef PRODUCTPLAYBACK_H_
#define PRODUCTPLAYBACK_H_

#include <QThread>
#include <QTime>
#include <ProductWriter.h>

#include "CfRadialFilesReader.h"

class ProductPlayback : public QThread {
    
    Q_OBJECT
    
public:
	/**
	 * Constructor.
	 * @param sink the ProductWriter sink for resulting time series
	 * @param fileNames the names of time series archive files for playback
	 */
    ProductPlayback(ProductWriter *sink, const QStringList & fileNames, float playbackSpeed = 1.0);
    virtual ~ProductPlayback();
    void run();
protected slots:
    /**
     * Show information.
     */
    void showInfo();
    
private:
    /**
     * Our reader for CFRadial product files
     */
    CfRadialFilesReader _reader;

    /**
     * ProductWriter sink for our time series data
     */
    ProductWriter *_writer;
    
    /**
     * Number of pulses to pack in each published ProfilerDDS::TimeSeriesSequence
     */
    int _pulsesPerSequence;
    
    /**
     * Time of last write, used in controlling data publish rate.
     */
    QTime _lastWriteTime;
    
    /**
     * Desired publication speed, relative to real time.  (E.g., a value
     * of 2.0 should cause data to be published at twice 'real-time'
     * speed.)
     */
    float _playbackSpeed;
};

#endif /* PRODUCTPLAYBACK_H_ */
