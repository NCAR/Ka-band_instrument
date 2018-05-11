/*
 * NoXmitBitmap.h
 *
 *  Created on: May 11, 2018
 *      Author: Chris Burghart <burghart@ucar.edu>
 */

#ifndef SRC_KADRX_NOXMITBITMAP_H_
#define SRC_KADRX_NOXMITBITMAP_H_

#include <cstdint>
#include <string>
#include <sstream>

/// Class which encapsulates a bitmap of reasons that kadrx may disable
/// transmit.
class NoXmitBitmap {
public:
    /// @brief Construct a NoXmitBitmap with no bits set.
    NoXmitBitmap() : _noXmitBitmap(0) {}

    virtual ~NoXmitBitmap() {}

    /// Bit numbers for conditions which will disable transmit
    typedef enum _NOXMIT_BITNUM {
            XMLRPC_REQUEST = 0,
            N2_PRESSURE_LOW,
            NO_LIMITER_TRIGGERS,
            NOXMIT_IN_BLANKING_SECTOR,
            NBITS    // Count of "no transmit" conditions
    } BITNUM;

    /// @brief Return a string describing the given bit's "no transmit" reason
    /// @param bitnum the bit number of interest
    static std::string NoXmitReason(BITNUM bitnum) {
        std::string desc;
        switch (bitnum) {
        case XMLRPC_REQUEST:
            desc = "On XML-RPC request"; break;
        case N2_PRESSURE_LOW:
            desc = "N2 waveguide pressure low"; break;
        case NO_LIMITER_TRIGGERS:
            desc = "No limiter triggers detected"; break;
        case NOXMIT_IN_BLANKING_SECTOR:
            desc = "In blanking sector"; break;
        default:
            std::ostringstream os;
            os << "BUG: unhandled NOXMIT_BITNUM " << bitnum;
            desc = os.str();
            break;
        }
        return(desc);
    }

    /// @brief Return a mask for the given BITNUM bit.
    static uint16_t MaskForBit(BITNUM bitnum) {
        return(1 << bitnum);
    }

    /// @brief Set the selected bit in the bitmap
    /// @param bitnum the bit number to be set
    void setBit(BITNUM bitnum) {
        _noXmitBitmap |= MaskForBit(bitnum);
    }

    /// @brief Return true iff the selected bit is set in the bitmap
    /// @return true iff the selected bit is set in the bitmap
    bool bitIsSet(BITNUM bitnum) const {
        return(_noXmitBitmap & MaskForBit(bitnum));
    }

    /// @brief Clear the selected bit in the bitmap
    /// @param bitnum the bit number to be cleared
    void clearBit(BITNUM bitnum) {
        _noXmitBitmap &= ~MaskForBit(bitnum);
    }

    /// @brief Return true iff the selected bit is clear in the bitmap
    /// @return true iff the selected bit is clear in the bitmap
    bool bitIsClear(BITNUM bitnum) const {
        return(~_noXmitBitmap & MaskForBit(bitnum));
    }

    /// @brief Return true iff the bitmap has no bits set
    /// @return true iff the bitmap has no bits set
    bool allBitsClear() const {
        return(_noXmitBitmap == 0);
    }
    
    bool operator!=(const NoXmitBitmap& other) const {
        return(_noXmitBitmap == other._noXmitBitmap);
    }
private:
    uint16_t _noXmitBitmap;
};

/// @brief Operator to allow for incrementing BITNUM
NoXmitBitmap::BITNUM operator++(NoXmitBitmap::BITNUM & bitnum, int dummy) {
    return(static_cast<NoXmitBitmap::BITNUM>(bitnum + 1));
}

#endif /* SRC_KADRX_NOXMITBITMAP_H_ */
