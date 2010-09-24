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
    return keys;
}

// Keys for integer values we'll accept
std::set<std::string> KaDrxConfig::_IntLegalKeys(_createIntLegalKeys());
std::set<std::string> KaDrxConfig::_createIntLegalKeys() {
    std::set<std::string> keys;
    keys.insert("gates");
    keys.insert("actual_num_rcvrs");
    keys.insert("ddc_type");
    return keys;
}

// Keys for boolean values we'll accept
std::set<std::string> KaDrxConfig::_BoolLegalKeys(_createBoolLegalKeys());
std::set<std::string> KaDrxConfig::_createBoolLegalKeys() {
    std::set<std::string> keys;
    keys.insert("staggered_prt");
    keys.insert("pdpp");
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
            if ((valueStream >> bVal).fail()) {
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

void 
KaDrxConfig::fillDdsSysHousekeeping(RadarDDS::SysHousekeeping& hskp) const {
    // The non-bool values are easy, since we have special values to indicate
    // the ones which weren't set.
    hskp.actual_num_rcvrs = actual_num_rcvrs();
    hskp.altitude = altitude();
    hskp.ant_E_plane_angle = ant_E_plane_angle();
    hskp.ant_H_plane_angle = ant_H_plane_angle();
    hskp.ant_encoder_up = ant_encoder_up();
    hskp.ant_gain = ant_gain();
    hskp.ant_hbeam_width = ant_hbeam_width();
    hskp.ant_pitch_up = ant_pitch_up();
    hskp.ant_vbeam_width = ant_vbeam_width();
    hskp.gates = gates();
    hskp.latitude = latitude();
    hskp.longitude = longitude();
    hskp.prt1 = prt1();
    hskp.prt2 = prt2();
    hskp.radar_id = radar_id().c_str();
    hskp.rcvr_cntr_freq = rcvr_cntr_freq();
    hskp.rcvr_digital_gain = rcvr_digital_gain();
    hskp.rcvr_filter_mismatch = rcvr_filter_mismatch();
    hskp.rcvr_gate0_delay = rcvr_gate0_delay();
    hskp.rcvr_if_gain = rcvr_if_gain();
    hskp.rcvr_noise_figure = rcvr_noise_figure();
    hskp.rcvr_pulse_width = rcvr_pulse_width();
    hskp.rcvr_rf_gain = rcvr_rf_gain();
    hskp.rcvr_switching_network_loss = rcvr_switching_network_loss();
    hskp.rcvr_waveguide_loss = rcvr_waveguide_loss();
    hskp.tx_chirp_bandwidth = tx_chirp_bandwidth();
    hskp.tx_cntr_freq = tx_cntr_freq();
    hskp.tx_peak_power = tx_peak_power();
    hskp.tx_peak_pwr_coupling = tx_peak_pwr_coupling();
    hskp.tx_pulse_width = tx_pulse_width();
    hskp.tx_switching_network_loss = tx_switching_network_loss();
    hskp.tx_upconverter_latency = tx_upconverter_latency();
    hskp.tx_waveguide_loss = tx_waveguide_loss();
    
    // For the bool values, we need to set a default value if they are unset
    if (pdpp() != UNSET_BOOL)
        hskp.pdpp = pdpp();
    else
        hskp.pdpp = false;  // default to false if unset
    
    if (staggered_prt() != UNSET_BOOL)
        hskp.staggered_prt = staggered_prt();
    else
        hskp.staggered_prt = false; // default to false if unset
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
    
    return valid;
}

std::vector<double> 
KaDrxConfig::gp_timer_delays() const {
    std::vector<double> delays;
    // The first of the general purpose timers (TIMER3) is used in Ka for 
    // transmit pulse modulation.
    delays.push_back(tx_pulse_mod_delay());
    // The other GP timers are unused
    delays.push_back(0);
    delays.push_back(0);
    delays.push_back(0);
    delays.push_back(0);
    
    return delays;
}

std::vector<double> 
KaDrxConfig::gp_timer_widths() const {
    std::vector<double> widths;
    // The first of the general purpose timers (TIMER3) is used in Ka for 
    // transmit pulse modulation.
    widths.push_back(tx_pulse_mod_width());
    // The other GP timers are unused
    widths.push_back(0);
    widths.push_back(0);
    widths.push_back(0);
    widths.push_back(0);
    
    return widths;
}
