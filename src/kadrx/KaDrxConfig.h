/*
 * KaDrxConfig.h
 *
 *  Created on: Jun 11, 2010
 *      Author: burghart
 */

#ifndef KADRXCONFIG_H_
#define KADRXCONFIG_H_

#include <exception>
#include <cmath>
#include <string>
#include <map>
#include <set>
#include <vector>
#include <cstdlib>
#include <iostream>

class KaDrxConfig {
public:
    KaDrxConfig(std::string configFile);
    virtual ~KaDrxConfig();

    /// number of gates
    int gates() const { 
        return _getIntVal("gates"); 
    }
    /// radar name
    std::string radar_id() const { 
        return _getStringVal("radar_id");
    }
    /** 
     * Staggered PRT? 
     * @ return 0 if false, 1 if true, or UNSET_BOOL if unset
     */
    int staggered_prt() const {
        return _getBoolVal("staggered_prt");
    }
    /** 
     * Polarization-diversity pulse pair?
     * @ return 0 if false, 1 if true, or UNSET_BOOL if unset
     */
    int pdpp() const { 
        return _getBoolVal("pdpp");
    }
    /// first PRT, s
    double prt1() const {
        return _getDoubleVal("prt1");
    }
    /// second PRT, s
    double prt2() const {
        return _getDoubleVal("prt2");
    }
    /// Burst sample delay, s
    double burst_sample_delay() const { 
        return _getDoubleVal("burst_sample_delay"); 
    }
    /// Burst sample width, s
    double burst_sample_width() const { 
        return _getDoubleVal("burst_sample_width"); 
    }
    /// Burst sample frequency, hz
    double burst_sample_frequency() const { 
        return _getDoubleVal("burst_sample_frequency"); 
    }
    /// peak transmit power, kW
    double tx_peak_power() const {
        return _getDoubleVal("tx_peak_power");
    }
    /// center transmit frequency, Hz
    double tx_cntr_freq() const {
        return _getDoubleVal("tx_cntr_freq");
    }
    /// transmit chirp bandwidth, Hz
    double tx_chirp_bandwidth() const {
        return _getDoubleVal("tx_chirp_bandwidth");
    }
    /// transmit pulse delay, s
    double tx_delay() const {
        return _getDoubleVal("tx_delay");
    }
    /// transmit pulse width, s
    double tx_pulse_width() const {
        return _getDoubleVal("tx_pulse_width");
    }
    /// transmit pulse modulation timer delay, s
    double tx_pulse_mod_delay() const {
        return _getDoubleVal("tx_pulse_mod_delay");
    }
    /// transmit pulse modulation timer width, s
    double tx_pulse_mod_width() const {
        return _getDoubleVal("tx_pulse_mod_width");
    }
    /**
     * Are we using an external clock?
     * @ return 0 if false, 1 if true, or UNSET_BOOL if unset
     */
    int external_clock() const {
        return _getBoolVal("external_clock");
    }
    /** 
     * Are we using an external start trigger? 
     * @ return 0 if false, 1 if true, or UNSET_BOOL if unset
     */
    int external_start_trigger() const {
        return _getBoolVal("external_start_trigger");
    }
    /** 
     * Are we using LDR mode? (If not, we are in ZDR mode)
     * @ return 0 if false, 1 if true, or UNSET_BOOL if unset
     */
    int ldr_mode() const {
        return _getBoolVal("ldr_mode");
    }
    
    /// Is AFC enabled?
    int afc_enabled() const {
        return _getBoolVal("afc_enabled");
    }
    /// AFC G0 threshold power for reliable calculated frequencies, in dBm
    double afc_g0_threshold_dbm() const {
        return _getDoubleVal("afc_g0_threshold_dbm");
    }
    /// AFC coarse step, Hz
    int afc_coarse_step() const {
        return _getIntVal("afc_coarse_step");
    }
    /// AFC fine step, Hz
    int afc_fine_step() const {
        return _getIntVal("afc_fine_step");
    }
    /// size of queue buffers for merge
    int merge_queue_size() const {
        return _getIntVal("merge_queue_size");
    }
    /// TCP port for IWRF data server
    int iwrf_server_tcp_port() const {
        return _getIntVal("iwrf_server_tcp_port");
    }
    /// How often do we send IWRF meta data?
    int pulse_interval_per_iwrf_meta_data() const {
      return _getIntVal("pulse_interval_per_iwrf_meta_data");
    }
    // @TODO End-of-line comments below are not working correctly in doxygen. Change to pre-comments.
    double tx_switching_network_loss() const { return _getDoubleVal("tx_switching_network_loss"); }  /// dB
    double tx_waveguide_loss() const { return _getDoubleVal("tx_waveguide_loss"); }      /// dB
    double tx_peak_pwr_coupling() const { return _getDoubleVal("tx_peak_pwr_coupling"); }        /// dB
    double tx_upconverter_latency() const { return _getDoubleVal("tx_upconverter_latency"); }    /// seconds
    
