/*
 * ProductArchiver.h
 *
 *  Created on: May 5, 2010
 *      Author: burghart
 */

#ifndef PRODUCTARCHIVER_H_
#define PRODUCTARCHIVER_H_

#include <ProductReader.h>

#include <Radx/RadxFile.hh>
#include <Radx/RadxVol.hh>


/// ProductArchiver class
/// Collect incoming product data via the given DDSSubscriber and write 
/// CFRadial netCDF files containing the product data.
class ProductArchiver : public ProductReader {
public:
    /// ProductArchiver constructor
    /// @param subscriber the associated DDSSubscriber
    /// @param topicName the DDS topic name for incoming product data
    /// @param dataDir the destination directory for output files
    /// @param raysPerFile the desired ray count for output files
    /// @param fileFormat the Radx::file_format_t file format for output files,
    ///     e.g., RadxFile::FILE_FORMAT_CFRADIAL (see Radx/RadxFile.hh)
    ProductArchiver(DDSSubscriber& subscriber, std::string topicName,
            std::string dataDir, uint raysPerFile,
            RadxFile::file_format_t fileFormat);
    virtual ~ProductArchiver();

    /// Implement DDSReader::notify(), which will be called
    /// whenever new samples are added to the DDSReader available
    /// queue. Process the samples here.
    virtual void notify();

    /// Return the number of bytes written
    /// @return the number of bytes written
    int bytesWritten() const { return _bytesWritten; }
    
    /// Return the number of rays read
    /// @return the number of rays read
    int raysRead() const { return _raysRead; }
    
    /// Return the number of rays written
    /// @return the number of rays written
    int raysWritten() const { return _raysWritten; }

private:
    // Write out the volume stored in _radxVol, clear _radxVol, and
    // increment the volume counter
    void _writeCurrentVolume();
    
    // Initialize our volume
    void _initVolume();
    
    // destination directory
    std::string _dataDir;
    
    // number of rays to write per file
    uint _raysPerFile;
    
    // Our RadxFile object, for writing output files
    RadxFile _radxFile;
    
    // Our RadxVolume
    RadxVol _radxVol;
    
    // How many rays have we read?
    int _raysRead;

    // How many rays have we written?
    int _raysWritten;
    
    // How many bytes have we written?
    int _bytesWritten;
    
    // Mutex for thread-safe access to our members
    ACE_Recursive_Thread_Mutex _mutex;
};

#endif /* PRODUCTARCHIVER_H_ */
