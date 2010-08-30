/*
 * SnifferTSReader.h
 *
 *  Created on: Jan 20, 2009
 *      Author: martinc
 */

#ifndef SNIFFERTSREADER_H_
#define SNIFFERTSREADER_H_

#include <deque>
#include "TSReader.h"
#include <pthread.h>

using namespace RadarDDS;

class SnifferTSReader: public TSReader {
public:
	SnifferTSReader(DDSSubscriber& subscriber, std::string topicName);
	virtual ~SnifferTSReader();

	/// Block until the next time series is available
	/// @return The next time series. It must be returned
	/// with returnItem();
	TimeSeriesSequence* nextTSS();

	/// Called when one or more items have become available.
	/// Signal to nextTS() via the condition variable that
	/// items are ready.
	virtual void notify();

protected:
	std::deque<TimeSeriesSequence*> _sequences;
	pthread_mutex_t _sequencesMutex;
};

#endif /* SNIFFERTSREADER_H_ */