    double ant_gain() const { return _getDoubleVal("ant_gain"); }                    /// dB
    double ant_hbeam_width() const { return _getDoubleVal("ant_hbeam_width"); }      /// degrees
    double ant_vbeam_width() const { return _getDoubleVal("ant_vbeam_width"); }      /// degrees
    double ant_E_plane_angle() const { return _getDoubleVal("ant_E_plane_angle"); }  /// degrees
    double ant_H_plane_angle() const { return _getDoubleVal("ant_H_plane_angle"); }  /// degrees
    double ant_encoder_up() const { return _getDoubleVal("ant_encoder_up"); }        /// degrees
    double ant_pitch_up() const { return _getDoubleVal("ant_pitch_up"); }            /// degrees

    int actual_num_rcvrs() const { return _getIntVal("actual_num_rcvrs"); }     /// number of channels
    double rcvr_bandwidth() const { return _getDoubleVal("rcvr_bandwidth"); }   /// Hz
    double rcvr_cntr_freq() const { return _getDoubleVal("rcvr_cntr_freq"); }   /// Hz
    double rcvr_pulse_width() const { return _getDoubleVal("rcvr_pulse_width"); }   /// seconds
    double rcvr_switching_network_loss() const { return _getDoubleVal("rcvr_switching_network_loss"); }  /// dB
    double rcvr_waveguide_loss() const { return _getDoubleVal("rcvr_waveguide_loss"); }     /// dB
    double rcvr_noise_figure() const { return _getDoubleVal("rcvr_noise_figure"); }         /// dB
    double rcvr_filter_mismatch() const { return _getDoubleVal("rcvr_filter_mismatch"); }   /// dB
    double rcvr_rf_gain() const { return _getDoubleVal("rcvr_rf_gain"); }   /// dB
    double rcvr_if_gain() const { return _getDoubleVal("rcvr_if_gain"); }   /// dB
    double rcvr_digital_gain() const { return _getDoubleVal("rcvr_digital_gain"); } /// dB
    double rcvr_gate0_delay() const { return _getDoubleVal("rcvr_gate0_delay"); }   /// seconds
    
    /// Coupling difference in dB between H channel QuinStar power detector and 
    /// A/D H channel input: power_at_A/D = power_at_QuinStar + rcvr_h_power_corr
    /// @return the coupling difference between the H channel QuinStar power
    /// detector and the A/D H channel input, dB
    double rcvr_h_power_corr() const { return _getDoubleVal("rcvr_h_power_corr"); }
    
    /// Coupling difference in dB between V channel QuinStar power detector and 
    /// A/D V channel input: power_at_A/D = power_at_QuinStar + rcvr_v_power_corr
    /// @return the coupling difference between the V channel QuinStar power
    /// detector and the A/D V channel input, dB
    double rcvr_v_power_corr() const { return _getDoubleVal("rcvr_v_power_corr"); }
    
    /// Coupling difference in dB between test target QuinStar power detector 
    /// and A/D H channel input: power_at_A/D = power_at_QuinStar + rcvr_tt_power_corr
    /// @return the coupling difference between the test target QuinStar power
    /// detector and the A/D H channel input, dB
    double rcvr_tt_power_corr() const { return _getDoubleVal("rcvr_tt_power_corr"); }
    
    /// Return the range to the center of gate 0, in meters
    /// @return the range to the center of gate 0, in meters
    double range_to_gate0() const { return _getDoubleVal("range_to_gate0"); }
    
    double test_target_delay() const { return _getDoubleVal("test_target_delay"); } /// seconds
    double test_target_width() const { return _getDoubleVal("test_target_width"); } /// seconds
    
    double latitude() const { return _getDoubleVal("latitude"); }    /// degrees
    double longitude() const { return _getDoubleVal("longitude"); }  /// degrees
    double altitude() const { return _getDoubleVal("altitude"); }    /// meters MSL
    
    int ddcType() const { return _getIntVal("ddc_type"); } /// 4 or 8
    
    // iqcount_scale_for_mw: count scaling factor to easily get power in mW from
    // I and Q.  If I and Q are counts from the Pentek, the power at the A/D in 
    // mW is:
    //
    //      (I / iqcount_scale_for_mw)^2 + (Q / iqcount_scale_for_mw)^2
    //
    // This value is determined empirically.
    double iqcount_scale_for_mw() const {
        return _getDoubleVal("iqcount_scale_for_mw");
    }

    /// should we cohere the IQ to the burst?
    int cohere_iq_to_burst() const {
      return _getBoolVal("cohere_iq_to_burst");
    }

    /// should we comnine every second gate
    int combine_every_second_gate() const {
      return _getBoolVal("combine_every_second_gate");
    }

    /// write Pei format time series files?
    int write_pei_files() const {
        return _getBoolVal("write_pei_files");
    }
    
    /// maximum number of gates to write in Pei format files
    int max_pei_gates() const {
        return _getIntVal("max_pei_gates");
    }
    
    /// simulation of angles

