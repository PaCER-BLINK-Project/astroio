#include <string>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <cstdio>
#include <algorithm>
#include "utils.hpp"
#include "astroio.hpp"


extern const ObservationInfo VCS_OBSERVATION_INFO {
    .nAntennas = 128u,
    .nFrequencies = 128u,
    .nPolarizations = 2u,
    .nTimesteps = 10000u,
    .timeResolution = 0.0001, // in seconds
    .startTime = 1313388762,
    .coarseChannel = 20
};


extern const ObservationInfo EDA2_OBSERVATION_INFO {
    .nAntennas = 256u,
    .nFrequencies = 1u,
    .nPolarizations = 2u,
    .nTimesteps = 262144,
    .timeResolution = 1.08e-6
};



namespace {

    int8_t eightBitLookup[65536][4] {};
    bool lookupInitialized {false};


    void build_eight_bit_lookup(){
        uint32_t index = 0;
        int value = 0;
        uint8_t original = 0;
        uint8_t answer = 0;
        uint8_t outval = 0;

        for (index=0; index <= 65535; index++){
            for (outval = 0; outval < 4 ; outval++){
                original = index >> (outval * 4);
                original = original & 0xf; // the sample
                if(original >= 0x8) { // it is a negative number
                    // https://en.wikipedia.org/wiki/Two%27s_complement#Subtraction_from_2N
                    value = original - 0x10;
                }
                else {
                    value = original;
                }
                answer = value & 0xff;
                eightBitLookup[index][outval] = answer;
            }
        }
    }
}



Voltages Voltages::from_dat_file(const std::string& filename, const ObservationInfo& obsInfo, unsigned int nIntegrationSteps, bool use_pinned_mem){
    // TODO: fix edge usage.
    const unsigned int edge {0}, timestepsPerRead {100u};
    std::ifstream fin;
    fin.open(filename, std::ios::binary);
    if(!fin && fin.gcount() == 0){
        std::cerr << "Error happened when reading the input file." << std::endl;
        throw std::exception();
    }
    if(!lookupInitialized) build_eight_bit_lookup();
    const size_t bytesPerComplexSample {1}; // 4+4 bits 
    const size_t nSamplesInTimestep {obsInfo.nFrequencies * obsInfo.nAntennas *  obsInfo.nPolarizations};
    const size_t bytesPerTimestep {nSamplesInTimestep * bytesPerComplexSample};
    const size_t bytesPerRead {timestepsPerRead * bytesPerTimestep};
    char* buffer {new char[bytesPerRead]};
    // We are going to read 2 complex samples at a time, one for each polarization of an antenna.
    // Each sample is made of 2 4bit data points, that need to be expanded to 8bit data points to be
    // processed.
    int8_t expanded[4]; // Temp variable for doing the 4-bit expansion
    fin.read(buffer, bytesPerRead);
    long long bytesRead {fin.gcount()};

    // variables used for output indexing
    const size_t samplesInPol {nIntegrationSteps};
    const size_t samplesInAntenna {samplesInPol * obsInfo.nPolarizations};
    const size_t samplesInFrequency {samplesInAntenna * obsInfo.nAntennas};
    const size_t samplesInTimeInterval {samplesInFrequency * obsInfo.nFrequencies};
    const size_t nIntegrationIntervals {(obsInfo.nTimesteps + nIntegrationSteps - 1)/ nIntegrationSteps };
    /*
        We allocate slightly more memory than simply nComplexSamples so we can avoid dealing with
        the boundary condition happening when obsInfo.nTimesteps % nIntegrationSteps != 0. 
    */
    MemoryBuffer<std::complex<int8_t>> mbVoltages {nIntegrationIntervals * samplesInTimeInterval, use_pinned_mem, false};
    auto voltages = mbVoltages.data();
    memset(voltages, 0, sizeof(std::complex<int8_t>) * nIntegrationIntervals * samplesInTimeInterval);

    size_t currentTimeInterval;
    size_t currentIntegratorStep;
    size_t total_timesteps {0};
    while(bytesRead == bytesPerRead){
        size_t sample_idx {0};
        for(size_t ts = 0; ts < timestepsPerRead; ts++, total_timesteps++){
            currentTimeInterval = total_timesteps / nIntegrationSteps;
            currentIntegratorStep = total_timesteps % nIntegrationSteps;
            for(size_t ch = 0; ch < obsInfo.nFrequencies; ch++){
                for(size_t a = 0; a < obsInfo.nAntennas; a++){
                    // set edge channels to 0
                    if(ch < edge || ch >= (obsInfo.nFrequencies - edge)){
                        for (size_t r = 0; r < 4; r++)
                            expanded[r] = 0;
                    }else{
                        uint16_t rawSamples = *reinterpret_cast<uint16_t*>(&buffer[sample_idx]);
                        memcpy(expanded, eightBitLookup[rawSamples], 4);
                    }
                    // output layout is Time, Frequency, Antenna, Polarization, Integration Step
                    size_t outIndex = currentTimeInterval * samplesInTimeInterval + ch * samplesInFrequency + a * samplesInAntenna;
                    voltages[outIndex + currentIntegratorStep].real(expanded[0]);
                    voltages[outIndex + currentIntegratorStep].imag(expanded[1]);
                    voltages[outIndex + samplesInPol + currentIntegratorStep].real(expanded[2]);
                    voltages[outIndex + samplesInPol + currentIntegratorStep].imag(expanded[3]);
                    
                    sample_idx += 2; // advances 2 samples at a time
                }
            }
        }
        fin.read(buffer, bytesPerRead);
        bytesRead = fin.gcount();
    }
    fin.close();
    delete[] buffer;
    return Voltages {std::move(mbVoltages), obsInfo, nIntegrationSteps};
}




