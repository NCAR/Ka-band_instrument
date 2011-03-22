/*
 * KaDrxConfig.cpp
 *
 *  Created on: Jun 11, 2010
 *      Author: burghart
 */

#include "KaDrxConfig.h"

#include <algorithm>
#include <iostream>
#include <fstream>
#include <sstream>
#include <cerrno>
#include <climits>
#include <cmath>
#include <cstring>

const double KaDrxConfig::UNSET_DOUBLE = -INFINITY;
const int KaDrxConfig::UNSET_INT = INT_MIN;
const std::string KaDrxConfig::UNSET_STRING("<unset string>");
const int KaDrxConfig::UNSET_BOOL = UNSET_INT;

// Keys for floating point values we'll accept
std::set<std::string> KaDrxConfig::_DoubleLegalKeys(_createDoubleLegalKeys());
std::set<std::string> KaDrxConfig::_createDoubleLegalKeys() {
    std::set<std::string> keys;
    keys.insert("prt1");
    keys.insert("prt2");
    keys.insert("tx_peak_power");
    keys.insert("tx_cntr_freq");
    keys.insert("tx_chirp_bandwidth");
    keys.insert("tx_delay");
    keys.insert("tx_pulse_width");
    keys.insert("tx_pulse_mod_delay");
    keys.insert("tx_pulse_mod_width");
    keys.insert("tx_switching_network_loss");
    keys.insert("tx_waveguide_loss");
    keys.insert("tx_peak_pwr_coupling");
    keys.insert("tx_upconverter_latency");
    keys.insert("ant_gain");
    keys.insert("ant_hbeam_width");
    keys.insert("ant_vbeam_width");
    keys.insert("ant_E_plane_angle");
    keys.insert("ant_H_plane_angle");
    keys.insert("ant_encoder_up");
    keys.insert("ant_pitch_up");
    keys.insert("rcvr_bandwidth");
    keys.insert("rcvr_cntr_freq");
    keys.insert("rcvr_digital_gain");
    keys.insert("rcvr_filter_mismatch");
    keys.insert("rcvr_gate0_delay");
    keys.insert("rcvr_if_gain");
    keys.insert("rcvr_noise_figure");
    keys.insert("rcvr_pulse_width");
    keys.insert("rcvr_rf_gain");
    keys.insert("rcvr_switching_network_loss");
    keys.insert("rcvr_waveguide_loss");
    keys.insert("latitude");
    keys.insert("longitude");
    keys.insert("altitude");
    keys.insert("burst_sample_delay");
    keys.insert("burst_sample_width");
    keys.insert("burst_sample_frequency");
    keys.insert("afc_g0_threshold_dbm");
    keys.insert("iqcount_scale_for_mw");
    keys.insert("test_target_delay");
    keys.insert("test_target_width");
    keys.insert("sim_start_elev");
    keys.insert("sim_delta_elev");
    keys.insert("sim_az_rate");
    keys.insert("range_to_gate0");
    return keys;
}

// Keys for integer values we'll accept
std::set<std::string> KaDrxConfig::_IntLegalKeys(_createIntLegalKeys());
std::set<std::string> KaDrxConfig::_createIntLegalKeys() {
    std::set<std::string> keys;
    keys.insert("gates");
    keys.insert("actual_num_rcvrs");
    keys.insert("ddc_type");
    keys.insert("afc_coarse_step");
    keys.insert("afc_fine_step");
    keys.insert("merge_queue_size");
    keys.insert("iwrf_server_tcp_port");
    keys.insert("pulse_interval_per_iwrf_meta_data");
    keys.insert("sim_n_elev");
    keys.insert("max_pei_gates");
    return keys;
}

// Keys for boolean values we'll accept
std::set<std::string> KaDrxConfig::_BoolLegalKeys(_createBoolLegalKeys());
std::set<std::string> KaDrxConfig::_createBoolLegalKeys() {
    std::set<std::string> keys;
    keys.insert("afc_enabled");
    keys.insert("external_clock");
    keys.insert("external_start_trigger");
    keys.insert("pdpp");
    keys.insert("staggered_prt");
    keys.insert("ldr_mode");
    keys.insert("cohere_iq_to_burst");
    keys.insert("combine_every_second_gate");
    keys.insert("simulate_antenna_angles");
    keys.insert("write_pei_files");
    keys.insert("simulate_pmc730");
    keys.insert("simulate_tty_oscillators");
    return keys;
}

