#! /bin/env python
#
# Print the system clock offset in seconds relative to chronyd's current sync
# source. 
#
# Print an error message and exit with a non-zero status if chronyd is not
# responding or is not currently synchronized to a source.

import re
import subprocess
import sys

def main():
    # Run 'chronyc sources' to get the latest source info from chronyd
    try:
        sources_output = subprocess.check_output(['chronyc', 'sources'])
    except subprocess.CalledProcessError as err:
        exitOnCalledProcessError(err)
    
    # The output from 'chronyc sources' will normally look something like:
    #
    #   210 Number of sources = 1
    #   MS Name/IP address         Stratum Poll Reach LastRx Last sample
    #   ===============================================================================
    #   ^* timeserver.ka.net             1   6   177    29   -489us[ -494us] +/- 9755us
    #
    # We just want to confirm that chronyd is currently synchronized to a
    # source, which is indicated on the source's line with a '*' in the second
    # character.
    synchronized = False
    
    lines = sources_output.split('\n')
    for line in lines:
        if re.match('^.\*', line):
            synchronized = True
            break
    
    if not synchronized:
        print('chronyd is not currently synchronized to a source');
        sys.exit(1)
    
    # chronyd is synchronized to a source, so print the offset as reported by
    # 'chronyc tracking'. Output looks like:
    #
    #   Reference ID    : 192.168.1.5 (timeserver.ka.net)
    #   Stratum         : 2
    #   Ref time (UTC)  : Thu Jun  8 22:14:08 2017
    #   System time     : 0.000009816 seconds slow of NTP time
    #   Last offset     : -0.000004181 seconds
    #   RMS offset      : 0.000152735 seconds
    #   Frequency       : 78.268 ppm slow
    #   Residual freq   : -0.001 ppm
    #   Skew            : 0.085 ppm
    #   Root delay      : 0.002835 seconds
    #   Root dispersion : 0.007340 seconds
    #   Update interval : 65.0 seconds
    #   Leap status     : Normal
    #
    # and we just want the number from the "Last offset" line. 
    try:
        tracking_output = subprocess.check_output(['chronyc', 'tracking'])
    except subprocess.CalledProcessError as err:
        exitOnCalledProcessError(err)
    
    lines = tracking_output.split('\n')
    for line in lines:
        if re.match('^Last offset +: ', line):
            # Split the line by whitespace, then take the 4th field to get the
            # offset value (in seconds)
            offsetSeconds = line.split()[3]
            # Print the system clock offset in seconds
            print(offsetSeconds)
            break;

def exitOnCalledProcessError(err):
    """Log a message and exit on receipt of a subprocess.CalledProcessError

    Args:
        err (subprocess.CalledProcessError): The received error
    """
    print('Error executing "' + ' '.join(err.cmd) + '": ' + err.output)
    sys.exit(1)

if __name__ == '__main__':
    main()