Voltages Voltages::from_memory(const int8_t *buffer, size_t length, const ObservationInfo& obsInfo, unsigned int nIntegrationSteps, bool use_pinned_mem){
    const size_t bytesPerComplexSample {2}; // 4+4 bits 
    const size_t nSamplesInTimestep {obsInfo.nFrequencies * obsInfo.nAntennas *  obsInfo.nPolarizations};
    const size_t nComplexSamples {obsInfo.nTimesteps * nSamplesInTimestep};
    const size_t samplesSize {nComplexSamples * bytesPerComplexSample};
    if(length != nComplexSamples * bytesPerComplexSample){
        std::cerr << "Error: unexpected buffer size (" << length << "). Expected " <<  samplesSize << std::endl;
        throw std::exception(); // TODO bette exception.
    }
    size_t samplesInPol {nIntegrationSteps};
    const size_t samplesInAntenna {samplesInPol * obsInfo.nPolarizations};
    const size_t samplesInFrequency {samplesInAntenna * obsInfo.nAntennas};
    const size_t samplesInTimeInterval {samplesInFrequency * obsInfo.nFrequencies};
    size_t currentTimeInterval;
    size_t currentIntegratorStep;
    const size_t nIntegrationIntervals {(obsInfo.nTimesteps + nIntegrationSteps - 1)/ nIntegrationSteps };
    size_t sample_idx {0};
    /*
        We allocate slightly more memory than simply nComplexSamples so we can avoid dealing with
        the boundary condition happening when obsInfo.nTimesteps % nIntegrationSteps != 0. 
    */
    MemoryBuffer<std::complex<int8_t>> mbVoltages {nIntegrationIntervals * samplesInTimeInterval, use_pinned_mem, false};
    auto voltages = mbVoltages.data();
    memset(voltages, 0, sizeof(std::complex<int8_t>) * nIntegrationIntervals * samplesInTimeInterval);
    for(size_t ts = 0; ts < obsInfo.nTimesteps; ts++){
        currentTimeInterval = ts / nIntegrationSteps;
        currentIntegratorStep = ts % nIntegrationSteps;
        for(size_t ch = 0; ch < obsInfo.nFrequencies; ch++){
            for(size_t a = 0; a < obsInfo.nAntennas; a++){
                // output layout is Time, Frequency, Antenna, Polarization, Integration Step
                 size_t outIndex = currentTimeInterval * samplesInTimeInterval + ch * samplesInFrequency + a * samplesInAntenna;
                voltages[outIndex + currentIntegratorStep].real(buffer[sample_idx++]);
                voltages[outIndex + currentIntegratorStep].imag(buffer[sample_idx++]);
                voltages[outIndex + samplesInPol + currentIntegratorStep].real(buffer[sample_idx++]);
                voltages[outIndex + samplesInPol + currentIntegratorStep].imag(buffer[sample_idx++]);         
            }
        }
    }
    return {std::move(mbVoltages), obsInfo, nIntegrationSteps};
}



