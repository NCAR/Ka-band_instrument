/*
 * TimeSeriesAdapter.h
 * 
 * Adapter class for RadarDDS::TimeSeries <-> IwrfTsPulse translations.
 *
 *  Created on: May 20, 2010
 *      Author: burghart
 */

#ifndef TIMESERIESADAPTER_H_
#define TIMESERIESADAPTER_H_

// RadarDDS::TimeSeries class
#include "kaddsTypeSupportImpl.h"

// RAL IwrfTsPulse class
#include <radar/IwrfTsPulse.hh>


class TimeSeriesAdapter {
public:
    /**
     * Convert from IwrfTsPulse to RadarDDS::TimeSeries.
     * @param[in] iwrfPulse the IwrfTsPulse to be converted
     * @param[out] ddsPulse the destination RadarDDS::TimeSeries
     */
    static void IwrfToDDS(const IwrfTsPulse& iwrfPulse, 
            RadarDDS::TimeSeries& ddsPulse);
    /**
     * Convert from RadarDDS::TimeSeries to IwrfTsPulse.
     * @param ddsPulse the RadarDDS::TimeSeries to be converted
     * @param iwrfPulse the destination IwrfTsPulse
     * @param packetSequenceNum the packet sequence number to assign to the
     *     IwrfTsPulse
     */
    static void DDSToIwrf(const RadarDDS::TimeSeries& ddsPulse,
            IwrfTsPulse& iwrfPulse, si64 packetSequenceNum);
};

#endif /* TIMESERIESADAPTER_H_ */
