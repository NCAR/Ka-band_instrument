/*
 * ProductAdapter.h
 * 
 * Adapter class for RadarDDS::ProductSet <-> RadxRay translations.
 *
 *  Created on: June 7, 2010
 *      Author: burghart
 */

#ifndef PRODUCTADAPTER_H_
#define PRODUCTADAPTER_H_

// RadarrDDS::ProductSet class
#include "kaddsTypeSupportImpl.h"

// RAL Radx classes
#include <Radx/RadxRay.hh>
#include <Radx/RadxVol.hh>
#include <Radx/RadxRcalib.hh>


class ProductAdapter {
public:
    /**
     * Convert from RadxRay (+ its RadxVol) to a RadarDDS::ProductSet ray.
     * @param radxRay the RadxRay to be converted
     * @param radxVol the RadxVol associated with radxRay
     * @param productSet the destination RadarDDS::ProductSet
     */
    static void RadxRayToDDS(const RadxRay& radxRay, const RadxVol& RadxVol,
            RadarDDS::ProductSet& productSet);
    /**
     * Convert a RadarDDS::ProductSet ray to a RadxRay/RadxVol/RadxRcalib
     * combination. Note that the three Radx objects will be populated, but
     * no association will be set between them. I.e., insertion of the RadxRay
     * into the RadxVol and association of the RadxRcalib with the RadxVol
     * and RadxRay must happen elsewhere (either before or after this
     * method is called). The platform type should be set in the RadxVol
     * before calling this function.
     * @param productSet the RadarDDS::ProductSet ray to be converted
     * @param radxRay the destination RadxRay
     * @param radxVol the RadxVol to hold volume information extracted from
     *     productSet
     * @param radxRcalib the RadxRcalib to hold calibration information 
     *     extracted from productSet.
     */
    static void DDSToRadxRay(const RadarDDS::ProductSet& productSet,
            RadxRay& radxRay, RadxVol& radxVol, RadxRcalib& radxRcalib);
};

#endif /* PRODUCTADAPTER_H_ */
