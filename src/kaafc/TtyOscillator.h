/*
 * TtyOscillator.h
 *
 *  Created on: Dec 7, 2010
 *      Author: burghart
 */

#ifndef TTYOSCILLATOR_H_
#define TTYOSCILLATOR_H_

#include <string>

/**
 * Class for one of three Ka-band serial-port-controlled oscillators, which
 * are connected @ 9600 8N1. Two types of commands are accepted by these 
 * oscillators:
 * 
 *      1) Set frequency - an 8-byte string
 *          byte 0:     oscillator number (ASCII '0', 1', or '2')
 *          byte 1:     ASCII 'm'
 *          bytes 2-6:  5-digit frequency string, in units of the oscillator's 
 *                      frequency step.
 *          byte 7:     ASCII NUL
 *          
 *          Upon receipt of this command, the oscillator should change to the
 *          requested frequency.
 * 
 *      2) Send status - a 3-byte string
 *          byte 0:     oscillator number (ASCII '0', 1', or '2')
 *          byte 1:     ASCII 's'
 *          byte 2:     ASCII NUL
 *  
 * The oscillator writes to the serial port only when prompted by receiving
 * a 'Send status' command. Its response is a 13-byte string:
 *      bytes 0-7:  unknown
 *      bytes 8-12: 5-digit frequency string, in units of the oscillator's
 *                  frequency step.
 *                  
 * The TtyOscillator class hides the serial interface described above, providing
 * a more streamlined interface.
 */
class TtyOscillator {
public:
    TtyOscillator(std::string devName, unsigned int defaultFreq,
            unsigned int freqStep);
    virtual ~TtyOscillator();
private:
    std::string _devName;
    int _fd;
    unsigned int _freqStep;
    unsigned int _currentFreq;
};

#endif /* TTYOSCILLATOR_H_ */