// Keys for string values we'll accept
std::set<std::string> KaDrxConfig::_StringLegalKeys(_createStringLegalKeys());
std::set<std::string> KaDrxConfig::_createStringLegalKeys() {
    std::set<std::string> keys;
    keys.insert("radar_id");
    return keys;
}

// Return a copy of a string, without comments and leading and trailing whitespace
static std::string
trimmedString(const std::string& str) {
    std::string s(str);
    // strip comment (from first '#' to end of line)
    if (s.find('#') != s.npos)
        s.erase(s.find('#'));
    // strip leading whitespace
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), 
            std::not1(std::ptr_fun<int,int>(std::isspace))));
    // strip trailing whitespace
    s.erase(std::find_if(s.rbegin(), s.rend(), std::not1(std::ptr_fun<int,int>(std::isspace))).base(), 
            s.end());
    return s;
}

// Return the contents of the given key/value line *before* the first whitespace
static std::string
keyFromLine(const std::string& line) {
    std::string s(line);
    s.erase(std::find_if(s.begin(), s.end(), std::ptr_fun<int,int>(std::isspace)), 
            s.end());
    return s;
}

// Return the contents of the given key/value line *after* the first whitespace.
static std::string
valueFromLine(const std::string& line) {
    std::string s(line);
    // erase up to the first space, then again up to the first non-space
    s.erase(s.begin(), 
            std::find_if(s.begin(), s.end(), std::ptr_fun<int,int>(std::isspace)));
    s.erase(s.begin(), 
            std::find_if(s.begin(), s.end(), std::not1(std::ptr_fun<int,int>(std::isspace))));
    return s;
}


KaDrxConfig::KaDrxConfig(std::string configFile) {
    std::fstream infile(configFile.c_str(), std::ios_base::in);
    if (infile.fail()) {
        std::cerr << "Error opening config file '" << configFile << "': " <<
            strerror(errno) << std::endl;
        exit(1);
    }
    // Read each line from the file, discarding empty lines and lines
    // beginning with '#'.
    // 
    // Other lines are parsed as "<key> <value>", and saved into
    // our dictionaries. Unknown keys will cause error exit.
    std::string line;
    while (true) {
        std::getline(infile, line);
        if (infile.eof())
            break;
        // Trim comments and leading and trailing space from the line
        line = trimmedString(line);
//        // Skip empty lines and comment lines beginning with '#'
//        if (! line.length() || line[0] == '#')
//            continue;
        // If there's nothing left, move to the next line
        if (! line.length())
            continue;
        
        std::string key = keyFromLine(line);
        std::string strValue = valueFromLine(line);
        std::istringstream valueStream(strValue);
        if (_DoubleLegalKeys.find(key) != _DoubleLegalKeys.end()) {
            double fVal;
            if ((valueStream >> fVal).fail()) {
                std::cerr << "Bad double value '" << strValue << "' for key " <<
                    key << " in config file" << std::endl;
                exit(1);
            }
            _doubleVals[key] = fVal;
        } else if (_IntLegalKeys.find(key) != _IntLegalKeys.end()) {
            int iVal;
            if ((valueStream >> iVal).fail()) {
                std::cerr << "Bad int value '" << strValue << "' for key " <<
                    key << " in config file" << std::endl;
                exit(1);
            }
            _intVals[key] = iVal;
        } else if (_BoolLegalKeys.find(key) != _BoolLegalKeys.end()) {
            bool bVal;
            if (strValue == "true") {
                bVal = true;
            } else if (strValue == "false") {
                bVal = false;
            } else if ((valueStream >> bVal).fail()) {
                std::cerr << "Bad bool value '" << strValue << "' for key " <<
                    key << " in config file" << std::endl;
                exit(1);
            }
            _boolVals[key] = bVal;
        } else if (_StringLegalKeys.find(key) != _StringLegalKeys.end()) {
            _stringVals[key] = strValue;
        } else {
            std::cerr << "Illegal key '" << key << "' in config file" << std::endl;
            exit(1);
        }
    }
}

KaDrxConfig::~KaDrxConfig() {
}

