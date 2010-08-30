/*
 * Ts2PeiFile.cpp
 *
 *  Created on: Apr 22, 2010
 *      Author: burghart
 *      
 *  Read IWRF time series file(s) and write IQ data in Pei's simple format
 *  to stdout.
 */

#include "IwrfTsFilesReader.h"

#include <iostream>
#include <cstdlib>
#include <ctime>
#include <unistd.h> // for usleep()


int
main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " tsfile [tsfile...]" << std::endl;
        exit(1);
    }

    QStringList fileNames;
    for (int i = 1; i < argc; i++)
        fileNames << argv[i];
    IwrfTsFilesReader reader(fileNames, false);
    
    IwrfTsInfo iwrfInfo;
    IwrfTsPulse iwrfPulse(iwrfInfo);
    
    // For each pulse
    while (reader.getNextIwrfPulse(iwrfPulse)) {
        // Start with a metadata header:
        // 8-byte float     time in seconds since 1970-01-01 00:00:00 UTC
        // 4-byte float     PRF, Hz
        // 4-byte float     receiver pulse width (i.e., time per gate), s
        // 4-byte int       number of gates
        double ftime = iwrfPulse.getFTime();
        std::cout.write((char*)&ftime, sizeof(double));
        float prf = iwrfPulse.getPrf();
        std::cout.write((char*)&prf, sizeof(float));
        float pulseWidth = iwrfPulse.get_pulse_width_us() * 1.0e-6;
        std::cout.write((char*)&pulseWidth, sizeof(float));
        int nGates = iwrfPulse.getNGates();
        std::cout.write((char*)&nGates, sizeof(int));

        // Data:
        // 2-byte int   I[0]
        // 2-byte int   Q[0]
        // 2-byte int   I[1]
        // 2-byte int   Q[1]
        // ...
        // 2-byte int   I[nGates-1]
        // 2-byte int   Q[nGates-1]
        if (iwrfPulse.get_iq_encoding() != IWRF_IQ_ENCODING_SCALED_SI16) {
            std::cerr << "IWRF IQ encoding is " << iwrfPulse.get_iq_encoding() << 
                ", not IWRF_IQ_ENCODING_SCALED_SI16 @ time " << ftime << "!" << 
                std::endl;
            exit(1);
        }
        si16 *iqData = (si16*)iwrfPulse.getPackedData();
        for (int g = 0; g < nGates; g++) {
            // Write I and Q (4 bytes total) for this gate
            std::cout.write((char*)&(iqData[2 * g]), 4);
        }
    }
}
