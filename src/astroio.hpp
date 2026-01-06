#ifndef __ASTROIO_H__
#define __ASTROIO_H__

#include <cstring>
#include <fitsio.h>
#include <iostream>
#include <vector>
#include <exception>
#include <map>
#include <cmath>
#include <string>
#include <fstream>
#include <complex>

#include "FITS.hpp"
#include "memory_buffer.hpp"

enum class TelescopeID {MWA1, MWA2, MWA3, EDA2};
/**
 * @brief An observation is characterized by the telescope configuration, a start time and
 * and an identifier.
 */
struct ObservationInfo {
    unsigned int nAntennas;
    unsigned int nFrequencies;
    unsigned int nPolarizations;
    unsigned int nTimesteps;
    // Time resolution in seconds
    double timeResolution;
    // Fine channel frequency resolution in MHz
    double frequencyResolution;
    double coarseChannelBandwidth;
    // Time when the observation starts.
    time_t startTime;
    unsigned int coarseChannel;
    double geo_long_deg; // Geographic coordinates of the observatory LONGITUDE [degrees]
    double geo_lat_deg;  // LATITUDE [degrees]
    // Index of the coarse channel within the list of 24 coarse channels comprising a full
    // MWA observation.
    unsigned int coarse_channel_index;
    std::string id;
    TelescopeID telescope;
    std::string metadata_file;
    std::string calibration_solutions_file;
};


/**
 * @brief A path to a .dat file with associated information regarding the observation
 * it belongs to.
*/
using DatFile = std::pair<std::string, ObservationInfo>;


// Prefilled ObservationInfo structure for VCS data
extern const ObservationInfo VCS_OBSERVATION_INFO;

// Prefilled ObservationInfo structure for EDA2 data
extern const ObservationInfo EDA2_OBSERVATION_INFO;
/**
 * @brief Voltage data making up an observation recorded by a radiotelescope.
 * 
 * Data is stored as an array `data` of 16-bit complex samples, 8 bits for the real
 * part and 8 bits for the complex one. Telescope and observation characteristics 
 * are collected into the `obsInfo` attribute. Data layout facilitates the integration
 * operation over `nIntegrationSteps` time steps. In particular, the array data represents
 * a multidimentional matrix whose dimensions are
 *  [integration_interval][frequency][antenna][polarization][time_step].
 */
class Voltages : public MemoryBuffer<std::complex<int8_t>> {

    public:
    ObservationInfo obsInfo;
    unsigned int nIntegrationSteps;

    Voltages& operator=(Voltages&& other){
        MemoryBuffer::operator=(std::move(other));
        obsInfo = other.obsInfo;
        nIntegrationSteps = other.nIntegrationSteps;
        return *this;
    }

    Voltages& operator=(Voltages& other){
        if(this == &other) return *this;
        MemoryBuffer::operator=(other);
        obsInfo = other.obsInfo;
        nIntegrationSteps = other.nIntegrationSteps;
        return *this;
    }

    Voltages(MemoryBuffer<std::complex<int8_t>>&& data, const ObservationInfo& obsInfo, unsigned int nIntegrationSteps) : MemoryBuffer {std::move(data)} {
        this->obsInfo = obsInfo;
        this->nIntegrationSteps = nIntegrationSteps;
    }

    Voltages(const Voltages& other) : MemoryBuffer {other} {
        obsInfo = other.obsInfo;
        nIntegrationSteps = other.nIntegrationSteps;
    }


    Voltages(Voltages&& other) : MemoryBuffer {std::move(other)} {
        obsInfo = other.obsInfo;
        nIntegrationSteps = other.nIntegrationSteps;
    }
    
    /**
     * Return the number of complex samples in the array.
     */
    size_t size() const {
        return static_cast<size_t>(obsInfo.nPolarizations) * obsInfo.nAntennas * obsInfo.nFrequencies * obsInfo.nTimesteps;
    }

    /**
     * Read voltage data from a .dat file.
     * Data in the file is ordered according to the following axes, from the slowest to the fastest:
     *      [time][channel][station][polarization][complexity]
     * When reading the file, this method will also reorder the data such that the final layout is
     *      [time_interval][channel][station][polarization][complexity][integration_step]
     * where the number of time intervals is ceil(time / nIntegrationSteps)
     * 
     * @param filename: path to the .dat file.
     * @param obsInfo; metadata information regarding the obervation. For VCS data, you can use the constant
     * VCS_OVSERVATION_INFO.
     * @param nIntegrationSteps: number of timesteps to integrate over when/if data will be correlated.
     * @param edge: set to zero `edge` channels at the top and the bottom of the frequency band.
     * @param timestepsPerRead: number of timesteps o read from the file at each read call. Might be useful
     * to optimise memory consumption.
     * @return A new instance of the Voltage class.
     */
    static Voltages from_dat_file(const std::string& filename, const ObservationInfo& obsInfo, unsigned int nIntegrationSteps);