Voltages Voltages::from_eda2_file(const std::string& filename, const ObservationInfo& obs_info, unsigned int nIntegrationSteps,
        bool use_pinned_mem){
    // TODO: more efficient implementation
    char *buffer {nullptr};
    size_t size {0};
    read_data_from_file(filename, buffer, size);
    auto volt = Voltages::from_memory(reinterpret_cast<int8_t*>(buffer), size, obs_info, nIntegrationSteps, use_pinned_mem);
    delete[] buffer;
    return volt;
}


Visibilities Visibilities::from_fits_file(const std::string& filename, const ObservationInfo& oInfo){

    FITS fitsImage {FITS::from_file(filename)};
    ObservationInfo obsInfo {oInfo};
    const unsigned int n_baselines {(obsInfo.nAntennas + 1) * (obsInfo.nAntennas / 2)};
    const size_t matrixSize {n_baselines * obsInfo.nPolarizations * obsInfo.nPolarizations};

    size_t nHDUs {fitsImage.size()};

    unsigned int nIntegrationIntervals {static_cast<unsigned int>(nHDUs)}, nAveragedChannels;
    unsigned int nIntegrationSteps {obsInfo.nTimesteps / nIntegrationIntervals};
    
    size_t xcorrSize {obsInfo.nFrequencies * matrixSize * nIntegrationIntervals};

    MemoryBuffer<std::complex<float>> mbXcorr {xcorrSize, false, false};
    auto xcorr = mbXcorr.data();
    for(size_t idx {0}; idx < nHDUs; idx++){
        auto hdu = fitsImage[idx];
        if(idx == 0){
            // TODO what about the following info
            int msElapsed;
            float integrationTime;
            std::string comment;
            std::tie(obsInfo.startTime, comment) = hdu.get_keyword<long>("TIME");
            std::tie(msElapsed, comment) = hdu.get_keyword<int>("MILLITIM");
            std::tie(integrationTime, comment) = hdu.get_keyword<float>("INTTIME");
            std::tie(obsInfo.coarseChannel, comment) = hdu.get_keyword<unsigned int>("COARSE_CHAN");
            if(hdu.get_ydim() != matrixSize * 2){
                std::cerr << "Axis 1 is wrong. Value returned is " << hdu.get_ydim() << " instead of " << (matrixSize * 2) << std::endl;
                throw std::exception();
            }
            nAveragedChannels = obsInfo.nFrequencies / hdu.get_xdim();
        }
        // read data
        float *pMatrixOut {reinterpret_cast<float*>(xcorr) + idx * hdu.get_xdim() * hdu.get_ydim()};
        memcpy(pMatrixOut, hdu.get_image_data(), hdu.get_xdim() * hdu.get_ydim() * sizeof(float));
    }
    return Visibilities{std::move(mbXcorr), obsInfo, nIntegrationSteps, nAveragedChannels};
}



