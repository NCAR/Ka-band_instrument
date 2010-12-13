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
 *          byte 0:     oscillator number (ASCII '0', '1', or '2')
 *          byte 1:     ASCII 'm'
 *          bytes 2-6:  5-digit frequency string, in units of the oscillator's 
 *                      frequency step.
 *          byte 7:     ASCII NUL
 *          
 *          Upon receipt of this command, the oscillator should change to the
 *          requested frequency.
 * 
 *      2) Send status - a 3-byte string
 *          byte 0:     oscillator number (ASCII '0', '1', or '2')
 *          byte 1:     ASCII 's'
 *          byte 2:     ASCII NUL
 *  
 * The oscillator writes to the serial port only in response to a 'Send 
 * status' command. Its response is a 13-byte string:
 *      byte 0:     ASCII 's'
 *      byte 1:     oscillator number (ASCII '0', '1', or '2')
 *      byte 2:     ASCII 'd'
 * 		bytes 3-6:	4-digit string (use unknown)
 *      byte 7:		ASCII 'm'
 *      bytes 8-12: 5-digit frequency string, in units of the oscillator's
 *                  frequency step.
 *                  
 * The TtyOscillator class hides the serial interface described above, providing
 * a more streamlined interface.
 */
class TtyOscillator {
public:
    /**
     * Construct an object for controlling one of Ka's 3 serial-connected 
     * oscillators.
     * @param devName the name of the tty device connected to the oscillator
     * @param oscillatorNum the number of the oscillator, in range [0,2]
     * @param freqStep the step increment for oscillator frequencies, in Hz
     * @param scaledStartFreq the starting frequency for the oscillator, in
     *      units of freqStep
     */
    TtyOscillator(std::string devName, unsigned int oscillatorNum, 
            unsigned int freqStep, unsigned int scaledtartFreq);
    virtual ~TtyOscillator();
    
    /**
     * Set the frequency of the oscillator, in units of its frequency step.
     */
    void setFrequency(unsigned int scaledFreq);
    
    /**
     * Get the current frequency of the oscillator, in units of its frequency
     * step.
     */
    unsigned int getFrequency() const { return _currentScaledFreq; }
private:
    /**
     * Get the current status from the oscillator, which will be used to update 
     * _currentFreq.
     * @return 0 if status is read successfully, or 1 if status read times out
     */
    int _getStatus();
    
    /**
     * Send a command string to the oscillator. The command string must be
     * NULL-terminated, and the NULL character will be sent as part of the
     * command.
     * @param cmd the NULL-terminated string containing the command
     * @param timeNeeded the time needed for the oscillator to process the
     *      command, in seconds. No succeeding command will be sent until
     *      this amount of time has elapsed.
     */
    void _sendCmd(char *cmd, unsigned int timeNeeded);

    void _handleTimeout(int signum);

    std::string _devName;
    int _fd;
    unsigned int _oscillatorNum;
    unsigned int _freqStep;
    unsigned int _currentScaledFreq;
    int _nextWrite; // wait until this time before sending next cmd, seconds since 1970/1/1 00:00 UTC

};

#endif /* TTYOSCILLATOR_H_ */