    static Voltages from_dat_file_gpu(const std::string& filename, const ObservationInfo& obsInfo, unsigned int nIntegrationSteps);
    /**
     * Read voltage data from a memory buffer.
     * Data in memory is ordered according to the following axes, from the slowest to the fastest:
     *      [time][channel][station][polarization][complexity]
     * Unlike the .dat file, we expect that each real sample is of type int8_t.
     * When reading the file, this method will also reorder the data such that the final layout is
     *      [time_interval][channel][station][polarization][integration_step][complexity]
     * where the number of time intervals is ceil(time / nIntegrationSteps)
     * 
     * @param buffer: memory buffer where to read data from.
     * @param length: number of items in the buffer.
     * @param obsInfo; metadata information regarding the obervation. For VCS data, you can use the constant
     * VCS_OVSERVATION_INFO.
     * @param nIntegrationSteps: number of timesteps to integrate over when/if data will be correlated.
     * @param use_pinned_mem: if GPU support is enabled, gives the option to pin CPU memory for fast memory
     * transfers to GPU.
     * @return A new instance of the Voltage class.
     * 
     * TODO: check if we need the edge feature.
     */
    static Voltages from_memory(const int8_t *buffer, size_t length, const ObservationInfo& obsInfo, unsigned int nIntegrationSteps);



    /**
     * Read EDA2 voltage data from a binary dump of the corresponding HDF5 file.
     * (This is mainly used for testing purposes, we should probably read the HDF5 file directly)
    */
    static Voltages from_eda2_file(const std::string& filename, const ObservationInfo& obs_info, unsigned int nIntegrationSteps);

};


/**
 * @brief Correlated voltages, also known as visibilities.
 * 
 * Obtained by cross correlating voltages, visibilities are then typically subject to an averaging
 * over time and frequency. This information is stored in the `nIntegrationSteps` and
 * `nAveragedChannels` class attributes. The latter indicates how many contiguous channels are
 * averaged to reduce the original number of frequencies to `nFrequecies` in the final `data` array.
 * 
 * The array `data` 
 */
class Visibilities : public MemoryBuffer<std::complex<float>> {
    public:
    ObservationInfo obsInfo;
    unsigned int nIntegrationSteps;
    unsigned int nAveragedChannels;
    unsigned int nFrequencies;

    Visibilities(MemoryBuffer<std::complex<float>>&& data, const ObservationInfo& obsInfo, unsigned int nIntegrationSteps,
            unsigned int nAveragedChannels) : MemoryBuffer {std::move(data)} {
        this->obsInfo = obsInfo;
        this->nIntegrationSteps = nIntegrationSteps;
        this->nAveragedChannels = nAveragedChannels;
        this->nFrequencies = obsInfo.nFrequencies / nAveragedChannels;
    }

    Visibilities(const Visibilities& other) : MemoryBuffer {other} {
        obsInfo = other.obsInfo;
        nIntegrationSteps = other.nIntegrationSteps;
        nFrequencies = other.nFrequencies;
        nAveragedChannels = other.nAveragedChannels;
    }

    Visibilities(Visibilities&& other) : MemoryBuffer {std::move(other)} {
        obsInfo = other.obsInfo;
        nIntegrationSteps = other.nIntegrationSteps;
        nFrequencies = other.nFrequencies;
        nAveragedChannels = other.nAveragedChannels;
    }

    Visibilities& operator=(Visibilities& other){
        if(this == &other) return *this;
        MemoryBuffer::operator=(other);
        obsInfo = other.obsInfo;
        nIntegrationSteps = other.nIntegrationSteps;
        nFrequencies = other.nFrequencies;
        nAveragedChannels = other.nAveragedChannels;
        return *this;
    }

    Visibilities& operator=(Visibilities&& other){
        if(this == &other) return *this;
        obsInfo = other.obsInfo;
        nIntegrationSteps = other.nIntegrationSteps;
        nFrequencies = other.nFrequencies;
        nAveragedChannels = other.nAveragedChannels;
        MemoryBuffer::operator=(std::move(other));
        return *this;
    }