void Visibilities::to_fits_file(const std::string& filename) const{
    FITS fitsImage;
    const size_t nFrequencies {obsInfo.nFrequencies / nAveragedChannels}; 
    // one axis for matrix, one for frequency
    float integrationTime {static_cast<float>(obsInfo.timeResolution * nIntegrationSteps)};
    for(unsigned int interval {0}; interval < this->integration_intervals(); interval++){
        FITS::HDU hdu;
        std::complex<float>* pToMatrix = const_cast<std::complex<float>*>(this->data() + interval * (nFrequencies * this->matrix_size()));
        int msElapsed {static_cast<int>(interval *  (obsInfo.timeResolution * nIntegrationSteps * 1e3))};
        hdu.set_image(reinterpret_cast<float*>(pToMatrix),  static_cast<long>(nFrequencies), static_cast<long>(this->matrix_size()) * 2);
        hdu.add_keyword("TIME", static_cast<long>(obsInfo.startTime), "Unix time (seconds)");
        hdu.add_keyword("MILLITIM", msElapsed, "Milliseconds since TIME");
        hdu.add_keyword("INTTIME", integrationTime, "Integration time (s)");
        hdu.add_keyword("COARSE_CHAN", obsInfo.coarseChannel, "Receiver Coarse Channel Number (only used in offline mode)");
        fitsImage.add_HDU(hdu);
    }
    fitsImage.to_file(filename);
}



/**
 * @brief Extract information, such as obsid, coarse channel and timestamp, contained in the name 
 * of the .dat file where MWA Phase I voltages are stored.
 * @param file_path path to the .dat file.
 * @return `obs_info` - An ObservationInfo object containing information relative to the voltages
 * stored in the .dat file.
*/
ObservationInfo parse_mwa_phase1_dat_file_info(const std::string& file_path){
    ObservationInfo obs_info {VCS_OBSERVATION_INFO};
    std::string filename {file_path.substr(file_path.find_last_of('/') + 1)};
    size_t delimiter {filename.find('_')};
    std::string obs_id {filename.substr(0, delimiter)};
    time_t gps_time {atoll(filename.substr(delimiter + 1,
        filename.find_last_of('_') - delimiter - 1).c_str())};
    // position of the first digit of the coarse channel number. Reuse the
    // delimiter variable.
    delimiter = filename.find_last_of('_') + 3;
    size_t dot_position = filename.find('.');
    int coarse_channel {atoi(filename.substr(delimiter, dot_position - delimiter).c_str())};
    obs_info.id = obs_id;
    obs_info.startTime = gps_to_unix(gps_time);
    obs_info.coarseChannel = coarse_channel;
    return obs_info;
}



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
std::vector<std::vector<DatFile>> parse_mwa_dat_files(std::vector<std::string>& file_list){
    if(file_list.size() % 24 != 0) throw std::invalid_argument {
        "parse_mwa_dat_files: total number of files is not a multiple of 24."};
    std::sort(file_list.begin(), file_list.end());

    // Each observation is a list of .dat files.
    std::string current_observation_id {};
    time_t current_second {0ull};
    
    std::vector<std::vector<DatFile>> observation {};
    std::vector<DatFile> one_second_data {};

    for(const auto& file_path : file_list){
        ObservationInfo obs_info {parse_mwa_phase1_dat_file_info(file_path)};
        if(obs_info.id != current_observation_id){
            // Finished loading the current observation. Proceed to save it
            // in the array and get ready for the next one.
            // For now, enforce parsing only one observation.
            // TODO: handle mupliple observations.
            if(current_observation_id != std::string {}) throw std::invalid_argument {
                "read_mwa_dat_files: cannot read multiple observations."};
            current_observation_id = obs_info.id;
        }
        if(current_second != obs_info.startTime){
            if(current_second > 0ll){
                // Finished loading one second of data (24 files).
                if(one_second_data.size() != 24) throw std::invalid_argument {
                    "read_mwa_dat_files: one second of data missing .dat files."};
                observation.push_back(one_second_data);
                one_second_data.clear();
            }
            current_second = obs_info.startTime;
        }
        one_second_data.push_back({file_path, obs_info});
    }
    // Add the last second of data to be listed.
    if(one_second_data.size() != 24) throw std::invalid_argument {
        "read_mwa_dat_files: one second of data missing .dat files."};
    observation.push_back(one_second_data);
    return observation;
}

