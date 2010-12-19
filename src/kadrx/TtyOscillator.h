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
     * @param freqStep the step increment for oscillator frequency, in Hz
     * @param scaledMinFreq minimum frequency for the oscillator, in
     *      units of freqStep
     * @param scaledMaxFreq maximum frequency for the oscillator, in
     *      units of freqStep
     */
    TtyOscillator(std::string devName, unsigned int oscillatorNum, 
            unsigned int freqStep, unsigned int scaledMinFreq, 
            unsigned int scaledMaxFreq);
    virtual ~TtyOscillator();
    
    /**
     * Set the frequency of the oscillator, in units of its frequency step.
     * This is a synchronous call, and will take at least a few seconds to return.
     * Upon return, the oscillator is running at the requested frequency.
     * @param scaledFreq the desired frequency, in units of the oscillator's
     *      frequency step (@see getFreqStep()).
     */
    void setScaledFreq(unsigned int scaledFreq);
    
    /**
     * Set the frequency of the oscillator, in units of its frequency step.
     * This an asynchronous call and will return immediately; it must be 
     * followed by a call to freqAttained() to test if frequency has been
     * set successfully.
     */
    void setScaledFreqAsync(unsigned int scaledFreq);
    
    /**
     * Test if a frequency requested via setScaledFreqAsync() has been set
     * successfully. This call may take up to a few seconds to return.
     * If true is returned, the desired frequency is set.
     * Otherwise, the frequency has not been changed, and another call to 
     * setScaledFreqAsync() or setScaledFreq() will be required to set the 
     * desired frequency.
     * If called when no asynchronous frequency setting is in process, it will
     * immediately return true.
     * @return true iff the oscillator is running at the requested frequency
     */
    bool freqAttained();
    
    /**
     * Get the current frequency of the oscillator, in units of its frequency
     * step (i.e., true frequency = scaled frequency x frequency step).
     * @return the frequency of the oscillator, in units of the frequency step   
     */
    unsigned int getScaledFreq() const { return _scaledCurrentFreq; }
    
    /**
     * Get the scaled minimum frequency of the oscillator, in units of its
     * frequency step (i.e., true frequency = scaled frequency x frequency step).
     * @return the minimum frequency of the oscillator, in units of the 
     *      frequency step
     */
    unsigned int getScaledMinFreq() const { return _scaledMinFreq; }

    /**
     * Get the scaled maximum frequency of the oscillator, in units of its
     * frequency step (i.e., true frequency = scaled frequency x frequency step).
     * @return the maximum frequency of the oscillator, in units of the 
     *      frequency step
     */
    unsigned int getScaledMaxFreq() const { return _scaledMinFreq; }

    /**
     * Get the frequency step for the oscillator.
     * @return the frequency step for the oscillator, in Hz
     */
    unsigned int getFreqStep() const { return _freqStep; }
    
    /**
     * Device name to use when creating a simulation TtyOscillator.
     */
    static const std::string SIM_OSCILLATOR;
        
private:
    /**
     * Get the current status from the oscillator, which will be used to update 
     * _currentFreq.
     * @return true if status is read successfully, or false if status read times out
     */
    bool _getStatus();
    
    /**
     * Command wait time after sending frequency change request, in seconds
     */
    static const unsigned int _FREQCHANGE_WAIT = 3;
    
    /**
     * Command wait time after sending status request, in seconds
     */
    static const unsigned int _STATUS_WAIT = 0;
    
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
    /// requested frequency, set to non-zero while waiting for results of 
    /// a set frequency request
    unsigned int _scaledRequestedFreq;
    /// _scaledCurrentFreq = current frequency / _freqStep
    unsigned int _scaledCurrentFreq;
    /// _scaledMinFreq = minimum frequency / _freqStep
    unsigned int _scaledMinFreq;
    /// _scaledMaxFreq = maximum frequency / _freqStep
    unsigned int _scaledMaxFreq;
    int _nextWrite; // wait until this time before sending next cmd, seconds since 1970/1/1 00:00 UTC
    
    // Is this a simulated oscillator?
    bool _simulate;
};

#endif /* TTYOSCILLATOR_H_ */
