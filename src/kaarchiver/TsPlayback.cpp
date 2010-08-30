/*
 * TsPlayback.cpp
 *
 *  Created on: Apr 13, 2010
 *      Author: burghart
 */

#include "TsPlayback.h"
#include "TimeSeriesAdapter.h"

#include <iostream>
#include <cstdlib>
#include <ctime>
#include <unistd.h> // for usleep()

static const double SPEED_OF_LIGHT = 2.99792458e8; // m s-1

TsPlayback::TsPlayback(TSWriter *sink, const QStringList & fileNames,
        float playbackSpeed) : 
    _reader(fileNames),
    _writer(sink),
    _pulsesPerSequence(256),
    _lastWriteTime(),
    _playbackSpeed(playbackSpeed) {
}

TsPlayback::~TsPlayback() {
}

void 
TsPlayback::run() {
    bool channelCountWarned = false;
    RadarDDS::TimeSeriesSequence *tsSequence;
    
    // Have we warned about our receiver bandwidth kluge yet?
    bool warnedOnRxBandwidth = false;

    while (1) {
        tsSequence = _writer->getEmptyItem();
        tsSequence->chanId = 0;
        tsSequence->tsList.length(_pulsesPerSequence);
        if (! tsSequence) {
            std::cerr << "Unable to get an empty item from our writer!" << std::endl;
            exit(1);
        }

        // Fill the sequence
        IwrfTsInfo iwrfInfo;
        IwrfTsPulse iwrfPulse(iwrfInfo);
        for (int pulse = 0; pulse < _pulsesPerSequence; pulse++) {
            if (! _reader.getNextIwrfPulse(iwrfPulse))
                return;
            if (! channelCountWarned && iwrfPulse.get_n_channels() != 1) {
                std::cerr << "IWRF data contains " << 
                        iwrfPulse.get_n_channels() <<
                        " but only channel zero will be published!" << std::endl;
                channelCountWarned = true;
            }
            RadarDDS::TimeSeries & ddsPulse = tsSequence->tsList[pulse];
            TimeSeriesAdapter::IwrfToDDS(iwrfPulse, ddsPulse);
            // IwrfTsPulse has no metadata for receiver bandwidth, so 
            // rcvr_bandwidth in our RadarDDS::TimeSeries is currently unset.
            // Fix that now. Note that the 0.7 is only a nominal factor which 
            // applies to HCR as of about July 2010.
            // @TODO: find a better solution to this problem...
            ddsPulse.hskp.rcvr_bandwidth = 0.7 / ddsPulse.hskp.rcvr_pulse_width;
            if (! warnedOnRxBandwidth) {
                std::cout << "WARNING: Using 0.7 * 1/(receiver pulse width) " <<
                        "as the receiver bandwidth!" << std::endl;
                warnedOnRxBandwidth = true;
            }
        }
        // Figure out total time contained in this sequence, and how long 
        // until we can publish this set, given the requested playback speed.  
        // Sleep for a bit if necessary.
        int sequenceWidthUs = 
            tsSequence->tsList[_pulsesPerSequence - 1].hskp.timetag -
            tsSequence->tsList[0].hskp.timetag;
        int msecDelay = int(sequenceWidthUs / (1000 * _playbackSpeed));
        int msecSleep = msecDelay - _lastWriteTime.elapsed();
        // If we haven't published anything yet, publish this set immediately
        // and initialize _lastWriteTime.
        if (_lastWriteTime.isNull()) {
            msecSleep = 0;
            _lastWriteTime.start();
        }
        // If not enough time has elapsed since the last publish, sleep for
        // a bit.
        if (msecSleep > 0) {
            // Catch absurdly long sleeps.
            if (msecSleep > 500) {
                std::cerr << "Only sleeping 500 ms, rather than " << 
                    msecSleep << " that was calculated..." << std::endl;
                msecSleep = 500;
            }
            usleep(1000 * msecSleep);
        }
        // publish it
        _writer->publishItem(tsSequence);
        // Set _lastWriteTime to now, and restart its interval timer.
        _lastWriteTime.restart();
        tsSequence = 0;
    }
}

void
TsPlayback::showInfo() {
//    std::cerr << "showInfo(): " << _itemCount << " tsSequence-s received, " << 
//        _wrongChannelCount << " were wrong channel. " <<
//        _dwellCount << " product rays generated and " << 
//        _dwellDiscardCount << " could not be published" << std::endl;
}
