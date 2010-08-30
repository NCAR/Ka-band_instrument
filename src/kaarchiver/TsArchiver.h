#ifndef TSARCHIVER_H_
#define TSARCHIVER_H_

#include <vector>
#include <cstdio>

// headers for radd/archiver
#include <archiver/ArchiverService_impl.h>
#include <TSReader.h>

// RAL IwrfTsPulse class
#include <radar/IwrfTsPulse.hh>


// Singleton TsArchiver class
class TsArchiver : public TSReader {
public:
    // Get the singleton instance, creating it if necessary.
    static TsArchiver* TheArchiver(DDSSubscriber& subscriber,
            std::string topicName, std::string dataDir) {
        if (! _theArchiver)
            _theArchiver = new TsArchiver(subscriber, topicName, dataDir);
        return _theArchiver;
    }
    // Get the singleton instance or NULL
    static TsArchiver* TheArchiver() { return _theArchiver; }

    /// Subclass DDSReader::notify(), which will be called
    /// whenever new samples are added to the DDSReader available
    /// queue. Process the samples here.
    virtual void notify();

    /// Return the number of bytes written by this archiver.
    /// @return the number of bytes written by this archiver.
    int bytesWritten() const { return _bytesWritten; }
    
    int ddsDrops() { 
    	int drops = _ddsDrops;
    	_ddsDrops = 0;
    	return drops;
    }

protected:
    TsArchiver(DDSSubscriber& subscriber, std::string topicName,
    		std::string dataDir);
    virtual ~TsArchiver();
    
    void _assembleIwrfPulse(const RadarDDS::TimeSeries& ddsPulse);
    
    void _writeIwrfPulse();
    
private:
    /// Maximum number of gates we're prepared to handle...
    static const int TSARCHIVER_MAX_GATES = 4096;
    
    /// Are we ready to accept notify() calls yet?
    bool _acceptNotify;
    
    /// Pointer to the singleton archiver instance
    static TsArchiver* _theArchiver;

    /// Our radd/archiver servant and last status we got from it
    archiver::ArchiverService_var _archiverServant;
    archiver::ArchiverStatus_var _status;
    
    /// IwrfTsPulse object which we fill and then write for each incoming
    /// pulse
    IwrfTsInfo _iwrfInfo;
    IwrfTsPulse _iwrfPulse;
    
    /// Scratch ByteBlock, big enough to hold a really large IWRF packet (i.e., 
    /// an iwrf_ts_pulse with its 256-byte header, IWRF_MAX_CHAN 
    /// channels, TSARCHIVER_MAX_GATES gates, and 4-byte float representation 
    /// for I and Q).
    static const int SCRATCH_BLOCK_SIZE = 256 + 
        IWRF_MAX_CHAN * TSARCHIVER_MAX_GATES * 4 * 2;
    archiver::ByteBlock _scratchBlock;
    
    /// A FILE* which allows us to write to our scratch block as a file.
    FILE* _scratchBlockAsFile;
    
    /// last pulse number we received
    long _lastPulseRcvd;
    
    /// Our incrementing pulse counter (may disappear if a pulse number is
    /// added to the time series data)
    si64 _pktSeqNum;
    si64 _lastSeqWritten;
    /// How many bytes have we written?
    int _bytesWritten;
    /// How many full DDS packets have we missed?
    int _ddsDrops;
    /// The sample number of the last DDS sample. Used to detect 
	/// dropped DDS samples. Initialized to -1.
	int _lastDDSsample;
    
};

#endif /*TSARCHIVER_H_*/
