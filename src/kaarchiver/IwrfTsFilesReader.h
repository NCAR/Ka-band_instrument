/*
 * IwrfTsFilesReader.h
 *
 *  Created on: Apr 22, 2010
 *      Author: burghart
 */

#ifndef IWRFTSFILESREADER_H_
#define IWRFTSFILESREADER_H_

#include <QFile>
#include <QStringList>

// RAL IwrfTsPulse class
#include <radar/IwrfTsPulse.hh>

class IwrfTsFilesReader {
public:
    /**
     * Construct an IwrfTsFilesReader.
     * @param fileNames list of names of the files to be read
     * @param verbose if true, a message will be printed to std::cout as each 
     *     file is opened
     */
    IwrfTsFilesReader(const QStringList& fileNames, bool verbose = true);
    virtual ~IwrfTsFilesReader();
    /**
     * Get the next pulse from the file(s), as an IwrfTsPulse object.
     * Return true iff we successfully return a pulse. Any metadata preceding
     * the next pulse packet will also be read and incorporated into the
     * IwrfTsPulse.
     * @param pulse the IwrfTsPulse to be filled
     * @return true iff we return a good pulse
     */
    bool getNextIwrfPulse(IwrfTsPulse& pulse);
    
private:
    // Maximum number of IWRF time series gates we're prepared to handle...
    static const int MAX_GATES = 4096;
    
    /**
     * Open the next file in our list. Return true if the file was opened
     * successfully.
     * @return true iff a file is opened successfully
     */
    bool _openNextFile();
    
    /**
     * Return the name of the currently opened file, if any, else an 
     * empty string.
     * @return the name of the currently opened file, if any, else an empty string.
     */
    QString _currentFileName() {
        if (_currentFileNdx < 0)
            return "";
        else
            return _fileNames[_currentFileNdx];
    }
    
    /**
     * Our list of files, and index of the current file, and the current file.
     */
    QStringList _fileNames;
    int _currentFileNdx;
    QFile _tsFile;
    
    /**
     * If _verbose is true, we print a message to std::cout each time we open 
     * a file.
     */
    bool _verbose;
};

#endif /* IWRFTSFILESREADER_H_ */