    int simulate_antenna_angles() const {
      return _getBoolVal("simulate_antenna_angles");
    }
    int sim_n_elev() const {
      return _getIntVal("sim_n_elev");
    }
    double sim_start_elev() const {
      return _getDoubleVal("sim_start_elev"); /// deg
    }
    double sim_delta_elev() const {
      return _getDoubleVal("sim_delta_elev"); /// deg
    }
    double sim_az_rate() const {
      return _getDoubleVal("sim_az_rate"); /// deg/s
    }
    
    // Simulate existence of the PMC-730 multi-IO card?
    int simulate_pmc730() const {
    	return _getBoolVal("simulate_pmc730");
    }

    // Simulate the TTY oscillators (oscillators 0, 1, and 2)?
    int simulate_tty_oscillators() const {
    	return _getBoolVal("simulate_tty_oscillators");
    }
    
    // Are we allowing sector blanking via XML-RPC calls?
    int allow_blanking() const {
        return _getBoolVal("allow_blanking");
    }

    /**
     * Validate that all metadata values required for product generation 
     * have been set.
     * @return true iff metadata values required for product generation have
     * all been set
     */
    bool isValid(bool verbose = true) const;  
    
    /**
     * Value returned when an unset double item is requested.
     */
    static const double UNSET_DOUBLE;
    /**
     * Value returned when an unset int item is requested.
     */
    static const int UNSET_INT;
    /**
     * Value returned when an unset string item is requested.
     */
    static const std::string UNSET_STRING;
    /**
     * Value returned when an unset boolean item is requested.
     */
    static const int UNSET_BOOL;
    
private:
    
    /**
     * Look up the given key and return the associated double value, or
     * UNSET_DOUBLE if the key has no value set.
     * @param key the key to look up
     * @return the double value for the key, or UNSET_DOUBLE if the key has
     *    no value set.
     */
    double _getDoubleVal(std::string key) const {
        if (_DoubleLegalKeys.find(key) == _DoubleLegalKeys.end()) {
            std::cerr << __FUNCTION__ << ": Oops, request for bad double key " <<
                key << std::endl;
            exit(1);
        }
        std::map<std::string, double>::const_iterator it =  _doubleVals.find(key);
        if (it != _doubleVals.end())
            return it->second;
        else
            return UNSET_DOUBLE;
    }
    
    /**
     * Look up the given key and return the associated string value, or
     * an empty string if the key has no value set.
     * @param key the key to look up
     * @return the string value for the key, or an empty string if the key has
     *    no value set.
     */
    std::string _getStringVal(std::string key) const {
        if (_StringLegalKeys.find(key) == _StringLegalKeys.end()) {
            std::cerr << __FUNCTION__ << ": Oops, request for bad string key " <<
                key << std::endl;
            exit(1);
        }
        std::map<std::string, std::string>::const_iterator it = _stringVals.find(key);
        if (it != _stringVals.end())
            return it->second;
        else
            return UNSET_STRING;
    }
    
    /**
     * Look up the given key and return the associated int value, or
     * UNSET_INT if the key has no value set.
     * @param key the key to look up
     * @return the int value for the key, or UNSET_INT if the key has
     *    no value set.
     */
    int _getIntVal(std::string key) const {
        if (_IntLegalKeys.find(key) == _IntLegalKeys.end()) {
            std::cerr << __FUNCTION__ << ": Oops, request for bad int key " <<
                key << std::endl;
            exit(1);
        }
        std::map<std::string, int>::const_iterator it = _intVals.find(key);
        if (it != _intVals.end())
            return it->second;
        else
            return UNSET_INT;
    }
    
    /**
     * Look up the given key and return the associated boolean value. 
     * 0 is returned if the value is false, 1 if it is true, or UNSET_BOOL if 
     * the value is unset.
     * @param key the key to look up
     * @return 0 if the boolean value is false, 1 if it is true, or UNSET_BOOL
     *     if it is unset
     */
    int _getBoolVal(std::string key) const {
        if (_BoolLegalKeys.find(key) == _BoolLegalKeys.end()) {
            std::cerr << __FUNCTION__ << ": Oops, request for bad bool key " <<
                key << std::endl;
            exit(1);
        }
        std::map<std::string, bool>::const_iterator it = _boolVals.find(key);
        if (it != _boolVals.end())
            return(it->second ? 1 : 0);
        else
//            throw new UnsetItem(std::string("Config bool value for '") + key + "' is unset.");
            return UNSET_BOOL;  // unset
    }
    
    // Keys we allow
    static std::set<std::string> _DoubleLegalKeys;
    static std::set<std::string> _StringLegalKeys;
    static std::set<std::string> _IntLegalKeys;
    static std::set<std::string> _BoolLegalKeys;
    
    static std::set<std::string> _createDoubleLegalKeys();
    static std::set<std::string> _createStringLegalKeys();
    static std::set<std::string> _createIntLegalKeys();
    static std::set<std::string> _createBoolLegalKeys();
    
    // Our dictionaries for the values
    std::map<std::string, double> _doubleVals;
    std::map<std::string, int> _intVals;
    std::map<std::string, bool> _boolVals;
    std::map<std::string, std::string> _stringVals;
};

#endif /* KADRXCONFIG_H_ */
