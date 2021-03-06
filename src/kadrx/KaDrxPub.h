#ifndef KADRXPUB_H_
#define KADRXPUB_H_

#include "KaDrxConfig.h"
#include "p7142sd3c.h"

#include <cstdio>
#include <QThread>

class KaMerge;
class PulseData;
class BurstData;

/// KaDrxPub moves data from a downconverter to a data publisher. The 
/// downconverter is created for one receive channel via the associated 
/// p7142sd3c object. A TSWriter is passed in by the user, and is used for
/// data publishing.
///
/// KaDrxPub provides a separate thread for performing
/// the data processing. It is initiated by calling the start() method,
/// which then invokes the run() method. run() simply loops, reading beams
/// from p7142sd3cDn and publishing them via the TSWriter.
///
/// Since KaDrxPub does not use a Qt event loop at this point, the thread
/// should be stopped by calling its terminate() method.
class KaDrxPub : public QThread {
	Q_OBJECT
	public:
        /**
         * Enumerated values for the Ka receive channels.
         * KA_H_CHANNEL is the horizontal data channel
         * KA_V_CHANNEL is the vertical data channel
         * KA_BURST_CHANNEL is the transmit pulse sample (burst) channel
         * KA_N_CHANNELS provides the total count of Ka channels
         */
        typedef enum {
            KA_H_CHANNEL,
            KA_V_CHANNEL,
            KA_BURST_CHANNEL,
            KA_N_CHANNELS
        } KaChannel;
        
        /**
         * Constructor.
         * @param sd3c reference to the p7142sd3c object for our P7142 card
         * @param chanId the P7142 channel this thread will read
         * @param config KaDrxConfig defining the desired configuration.
         * @param merge pointer to the KaMerge instance used for merging
         * @param tsLength the number of time series pulses to be sent when
         *     we publish
         * @param gaussianFile Name of the file containing the Gaussian
         *     filter parameters
         * @param kaiserFile Name of the file containing the Kaiser
         *     filter parameters
         * @param simPauseMS The number of milliseconds to wait before returning
         *     simulated data when calling read()
         * @param simWavelength The wavelength of a simulated signal. The wavelength
         *     is in sample counts.
         */
        KaDrxPub(
                Pentek::p7142sd3c & sd3c,
                KaChannel chanId,
                const KaDrxConfig& config,
                KaMerge *merge,
                int tsLength,
                std::string gaussianFile,
                std::string kaiserFile,
                double simPauseMS,
                int simWavelength);
        
		/// Destructor
        virtual ~KaDrxPub();
		void run();
		/// @return The number of timeseries blocks that have been discarded
		/// since the last time this function was called.
		unsigned long tsDiscards();
		
		/// @return a pointer to our downconverter object
		Pentek::p7142sd3cDn* downconverter() { return _down; }

	private:
        static constexpr double _RAD_TO_DEG = 57.29577951308092;
        static constexpr double _DEG_TO_RAD = 0.017453292519943295;
        
		/// Return the current time in seconds since 1970/01/01 00:00:00 UTC.
		/// Returned value has 1 ms precision.
		/// @return the current time in seconds since 1970/01/01 00:00:00 UTC
		double _nowTime();
		
        /// add data to the merge object
        /// @param buf The raw buffer of data from the downconverter
        /// channel. It contains all Is and Qs
        /// @param pulseSeqNum The pulse number. Will be zero for raw data.
        void _addToMerge(const int16_t *iq, int64_t pulseSeqNum);
        
        /// Our associated p7142sd3c
        Pentek::p7142sd3c& _sd3c;
       
        /// Receiver channel
        unsigned int _chanId;
        
        /// configuration
        const KaDrxConfig& _config;

        /// Our associated Pentek downconverter
        Pentek::p7142sd3cDn* _down;

        /// I and Q count scaling factor to get power in mW easily:
        /// mW = (I_count / _iqScaleForMw)^2 + (Q_count / _iqScaleForMw)^2

        double _iqScaleForMw;

        /**
         * The number of gates being collected by the downconverter
         */
        unsigned int _nGates;

        /// Merging

        KaMerge *_merge;
        PulseData *_pulseData;
        BurstData *_burstData;
        
        // Are we doing AFC?
        bool _doAfc;
        
        // Keep numerator and denominator so that our burst frequency 
        // calculation performs a weighted average over time
        double _numerator;
        double _denominator;

        // burst properties
        double _g0Magnitude;
        double _g0PowerDbm;
        double _g0PhaseDeg;
        double _g0IvalNorm;
        double _g0QvalNorm;
        double _g0FreqHz;
        double _g0FreqCorrHz;
       
        // Burst frequency and phase calculation
        void _handleBurst(const int16_t *iq_data, int64_t pulseSeqNum);

        double _argDeg(double ival, double qval);

        // Are we generating a Pei file of time series data?
        bool _doPeiFile;

        // Our Pei file
        FILE* _peiFile;
        
        // Maximum number of gates to write to the Pei file
        unsigned int _maxPeiGates;
};

#endif /*KADRXPUB_H_*/