    std::complex<float> *at(unsigned int interval, unsigned int frequency, unsigned int a1, unsigned a2){
        const size_t nValuesInTimeInterval {this->matrix_size() * nFrequencies};
        // TODO: use actual frequency instead of a "frequency index". To do so, information must
        // be provided by the user.
        std::complex<float> *pData = this->data() + nValuesInTimeInterval * interval + this->matrix_size() * frequency;
        unsigned int min_a, max_a;
        if(a1 < a2) {
            min_a = a1;
            max_a = a2;
        }else{
            min_a = a2;
            max_a = a1;
        }
        unsigned int baseline = (max_a * (max_a + 1))/2 + min_a;
        pData += this->obsInfo.nPolarizations * this->obsInfo.nPolarizations * baseline;
        return pData;
    }


    std::complex<float> *at(unsigned int interval, unsigned int frequency, unsigned int baseline){
        const size_t nValuesInTimeInterval {this->matrix_size() * nFrequencies};
        // TODO: use actual frequency instead of a "frequency index". To do so, information must
        // be provided by the user.
        std::complex<float> *pData = this->data() + nValuesInTimeInterval * interval + this->matrix_size() * frequency;
        pData += this->obsInfo.nPolarizations * this->obsInfo.nPolarizations * baseline;
        return pData;
    }

    /**
     * Number of time intervals integrated over by the correlator.
     */
    size_t integration_intervals() const {
        return (obsInfo.nTimesteps + nIntegrationSteps - 1) / nIntegrationSteps;
    }
    
    // Number of complex visibilities in one frequency channel.
    size_t matrix_size() const {
        const size_t n_baselines {((obsInfo.nAntennas + 1) * obsInfo.nAntennas) / 2};
        return n_baselines * obsInfo.nPolarizations * obsInfo.nPolarizations;
    }

    // Number of complex visibilities in the whole `data` array.
    size_t size() const {
        const size_t nValuesInTimeInterval {this->matrix_size() * nFrequencies};
        return this->integration_intervals() * nValuesInTimeInterval;
    }
    
    /**
     * @brief Save visibilities to a FITS file on disk.
     * 
     * @param filename name of the output file.
     */
    void to_fits_file(const std::string& filename) const;


    /**
     * @brief Save visibilities to a FITS file on disk using MWAX format.
     * 
     * @param filename name of the output file.
     */
    void to_fits_file_mwax(const std::string& filename, int coarse_channel_idx) const;


    /**
     * @brief Load visibilities from a FITS file.
     * 
     * @param filename path to the FITS file to read visibilities from.
     * @param oInfo Information about the observation. Default assumes data come from the MWA VCS dataser.
     * @return Visibilities instance.
     */
    static Visibilities from_fits_file(const std::string& filename, const ObservationInfo &oInfo = VCS_OBSERVATION_INFO);
};


/**
 * @brief Extract information, such as obsid, coarse channel and timestamp, contained in the name 
 * of the .dat file where MWA Phase I voltages are stored.
 * @param file_path path to the .dat file.
 * @return `obs_info` - An ObservationInfo object containing information relative to the voltages
 * stored in the .dat file.
*/
ObservationInfo parse_mwa_phase1_dat_file_info(const std::string& file_path);


/**
 * @brief 
 * 
 * @param file_list: the list of paths to .dat files making up on or more MWA observations to be 
 * processed. The files will be sorted by observation ID and then timestamp. Consecutive 24 .dat
 * files make up a second of observation over the entire MWA frequency bandwidth and will be
 * processed together. Hence, the total number of files must be a multiple of 24.
 * @todo Ideally we want to return a structured output that partitions input by observation and
 * and then by groups of 24 files each representing 1 sencond of observation. Assume now a single
 * observation, single 24 files.
*/
std::vector<std::vector<DatFile>> parse_mwa_dat_files(std::vector<std::string>& file_list);

/**
 * @brief Get all the dat files in a directory. Optionally, only select the dat files corresponding
 * to a certain range of seconds, specified with an offset from the first second and a count.
 * 
 * @param directory: path to the directory containing the .dat files
 * @param offset: offset in number of seconds from the start of the observation.
 * @param count: number of seconds to consider, starting from the offset.
 * @todo Ideally we want to return a structured output that partitions input by observation and
 * and then by groups of 24 files each representing 1 sencond of observation. Assume now a single
 * observation, single 24 files.
*/
std::vector<std::vector<DatFile>> parse_mwa_dat_files(std::string directory, int start_second = 0, int count = -1);
#endif
