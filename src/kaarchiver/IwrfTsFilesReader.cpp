/*
 * IwrfTsFilesReader.cpp
 *
 *  Created on: Apr 22, 2010
 *      Author: burghart
 */

#include <cstdlib>
#include <cmath>
#include "IwrfTsFilesReader.h"

IwrfTsFilesReader::IwrfTsFilesReader(const QStringList& fileNames, 
        bool verbose) :
    _fileNames(fileNames),
    _currentFileNdx(-1),
    _verbose(verbose) {
}

IwrfTsFilesReader::~IwrfTsFilesReader() {
}

bool
IwrfTsFilesReader::getNextIwrfPulse(IwrfTsPulse& pulse) {
    // Make sure we have an open file and we're not at EOF
    if ((! _tsFile.isOpen() || _tsFile.atEnd()) && ! _openNextFile())
            return false;
    // Where are we in the file?
    qint64 pos = _tsFile.pos();
    
    int nread;
    
    // Peek at the next 8 bytes, which should contain the packet ID and
    // packet length. Verify that they're reasonable.
    char idAndLen[8];   // idAndLen[0] is ID, and idAndLen[1] is pkt length
    while (true) {
        if ((nread = _tsFile.peek((char*)&idAndLen, 8)) != 8) {
            std::cerr << "Short packet (" << nread << " bytes) @ byte " << 
                pos << " in file " << _currentFileName().toStdString() << 
                ". Moving to next file.";
            if (! _openNextFile())
                return false;
        } else {
            break;
        }
    }
    
    si32 id;
    memcpy(&id, idAndLen, 4);
    si32 len;
    memcpy(&len, idAndLen + 4, 4);

    if ((id & 0xffff0000) == 0x77770000) {
        /* we're good */;
    } else if ((id & 0x0000ffff) == 0x00007777) {
        // The ID is good, but endianness is swapped. Swap bytes in both
        // id and len.

        // Swap bytes in ID
        char *cptr = (char*)&id;
        cptr[0] = idAndLen[3];
        cptr[1] = idAndLen[2];
        cptr[2] = idAndLen[1];
        cptr[3] = idAndLen[0];
        // Swap bytes in len
        cptr = (char*)&len;
        cptr[0] = idAndLen[7];
        cptr[1] = idAndLen[6];
        cptr[2] = idAndLen[5];
        cptr[3] = idAndLen[4];
    } else {
        std::cerr << ": Bad packet ID " << std::hex << id  << std::dec <<
            " in file " << _currentFileName().toStdString() << " @ byte " << 
            pos << std::endl;
        exit(1);
    }
    
    // Buffer large enough for the biggest IWRF packet we're prepared to 
    // handle: i.e.,  an iwrf_ts_pulse with its 256-byte header, IWRF_MAX_CHAN 
    // channels, MAX_GATES gates, and 4-byte float representation 
    // for I and Q).
    static const int PKTBUFLEN = 256 + (IWRF_MAX_CHAN * MAX_GATES * 4 * 2);
    char packetBuf[PKTBUFLEN];
    if (len > PKTBUFLEN) {
        std::cerr << __FUNCTION__ <<  ": Packet too long (" << len << 
            " bytes) @ byte " << pos << " in file " << 
            _currentFileName().toStdString() << std::endl;
        exit(1);
    }
    
    // Read the packet into packetBuf.
    if ((nread = _tsFile.read(packetBuf, len)) != len) {
        std::cerr << __FUNCTION__ << ": Short packet read (" << nread << 
            " != " << len << ") at byte " << pos << " in file " <<
            _currentFileName().toStdString() << std::endl;
        exit(1);
    }
    
    // Different handling for pulse and non-pulse packets
    if (id == IWRF_PULSE_HEADER_ID) {
        // For a pulse packet, unpack (getting data available as floats)
        // and return
        if (pulse.setFromBuffer((void*)packetBuf, int(len), false) != 0) {
            std::cerr << __FUNCTION__ << ": Unable to decode packet @ byte " <<
                pos << " in file " << _currentFileName().toStdString() << 
                std::endl;
            exit(1);
        }
        //
        // Big KLUGE: In setFromBuffer() above, if it finds a start_range_m
        // of zero in the incoming data, it will attempt to set the value from
        // another source, and end up setting start_range_m to NaN. So if we
        // see a NaN come back, we cheat and look directly at the value we
        // passed in.  If it's zero, we force start_range_m to zero in the pulse.
        //
        if (isnan(pulse.get_start_range_m())) {
            iwrf_pulse_header_t* pulseHdr = (iwrf_pulse_header_t*)packetBuf;
            if (pulseHdr->start_range_m == 0.0)
                pulse.set_start_range_m(0.0);
        }
        return true;
    } else {
        // Unpack this non-pulse packet's metadata into our IwrfTsPulse's
        // IwrfTsInfo.
        pulse.getTsInfo().setFromBuffer((void*)packetBuf, int(len));
        // Try again for a pulse packet.
        return getNextIwrfPulse(pulse);
    }    
}

bool
IwrfTsFilesReader::_openNextFile() {
    _tsFile.close();
    if (++_currentFileNdx == _fileNames.size())
        return false;
    
    if (! QFile::exists(_currentFileName())) {
        std::cerr << "No such file: '" << _currentFileName().toStdString() << 
            "'" << std::endl;
        return _openNextFile();
    }
    _tsFile.setFileName(_currentFileName());
    if (! _tsFile.open(QIODevice::ReadOnly)) {
        std::cerr << "Error opening: '" << _currentFileName().toStdString() << 
            "'" << std::endl;
        return _openNextFile();
    }
    if (_verbose)
        std::cout << "Opened " << _currentFileName().toStdString() << std::endl;
    return true;
}
