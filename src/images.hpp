#ifndef __BLINK_IMAGES_H__
#define __BLINK_IMAGES_H__

#include <complex>
#include <string>
#include "astroio.hpp"
#include "memory_buffer.hpp"

class Images : public MemoryBuffer<std::complex<float>> {
    public:
    ObservationInfo obsInfo;
    unsigned int nIntegrationSteps;
    unsigned int nAveragedChannels;
    unsigned int nFrequencies;
    unsigned int side_size;
    double ra_deg, dec_deg;
    double pixscale_ra, pixscale_dec; // separate pixscales for RA,DEC due to different UV coverage (MAX(u) and MAX(v))

   Images(MemoryBuffer<std::complex<float>>&& data, const ObservationInfo& obsInfo, unsigned int nIntegrationSteps,
            unsigned int nAveragedChannels, unsigned int side_size) : MemoryBuffer {std::move(data)} {
        this->obsInfo = obsInfo;
        this->nIntegrationSteps = nIntegrationSteps;
        this->nAveragedChannels = nAveragedChannels;
        this->nFrequencies = obsInfo.nFrequencies / nAveragedChannels;
        this->side_size = side_size;
    }

    Images(const Images& other) : MemoryBuffer {other} {
        obsInfo = other.obsInfo;
        nIntegrationSteps = other.nIntegrationSteps;
        nFrequencies = other.nFrequencies;
        nAveragedChannels = other.nAveragedChannels;
        side_size = other.side_size;
    }

    Images(Images&& other) : MemoryBuffer {std::move(other)} {
        obsInfo = other.obsInfo;
        nIntegrationSteps = other.nIntegrationSteps;
        nFrequencies = other.nFrequencies;
        nAveragedChannels = other.nAveragedChannels;
        side_size = other.side_size;
    }

    Images& operator=(Images& other){
        if(this == &other) return *this;
        MemoryBuffer::operator=(other);
        obsInfo = other.obsInfo;
        nIntegrationSteps = other.nIntegrationSteps;
        nFrequencies = other.nFrequencies;
        nAveragedChannels = other.nAveragedChannels;
        side_size = other.side_size;
        return *this;
    }

    Images& operator=(Images&& other){
        MemoryBuffer::operator=(std::move(other));
        obsInfo = other.obsInfo;
        nIntegrationSteps = other.nIntegrationSteps;
        nFrequencies = other.nFrequencies;
        nAveragedChannels = other.nAveragedChannels;
        side_size = other.side_size;
        return *this;
    }

    #ifdef __GPU__
    __host__ __device__
    #endif
    std::complex<float> *at(unsigned int interval, unsigned int frequency) {
        const size_t nValuesInTimeInterval {image_size() * nFrequencies};
        std::complex<float> *pData = this->data() + nValuesInTimeInterval * interval + image_size() * frequency;
        return pData;
    }

    /**
     * Number of time intervals integrated over by the correlator.
     */
    #ifdef __GPU__
    __host__ __device__
    #endif
    size_t integration_intervals() const {
        return (obsInfo.nTimesteps + nIntegrationSteps - 1) / nIntegrationSteps;
    }
    
    // Number of pixels in a single image.
    #ifdef __GPU__
    __host__ __device__
    #endif
    size_t image_size() const {
       return side_size * side_size;
    }

    // Number of images `data` array.
    #ifdef __GPU__
    __host__ __device__
    #endif
    size_t size() const {
        return this->integration_intervals() * nFrequencies;
    }
    
    /**
     * @brief Save visibilities to a FITS file on disk.
     * 
     * @param filename name of the output file.
     */
    void to_fits_file(const std::string& filename, bool save_as_complex = false, bool save_imaginary = false);


   void to_fits_files(const std::string& directory_path, bool save_as_complex = false, bool save_imaginary = false);
};


#endif