bool
KaDrxConfig::isValid(bool verbose) const {
    bool valid = true;
    if (gates() == UNSET_INT) {
        if (verbose)
            std::cerr << "'gates' unset in DRX configuration" << std::endl;
        valid = false;
    }
    if (ant_gain() == UNSET_DOUBLE) {
        if (verbose)
            std::cerr << "'ant_gain' unset in DRX configuration" << std::endl;
        valid = false;
    }
    if (ant_hbeam_width() == UNSET_INT) {
        if (verbose)
            std::cerr << "'ant_hbeam_width' unset in DRX configuration" << std::endl;
        valid = false;
    }
    if (ant_vbeam_width() == UNSET_INT) {
        if (verbose)
            std::cerr << "'ant_vbeam_width' unset in DRX configuration" << std::endl;
        valid = false;
    }
    if (staggered_prt() == UNSET_BOOL) {
        if (verbose)
            std::cerr << "'staggered_prt' unset in DRX configuration" << std::endl;
        valid = false;
    }
    if (prt1() == UNSET_DOUBLE) {
        if (verbose)
            std::cerr << "'prt1' unset in DRX configuration" << std::endl;
        valid = false;
    }
    if (rcvr_bandwidth() == UNSET_DOUBLE) {
        if (verbose)
            std::cerr << "'rcvr_bandwidth' unset in DRX configuration" << std::endl;
        valid = false;
    }
    if (rcvr_digital_gain() == UNSET_DOUBLE) {
        if (verbose)
            std::cerr << "'rcvr_digital_gain' unset in DRX configuration" << std::endl;
        valid = false;
    }
    if (rcvr_filter_mismatch() == UNSET_DOUBLE) {
        if (verbose)
            std::cerr << "'rcvr_filter_mismatch' unset in DRX configuration" << std::endl;
        valid = false;
    }
    if (rcvr_gate0_delay() == UNSET_DOUBLE) {
        if (verbose)
            std::cerr << "'rcvr_gate0_delay' unset in DRX configuration" << std::endl;
        valid = false;
    }
    if (rcvr_noise_figure() == UNSET_DOUBLE) {
        if (verbose)
            std::cerr << "'rcvr_noise_figure' unset in DRX configuration" << std::endl;
        valid = false;
    }
    if (rcvr_pulse_width() == UNSET_DOUBLE) {
        if (verbose)
            std::cerr << "'rcvr_pulse_width' unset in DRX configuration" << std::endl;
        valid = false;
    }
    if (rcvr_rf_gain() == UNSET_INT) {
        if (verbose)
            std::cerr << "'rcvr_rf_gain' unset in DRX configuration" << std::endl;
        valid = false;
    }
    if (tx_cntr_freq() == UNSET_DOUBLE) {
        if (verbose)
            std::cerr << "'tx_cntr_freq' unset in DRX configuration" << std::endl;
        valid = false;
    }
    if (tx_peak_power() == UNSET_DOUBLE) {
        if (verbose)
            std::cerr << "'tx_peak_power' unset in DRX configuration" << std::endl;
        valid = false;
    }
    if (tx_pulse_width() == UNSET_DOUBLE) {
        if (verbose)
            std::cerr << "'tx_pulse_width' unset in DRX configuration" << std::endl;
        valid = false;
    }
    if (tx_pulse_width() != rcvr_pulse_width()) {
        if (verbose)
            std::cerr << "'rcvr_pulse_width' must be the same as " <<
                "'tx_pulse_width' for Ka" << std::endl;
            valid = false;
    }
    if (tx_delay() == UNSET_DOUBLE) {
        if (verbose)
            std::cerr << "'tx_delay' unset in DRX configuration" << std::endl;
        valid = false;
    }
    if (tx_pulse_mod_delay() == UNSET_DOUBLE) {
        if (verbose)
            std::cerr << "'tx_pulse_mod_delay' unset in DRX configuration" << std::endl;
        valid = false;
    }
    if (tx_pulse_mod_width() == UNSET_DOUBLE) {
        if (verbose)
            std::cerr << "'tx_pulse_mod_width' unset in DRX configuration" << std::endl;
        valid = false;
    }
    if (burst_sample_delay() == UNSET_DOUBLE) {
        if (verbose)
            std::cerr << "'burst_sample_delay' unset in DRX configuration" << std::endl;
        valid = false;
    }
    if (burst_sample_width() == UNSET_DOUBLE) {
        if (verbose)
            std::cerr << "'burst_sample_width' unset in DRX configuration" << std::endl;
        valid = false;
    }
    if (burst_sample_frequency() == UNSET_DOUBLE) {
        if (verbose)
            std::cerr << "'burst_sample_frequency' unset in DRX configuration" << std::endl;
        valid = false;
    }
    if (external_clock() == UNSET_BOOL) {
        if (verbose)
            std::cerr << "'external_clock' unset in DRX configuration" << std::endl;
        valid = false;
    }
    if (external_start_trigger() == UNSET_BOOL) {
        if (verbose)
            std::cerr << "'external_start_trigger' unset in DRX configuration" << std::endl;
        valid = false;
    }
    if (afc_enabled() == UNSET_BOOL) {
        if (verbose)
            std::cerr << "'afc_enabled' unset in DRX configuration" << std::endl;
        valid = false;
    }
    if (afc_g0_threshold_dbm() == UNSET_DOUBLE) {
        if (verbose)
            std::cerr << "'afc_g0_threshold_dbm' unset in DRX configuration" << std::endl;
        valid = false;
    }
    if (afc_coarse_step() == UNSET_INT) {
        if (verbose)
            std::cerr << "'afc_coarse_step' unset in DRX configuration" << std::endl;
        valid = false;
    }
    if (afc_fine_step() == UNSET_INT) {
        if (verbose)
            std::cerr << "'afc_fine_step' unset in DRX configuration" << std::endl;
        valid = false;
    }
    if (ldr_mode() == UNSET_BOOL) {
        if (verbose)
            std::cerr << "'ldr_mode' unset in DRX configuration" << std::endl;
        valid = false;
    }
    if (merge_queue_size() == UNSET_INT) {
        if (verbose)
            std::cerr << "'merge_queue_size' unset in DRX configuration" << std::endl;
        valid = false;
    }
    if (iwrf_server_tcp_port() == UNSET_INT) {
        if (verbose)
            std::cerr << "'iwrf_server_tcp_port' unset in DRX configuration" << std::endl;
        valid = false;
    }
    if (pulse_interval_per_iwrf_meta_data() == UNSET_INT) {
        if (verbose)
            std::cerr << "'pulse_interval_per_iwrf_meta_data' unset in DRX configuration" << std::endl;
        valid = false;
    }
    if (iqcount_scale_for_mw() == UNSET_DOUBLE) {
        if (verbose)
            std::cerr << "'iqcount_scale_for_mw' unset in DRX configuration" << std::endl;
        valid = false;
    }
    if (cohere_iq_to_burst() == UNSET_BOOL) {
        if (verbose)
            std::cerr << "'cohere_iq_to_burst' unset in DRX configuration" << std::endl;
        valid = false;
    }
    if (combine_every_second_gate() == UNSET_BOOL) {
        if (verbose)
            std::cerr << "'combine_every_second_gate' unset in DRX configuration" << std::endl;
        valid = false;
    }
    if (test_target_delay() == UNSET_DOUBLE) {
        if (verbose)
            std::cerr << "'test_target_delay' unset in DRX configuration" << std::endl;
        valid = false;
    }
    if (test_target_width() == UNSET_DOUBLE) {
        if (verbose)
            std::cerr << "'test_target_width' unset in DRX configuration" << std::endl;
        valid = false;
    }
    if (range_to_gate0() == UNSET_DOUBLE) {
        if (verbose)
            std::cerr << "'range_to_gate0' unset in DRX configuration" << std::endl;
        valid = false;
    }
    if (write_pei_files() == UNSET_BOOL) {
        if (verbose)
            std::cerr << "'write_pei_files' unset in DRX configuration" << std::endl;
        valid = false;
    }
    if (max_pei_gates() == UNSET_INT) {
        if (verbose)
            std::cerr << "'max_pei_gates' unset in DRX configuration" << std::endl;
        valid = false;
    }
    if (simulate_pmc730() == UNSET_BOOL) {
        if (verbose)
            std::cerr << "'simulate_pmc730' unset in DRX configuration" << std::endl;
        valid = false;
    }
    if (simulate_tty_oscillators() == UNSET_BOOL) {
        if (verbose)
            std::cerr << "'simulate_tty_oscillators' unset in DRX configuration" << std::endl;
        valid = false;
    }
    return valid;
}

std::vector<double> 
KaDrxConfig::gp_timer_delays() const {
    std::vector<double> delays;
    // The first of the general purpose timers (OUTPUT_TIMER) is used in Ka for 
    // transmit pulse modulation.
    delays.push_back(tx_pulse_mod_delay());
    // The other GP timers are unused
    delays.push_back(0);
    delays.push_back(0);
    delays.push_back(0);
    
    return delays;
}

std::vector<double> 
KaDrxConfig::gp_timer_widths() const {
    std::vector<double> widths;
    // The first of the general purpose timers (OUTPUT_TIMER) is used in Ka for 
    // transmit pulse modulation.
    widths.push_back(tx_pulse_mod_width());
    // The other GP timers are unused
    widths.push_back(0);
    widths.push_back(0);
    widths.push_back(0);
    
    return widths;
}
