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
#include <kaddsC.h>


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
     * Are we using an external start trigger? 
     * @ return 0 if false, 1 if true, or UNSET_BOOL if unset
     */
    bool external_start_trigger() const {
        return _getBoolVal("external_start_trigger");
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
    double rcvr_cntr_freq() const { return _getDoubleVal("rcvr_cntr_freq"); }        /// Hz
    double rcvr_pulse_width() const { return _getDoubleVal("rcvr_pulse_width"); }    /// seconds
    double rcvr_switching_network_loss() const { return _getDoubleVal("rcvr_switching_network_loss"); }  /// dB
    double rcvr_waveguide_loss() const { return _getDoubleVal("rcvr_waveguide_loss"); }      /// dB
    double rcvr_noise_figure() const { return _getDoubleVal("rcvr_noise_figure"); }          /// dB
    double rcvr_filter_mismatch() const { return _getDoubleVal("rcvr_filter_mismatch"); }    /// dB
    double rcvr_rf_gain() const { return _getDoubleVal("rcvr_rf_gain"); }                  /// dB
    double rcvr_if_gain() const { return _getDoubleVal("rcvr_if_gain"); }            /// dB
    double rcvr_digital_gain() const { return _getDoubleVal("rcvr_digital_gain"); }  /// dB
    double rcvr_gate0_delay() const { return _getDoubleVal("rcvr_gate0_delay"); }    /// seconds
    /// Five delay values for the general purpose timers in the sd3c firmware.
    /// The first GP timer is used for tx pulse modulation, and the rest are
    /// unused.
    std::vector<double> gp_timer_delays() const; 
    /// Five width values for the general purpose timers in the sd3c firmware.
    /// The first GP timer is used for tx pulse modulation, and the rest are
    /// unused.
    std::vector<double> gp_timer_widths() const; 

    double latitude() const { return _getDoubleVal("latitude"); }    /// degrees
    double longitude() const { return _getDoubleVal("longitude"); }  /// degrees
    double altitude() const { return _getDoubleVal("altitude"); }    /// meters MSL
    
    int ddcType() const { return _getIntVal("ddc_type"); } /// 4 or 8
    
    /**
     * Fill the given RadarDDS::SysHousekeeping struct from contents of the
     * configuration. Some or all existing contents may be overwritten.
     * @param hskp the RadarDDS::SysHousekeeping struct to be filled
     */
    void fillDdsSysHousekeeping(RadarDDS::SysHousekeeping& hskp) const;
    
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
