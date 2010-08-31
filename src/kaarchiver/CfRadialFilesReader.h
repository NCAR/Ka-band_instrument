/*
 * CfRadialFilesReader.h
 *
 *  Created on: Aug 18, 2010
 *      Author: burghart
 */

#ifndef CFRADIALFILESREADER_H_
#define CFRADIALFILESREADER_H_

#include <QFile>
#include <QStringList>

#include <Radx/RadxFile.hh>
#include <Radx/RadxVol.hh>

class CfRadialFilesReader {
public:
    /**
     * Construct an CfRadialFilesReader.
     * @param fileNames list of names of the files to be read
     * @param verbose if true, a message will be printed to std::cout as each 
     *     file is opened
     */
    CfRadialFilesReader(const QStringList& fileNames, bool verbose = true);
    virtual ~CfRadialFilesReader();
    /**
     * Get the next ray from the file(s), as a RadxRay object.
     * Return true iff we successfully return a ray.
     * @param ray the RadxRay to be filled
     * @return true iff we return a good ray
     */
    bool getNextRay(RadxRay & ray);
    
private:
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
    RadxFile _radxFile;
    
    /**
     * Volume read from the current file
     */
    RadxVol _radxVol;
    
    /**
     * If _verbose is true, we print a message to std::cout each time we open 
     * a file.
     */
    bool _verbose;
};

#endif /* CFRADIALFILESREADER_H_ */
