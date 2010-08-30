/*
 * TsPlayback.h
 *
 *  Created on: Oct 14, 2009
 *      Author: burghart
 *      
 * This class reads one or more files in IWRF Time Series format, reformats
 * the contents into ProfilerDDS::TimeSeriesSequence form, and publishes
 * the results via DDS using the given writer. The write rate is controlled
 * to match the original data rate (or some user-chosen multiple of
 * the original rate).
 */

#ifndef TSPLAYBACK_H_
#define TSPLAYBACK_H_

#include <QThread>
#include <QTime>
#include <TSWriter.h>

#include "IwrfTsFilesReader.h"

class TsPlayback : public QThread {
    
    Q_OBJECT
    
public:
	/**
	 * Constructor.
	 * @param sink the TSWriter sink for resulting time series
	 * @param fileNames the names of time series archive files for playback
	 */
    TsPlayback(TSWriter *sink, const QStringList & fileNames, float playbackSpeed = 1.0);
    virtual ~TsPlayback();
    void run();
protected slots:
    /**
     * Show information.
     */
    void showInfo();
    
private:
    /**
     * Our reader for IWRF time series files
     */
    IwrfTsFilesReader _reader;

    /**
     * TSWriter sink for our time series data
     */
    TSWriter *_writer;
    
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

#endif /* TSPLAYBACK_H_ */
