#ifndef KADNTHREAD_H_
#define KADNTHREAD_H_

#include <QThread>
#include <p7142sd3c.h>
#include <KaTSWriter.h>
#include "KaDrxConfig.h"

class kaDnThread: public QThread, public Pentek::p7142sd3cdn {
	Q_OBJECT
	public:
        /**
         * Constructor.
         * @param config KaDrxConfig defining the desired configuration.
         * @param tsWriter the time series DDS writer to be used.
         * @param publish should we publish data via DDS?
         * @param tsLength the number of time series pulses to be sent when
         *     we publish
         * @param devName the root device name (e.g., "/dev/pentek/p7140/0")
         * @param chanId the receiver channel of this downconverter
         * @param gaussianFile Name of the file containing the Gaussian
         *     filter parameters
         * @param kaiserFile Name of the file containing the Kaiser
         *     filter parameters
         * @param simulate generate simulated data? (no Pentek card is used
         *     if simulate is true)
         * @param simPauseMS The number of milliseconds to wait before returning
         *     simulated data when calling read()
         * @param simWavelength The wavelength of a simulated signal. Thw wavelength
         *     is in sample counts.
         */
        kaDnThread(
                const KaDrxConfig& config,
                KaTSWriter* tsWriter,
                bool publish,
                int tsLength,
                std::string devName,
                int chanId,
                std::string gaussianFile,
                std::string kaiserFile,
                bool simulate,
                int simWavelength,
                int simPauseMS);
        
		/// Destructor
        virtual ~kaDnThread();
		void run();
		/// @return The number of timeseries blocks that have been discarded
		/// since the last time this function was called.
		unsigned long tsDiscards();
		/// @return The cummulative number of dropped pulses
		unsigned long droppedPulses();
		/// @return the number of synchronization errors detected.
		unsigned long syncErrors();

	private:
		/// Return the current time in seconds since 1970/01/01 00:00:00 UTC.
		/// Returned value has 1 ms precision.
		/// @return the current time in seconds since 1970/01/01 00:00:00 UTC
		double _nowTime();
        /// Decode the information in a raw (no coherent integration) buffer 
        /// received from the P7142.   The number of unprocessed bytes (which 
        /// will be < _pentekPulseLen) is returned.
        /// @param buf The raw buffer of data from the downconverter
        /// channel. It contains all Is and Qs, plus the tagging
        /// information.
        /// @param n The length of the buffer. This is redundant
        /// information, since the buffer size better match the required
        /// size based on _gates, _tsLength, and whether coherent integration
        /// is in effect or not.
		/// @return the number of unprocessed bytes in the buffer, which
		/// will always be less than _pentekPulseLen
        int _decodeAndPublishRaw(char* buf, int n);
        /// Decode the information in a buffer of coherently integrated data
        /// received from the P7142.  The number of unprocessed bytes (which 
        /// will be < _pentekPulseLen) is returned.
        /// @param buf The raw buffer of CI data from the downconverter
        /// channel. It contains all Is and Qs, plus the tagging
        /// information.
        /// @param n The length of the buffer. This is redundant
        /// information, since the buffer size better match the required
        /// size based on _gates, _tsLength, and whether coherent integration
        /// is in effect or not.
        /// @return the number of unprocessed bytes in the buffer, which
        /// will always be less than _pentekPulseLen
        int _decodeAndPublishCI(char* buf, int n);
        /// Decode the channel/pulse number word.
        /// @param buf a pointer to the channel/pulse number word
        /// @param chan return argument for the unpacked channel number
        /// @param pulseNum return argument for the unpacked pulse number
        static void _unpackChannelAndPulse(const char* buf, unsigned int & chan,
                unsigned int & pulseNum);
        static const int MAX_PULSE_NUM = 1073741823;    // 2^30 - 1
        /**
         * Return true iff the current configuration is valid.
         * @return true iff the current configuration is valid.
         */
        bool _configIsValid() const;
        /**
         * Search through buf to find the index of the next occurrence of 
         * the sync word. If not found, return -1.
         * @param buf the buffer to search for the sync word
         * @param buflen the number of bytes available in buf
         * @return the index of the next sync word in buf, or -1 if the sync
         * word is not found.
         */
        static int _indexOfNextSync(const char* buf, int buflen);
		/// set true if coherent integrator is being used
		bool _doCI;
		/// Set true if we are going to publish the data
		bool _publish;
		/// The DDS time series writer
		KaTSWriter* _tsWriter;
		/// The number of unpublished blocks or partial blocks, due to no
		/// empty items being available from DDS. It is reset to zero whenever 
		/// tsDiscards() is called.
		unsigned long _tsDiscards;
		/// How many bytes in a single pulse of data from the Pentek?
		int _pentekPulseLen;
		/// The last pulse sequence number that we received. Used to keep
		/// track of dropped pulses.
		int _lastPulse;
		/// An estimate of dropped pulses. It may be in error
		/// if the pulse tag rolls over by more than the 14 bit
		/// total that it can hold. This test is only made if the
		/// channel number passes the validity test.
		unsigned long _droppedPulses;
		/// The number of times that an incorrect channel number was received,
		/// which indicates a synchronization error. If there is a sync error,
		/// then the sequence number check is not performed.
		unsigned long _syncErrors;
		/// Set false at startup, true after the first pulse has been received.
		bool _gotFirstPulse;
		/// How many pulses are we putting in each published sample?
		int _ddsSamplePulses;
        /// The DDS time series sequence we're filling. Once it has 
		/// _ddsSamplePulses pulses in it, we publish it.
        RadarDDS::KaTimeSeriesSequence *_ddsSampleInProgress;
        /// Where are we in _ddsSampleInProgress?
        int _ndxInDdsSample;
		/// The DDS sample number; increment when a sample is published.
		long _sampleNumber;
		/// Base DDS housekeeping with values that remain fixed for this
		/// downconverter.
		RadarDDS::SysHousekeeping _baseDdsHskp;
		
};

#endif /*KADNTHREAD_H_*/
