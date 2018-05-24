/*
 * NoXmitBitmap.h
 *
 *  Created on: May 11, 2018
 *      Author: Chris Burghart <burghart@ucar.edu>
 */

#ifndef SRC_KADRX_NOXMITBITMAP_H_
#define SRC_KADRX_NOXMITBITMAP_H_

#include <cstdint>
#include <exception>
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
            IN_BLANKING_SECTOR,
            HUP_SIGNAL_RECEIVED,
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
        case IN_BLANKING_SECTOR:
            desc = "In blanking sector"; break;
        case HUP_SIGNAL_RECEIVED:
            desc = "HUP signal received"; break;
        default:
            std::ostringstream os;
            os << "BUG: unhandled NOXMIT_BITNUM " << bitnum;
            desc = os.str();
            break;
        }
        return(desc);
    }

    /// @brief Set the selected bit in the bitmap
    /// @param bitnum the bit number to be set
    void setBit(BITNUM bitnum) {
        _noXmitBitmap |= _MaskForBit(bitnum);
    }

    /// @brief Return true iff the selected bit is set in the bitmap
    /// @return true iff the selected bit is set in the bitmap
    bool bitIsSet(BITNUM bitnum) const {
        return(_noXmitBitmap & _MaskForBit(bitnum));
    }

    /// @brief Clear the selected bit in the bitmap
    /// @param bitnum the bit number to be cleared
    void clearBit(BITNUM bitnum) {
        _noXmitBitmap &= ~_MaskForBit(bitnum);
    }

    /// @brief Return true iff the selected bit is clear in the bitmap
    /// @return true iff the selected bit is clear in the bitmap
    bool bitIsClear(BITNUM bitnum) const {
        return(~_noXmitBitmap & _MaskForBit(bitnum));
    }

    /// @brief Return true iff the bitmap has no bits set
    /// @return true iff the bitmap has no bits set
    bool allBitsClear() const {
        return(_noXmitBitmap == 0);
    }

    /// @brief Set all bits in the bitmap
    void setAllBits() {
        _noXmitBitmap = (1 << NBITS) - 1;
    }

    /// @brief Return the raw value of our bitmap.
    ///
    /// This integer value can be used to create a clone instance with the
    /// NoXmitBitmap(int bitmap) constructor.
    int rawBitmap() const { return(_noXmitBitmap); }

    bool operator!=(const NoXmitBitmap& other) const {
        return(_noXmitBitmap == other._noXmitBitmap);
    }

    /// @brief Operator to allow for incrementing BITNUM
    friend BITNUM operator++(BITNUM & bitnum, int dummy) {
        int newval = bitnum + 1;
        if (newval >= NBITS) {
            throw(std::range_error("Attempt to increment BITNUM outside legal range"));
        }
        return(static_cast<BITNUM>(newval));
    }

private:
    /// @brief KadrxStatus is a friend so that it can use the private
    /// constructor and int() cast.
    friend class KadrxStatus;

    /// @brief Cast to integer
    operator int() const { return(_noXmitBitmap); }

    /// @brief Construct from a raw bitmap value
    /// @param rawBitmap a raw bitmap value, as returned by the rawBitmap()
    /// method
    NoXmitBitmap(int rawBitmap) {
        // Test that the given bitmap value is valid
        const int MaxBitmap = (1 << NBITS) - 1;
        if (rawBitmap < 0 || rawBitmap > MaxBitmap) {
            std::ostringstream os;
            os << "Bitmap value " << rawBitmap <<
                  " is outside legal interval [0," << MaxBitmap << "]";
            throw(std::range_error(os.str()));
        }
        _noXmitBitmap = rawBitmap;
    }

    /// @brief Return a mask for the given BITNUM bit.
    static uint16_t _MaskForBit(BITNUM bitnum) {
        return(1 << bitnum);
    }

    /// Bitmap with bits marking reasons transmit is disabled.
    uint16_t _noXmitBitmap;
};

#endif /* SRC_KADRX_NOXMITBITMAP_H_ */
