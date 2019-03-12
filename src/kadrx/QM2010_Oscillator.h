// QM2010_Oscillator.h
//
//  Created on: Sep 26, 2018
//      Author: burghart

#ifndef QM2010_OSCILLATOR_H_
#define QM2010_OSCILLATOR_H_

#include <stdint.h>
#include <string>

/// @brief Class to handle communication with a Quonset Microwave QM2010
/// synthesizer.
class QM2010_Oscillator {
public:
    /// @brief Constructor
    /// @param devName the name of the tty device connected to the oscillator
    /// @param oscNum an integer ID for the oscillator
    /// @param refFreqMHz external reference frequency for the oscillator, MHz
    /// @param freqStepHz the step increment for oscillator frequency, Hz
    /// @param scaledMinFreq minimum frequency for the oscillator, in
    ///      units of freqStep
    /// @param scaledMaxFreq maximum frequency for the oscillator, in
    ///      units of freqStep
    QM2010_Oscillator(std::string devName,
                      uint oscNum,
                      uint refFreqMHz,
                      uint freqStepHz,
                      uint scaledMinFreq,
                      uint scaledMaxFreq);
    virtual ~QM2010_Oscillator();

    /// @brief Set the frequency of the oscillator, in units of its frequency
    /// step.
    /// @param scaledFreq the desired frequency, in units of the oscillator's
    ///      frequency step (@see getFreqStep()).
    void setScaledFreq(uint scaledFreq);

    /// @brief Get the current frequency of the oscillator, in units of its
    /// frequency step
    ///
    /// True frequency = scaled frequency x frequency step.
    /// @return the frequency of the oscillator, in units of the frequency step
    uint getScaledFreq() const { return _scaledCurrentFreq; }

    /// @brief Get the scaled minimum frequency of the oscillator, in units of its
    /// frequency step
    ///
    /// True frequency = scaled frequency x frequency step.
    /// @return the minimum frequency of the oscillator, in units of the
    ///      frequency step
    uint getScaledMinFreq() const { return _scaledMinFreq; }

    /// @brief Get the scaled maximum frequency of the oscillator, in units of its
    /// frequency step
    ///
    /// True frequency = scaled frequency x frequency step.
    /// @return the maximum frequency of the oscillator, in units of the
    ///      frequency step
    uint getScaledMaxFreq() const { return _scaledMaxFreq; }

    /// @brief Get the frequency step for the oscillator.
    /// @return the frequency step for the oscillator, in Hz
    uint getFreqStep() const { return _freqStep; }

private:
    /// @brief Get the current status from the oscillator, which will be used
    /// to update _currentFreq.
    /// @return true if status is read successfully, or false if status read
    /// times out
    bool _getStatus();

    /// @brief Send a command string to the oscillator.
    /// @param cmd the command string
    /// @return 0 if the command is sent successfully, else 1
    void _sendCmd(const std::string & cmd);

    /// @brief Send a command string to the oscillator and return the reply
    /// to the command
    ///
    /// The command string must end with a '?' to generate a reply
    /// @param cmd the command string
    /// @return the reply to the command
    std::string _sendCmdAndGetReply(const std::string & cmd);

    /// @brief Send a command string to the oscillator then parse and return
    /// the boolean result in the reply to cmd
    /// @param cmd the command string
    /// @return the boolean result in the reply to cmd
    bool _sendCmdAndGetBoolReply(const std::string & cmd);

    /// @brief Send a command string to the oscillator then parse and return
    /// the floating point result in the reply to cmd
    /// @param cmd the command string
    /// @return the floating point result in the reply to cmd
    float _sendCmdAndGetFloatReply(const std::string & cmd);

    /// @brief Send a command string to the oscillator then parse and return
    /// the integer result in the reply to cmd
    /// @param cmd the command string
    /// @return the integer result in the reply to cmd
    int _sendCmdAndGetIntReply(const std::string & cmd);

    /// @brief Return the oscillator name, "osc<oscNum>"
    /// @return the oscillator name, "osc<oscNum>"
    std::string _oscName() {
        return(std::string("osc") + std::to_string(_oscNum));
    }

    /// @brief Wait briefly for frequency lock, logging the result
    void _logLockedState();

    /// @brief Pathname for the device
    std::string _devName;

    /// @brief File descriptor for the opened device
    int _fd;

    /// @brief Integer ID for the oscillator, used for log messages
    uint _oscNum;

    /// @brief External reference frequency provided to the oscillator, Hz
    uint _refFreq;

    /// @brief The step increment for oscillator frequency, Hz
    uint _freqStep;

    /// @brief _scaledCurrentFreq = current frequency / _freqStep
    uint _scaledCurrentFreq;

    /// @brief _scaledMinFreq = minimum frequency / _freqStep
    uint _scaledMinFreq;

    /// @brief _scaledMaxFreq = maximum frequency / _freqStep
    uint _scaledMaxFreq;

    /// @brief Is the oscillator PLL locked?
    bool _locked;
};

#endif /* QM2010_OSCILLATOR_H_ */
