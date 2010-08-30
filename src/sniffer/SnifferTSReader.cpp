/*
 * SnifferTSReader.cpp
 *
 *  Created on: Jan 20, 2009
 *      Author: martinc
 */

#include "SnifferTSReader.h"

//////////////////////////////////////////////////////////////
SnifferTSReader::SnifferTSReader(DDSSubscriber& subscriber, std::string topicName):
TSReader(subscriber, topicName){
	pthread_mutex_init(&_sequencesMutex, 0);
}

//////////////////////////////////////////////////////////////
SnifferTSReader::~SnifferTSReader() {

}

//////////////////////////////////////////////////////////////
TimeSeriesSequence*
SnifferTSReader::nextTSS() {

	pthread_mutex_lock(&_sequencesMutex);

	TimeSeriesSequence* tss = 0;
	if (_sequences.size()) {
		tss = _sequences[0];
		_sequences.pop_front();
	}

	pthread_mutex_unlock(&_sequencesMutex);
	return tss;

}

//////////////////////////////////////////////////////////////
void
SnifferTSReader::notify() {
	TimeSeriesSequence* tss;
	while ((tss = getNextItem())) {
		pthread_mutex_lock(&_sequencesMutex);
		_sequences.push_back(tss);
		pthread_mutex_unlock(&_sequencesMutex);
	}
}
