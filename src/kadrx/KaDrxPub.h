#ifndef KADRXPUB_H_
#define KADRXPUB_H_

#include <QThread>
#include <p7142sd3c.h>
#include <KaTSWriter.h>
#include "KaDrxConfig.h"

/// KaDrxPub combines a downconverter and a data publisher. p7142sd3cdn
/// is inherited and provides access to the downconverter configuration
/// and operation. A TSWriter is passed in by the user, and is used for
/// data publishing.
///
/// KaDrxPub provides a separate thread for performing
/// the data processing. It is initiated by calling the start() method,
/// which then invokdes the run() method. run() simple loops, reading beams
/// from p7142sd3cdn and publishing them via the TSWriter.
class KaDrxPub: public QThread, public Pentek::p7142sd3cdn {
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
         * @param freeRun Set true to operate the SD3c in free running mode
         * @param simulate generate simulated data? (no Pentek card is used
         *     if simulate is true)
         * @param simPauseMS The number of milliseconds to wait before returning
         *     simulated data when calling read()
         * @param simWavelength The wavelength of a simulated signal. The wavelength
         *     is in sample counts.
         */
        KaDrxPub(
                const KaDrxConfig& config,
                KaTSWriter* tsWriter,
                bool publish,
                int tsLength,
                std::string devName,
                int chanId,
                std::string gaussianFile,
                std::string kaiserFile,
                bool freeRun,
                bool simulate,
                double simPauseMS,
                int simWavelength);
        
		/// Destructor
        virtual ~KaDrxPub();
		void run();
		/// @return The number of timeseries blocks that have been discarded
		/// since the last time this function was called.
		unsigned long tsDiscards();

	private:
		/// Return the current time in seconds since 1970/01/01 00:00:00 UTC.
		/// Returned value has 1 ms precision.
		/// @return the current time in seconds since 1970/01/01 00:00:00 UTC
		double _nowTime();
        /// Publish a beam. A DDS sample is built and put into _ddsSeqInProgress.
		/// When all of the samples have been filled in _ddsSeqInProgress, it is published.
        /// @param buf The raw buffer of data from the downconverter
        /// channel. It contains all Is and Qs
		/// @param pulsenum The pulse number. Will be zero for raw data.
        void publishDDS(char* buf, unsigned int pulsenum);
        /**
         * Return true iff the current configuration is valid.
         * @return true iff the current configuration is valid.
         */
        bool _configIsValid() const;
		/// Set true if we are going to publish the data
		bool _publish;
		/// The DDS time series writer
		KaTSWriter* _tsWriter;
		/// The number of unpublished blocks or partial blocks, due to no
		/// empty items being available from DDS. It is reset to zero whenever 
		/// tsDiscards() is called.
		unsigned long _tsDiscards;
		/// How many pulses are we putting in each published sample?
		int _ddsSamplePulses;
        /// The DDS time series sequence we're filling. Once it has 
		/// _ddsSamplePulses pulses in it, we publish it.
        RadarDDS::KaTimeSeriesSequence *_ddsSeqInProgress;
        /// Where are we in _ddsSeqInProgress?
        int _ndxInDdsSample;
		/// The DDS sample number; increment when a sample is published.
		long _sampleNumber;
		/// Base DDS housekeeping with values that remain fixed for this
		/// downconverter.
		RadarDDS::SysHousekeeping _baseDdsHskp;
		
};

#endif /*KADRXPUB_H_*/
