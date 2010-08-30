#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <cstdio>   // for popen()
#include <cstdlib>
#include <cstring>  // for strncpy() & strerror()
#include <cerrno>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

// headers for radd/archiver
#include <archiver/ArchiverListener.h>
#include <archiver/Resolver.h>
#include <archiver/Writer.h>
#include <tao/ORB_Core.h>

#include <dds/DCPS/Service_Participant.h>    // for TheServiceParticipant

#include "TimeSeriesAdapter.h"
#include "TsArchiver.h"

using std::string;

using namespace archiver;

typedef NameResolver<PortableServer::POA, InitResolver> POAResolver;

// Pointer to our singleton instance
TsArchiver* TsArchiver::_theArchiver = 0;

TsArchiver::TsArchiver(DDSSubscriber& subscriber, 
        std::string topicName, std::string dataDir) : 
            TSReader(subscriber, topicName), 
            _acceptNotify(false),
            _iwrfInfo(),
            _iwrfPulse(_iwrfInfo),
            _scratchBlock(SCRATCH_BLOCK_SIZE),
            _lastPulseRcvd(-1),
            _pktSeqNum(0),
            _lastSeqWritten(0),
            _bytesWritten(0),
            _lastDDSsample(-1) {
    std::cout << "Writing to data dir: " << dataDir << std::endl;
    
    // Create the archiver servant
    ArchiverService_impl* serviceImpl = 
        new ArchiverService_impl("archiver", TheServiceParticipant->get_ORB(),
                new archiver::Writer(), /*standalone*/ true);
    if (! serviceImpl) {
        std::cerr << __FUNCTION__ << ":" << __LINE__ << 
            ": Failed to create ArchiverService_impl";
        exit(1);
    }
    _archiverServant = serviceImpl->_this();

    // Configure the archiver servant and start it
    ArchiverConfig config = archiver::defaultConfig();
    config.fileName = "Ka_ts";
    config.fileSizeLimit = 100 * 1024 * 1024; // 100 MB
    config.directoryName = dataDir.c_str();
    _status = _archiverServant->reconfig(config);
    _status = _archiverServant->start();
    
    // Create a FILE* which allows us to write directly into our _scratchBlock.
    // We open using binary mode to avoid writing any extraneous null bytes.
    _scratchBlockAsFile = fmemopen((char*)_scratchBlock.get_buffer(), 
            _scratchBlock.maximum(), "wb");
    if (! _scratchBlockAsFile) {
        std::cerr << "TsArchiver::TsArchiver() error in fmemopen: " <<
            strerror(errno) << std::endl;
        exit(1);
    }
    
    // We can finally accept notify() calls...
    _acceptNotify = true;
}

TsArchiver::~TsArchiver() {
    fclose(_scratchBlockAsFile);
    _archiverServant->quit();
    _theArchiver = 0;
}

// This is the method which assembles and writes pulse packets when 
// new pulse data arrive.
void
TsArchiver::notify() {
    if (! _acceptNotify) {
    	std::cerr << "notify() call before construction complete!" << std::endl;
        return;
    }
    
    while (RadarDDS::TimeSeriesSequence* pItem = getNextItem()) {
    	
		// check for dropped samples by examining the DDS sample counter
		int ddsSample = pItem->sampleNumber;
		int delta = ddsSample - _lastDDSsample;

		if (_lastDDSsample != -1) {
			if (delta != 1) {
				if (delta > 1) {
					_ddsDrops += delta-1;
				} else {
					if (delta > -INT_MAX) {
						_ddsDrops += INT_MAX + delta;
					}
				}
			}
		}
		_lastDDSsample = ddsSample;

        int nPulses = pItem->tsList.length();
        for (int pulse = 0; pulse < nPulses; pulse++) {
            const RadarDDS::TimeSeries & ts = pItem->tsList[pulse];
            long pulseNum = ts.pulseNum;
            long delta = pulseNum - _lastPulseRcvd;
            if (_lastPulseRcvd > 0 && (delta != 1) && pulse != 0) {
            		std::cerr << "Pulse out of sequence! Got pulse " << 
            			pulseNum << " after pulse " << _lastPulseRcvd << std::endl;
            		exit(1);
            }
            _lastPulseRcvd = pulseNum;
            _assembleIwrfPulse(ts);
            _writeIwrfPulse();
        }
        returnItem(pItem);
    }
}

void
TsArchiver::_assembleIwrfPulse(const RadarDDS::TimeSeries & ddsPulse) {
    // Radar info
    if (! (_pktSeqNum % 100)) {
        iwrf_radar_info_t radarInfo;
        radarInfo.latitude_deg = 0.0;
        radarInfo.longitude_deg = 0.0;
        radarInfo.altitude_m = 0.0;
        radarInfo.platform_type = IWRF_RADAR_PLATFORM_FIXED;
        radarInfo.beamwidth_deg_h = 0.0;
        radarInfo.beamwidth_deg_v = 0.0;
        radarInfo.wavelength_cm = 0.32;
        radarInfo.nominal_gain_ant_db_h = 0.0;
        radarInfo.nominal_gain_ant_db_v = 0.0;
        memset(radarInfo.radar_name, 0, IWRF_MAX_RADAR_NAME);
        strncpy(radarInfo.radar_name, "Ka", IWRF_MAX_RADAR_NAME - 1);
        memset(radarInfo.site_name, 0, IWRF_MAX_SITE_NAME);
        strncpy(radarInfo.site_name, "nowhere", IWRF_MAX_SITE_NAME - 1);
        _iwrfInfo.setRadarInfo(radarInfo);
        _iwrfInfo.setRadarInfoPktSeqNum(++_pktSeqNum);
        _iwrfInfo.setRadarInfoActive(true);
    }

    TimeSeriesAdapter::DDSToIwrf(ddsPulse, _iwrfPulse, ++_pktSeqNum);
}

void
TsArchiver::_writeIwrfPulse() {
	archiver::ArchiverStatus *status;
	
    // Encode and write out IWRF packets for any metadata newer than 
    // _lastSeqWritten
    rewind(_scratchBlockAsFile);
    if (_iwrfInfo.writeMetaToFile(_scratchBlockAsFile, _lastSeqWritten) != 0) {
        std::cerr << __FUNCTION__ << ": error writing metadata before pulse @ " << 
            _iwrfPulse.getFTime() << std::endl;
    }
    _scratchBlock.length(ftell(_scratchBlockAsFile));
    // If writeMetaToFile actually put anything into our block, write the
    // block to disk.
    if (_scratchBlock.length()) {
        fflush(_scratchBlockAsFile);
        status = _archiverServant->sendBlock(_scratchBlock);
        delete(status);
        _bytesWritten += _scratchBlock.length();
    }
    
    // Encode and then write out the pulse packet
    rewind(_scratchBlockAsFile);
    if (_iwrfPulse.writeToFile(_scratchBlockAsFile) != 0) {
        std::cerr << __FUNCTION__ << ": error packing pulse @ " <<
            _iwrfPulse.getFTime() << std::endl;
    }
    _scratchBlock.length(ftell(_scratchBlockAsFile));
    fflush(_scratchBlockAsFile);
    status = _archiverServant->sendBlock(_scratchBlock);
    delete(status);
    _bytesWritten += _scratchBlock.length();
    
    _lastSeqWritten = _iwrfPulse.getSeqNum();
}
