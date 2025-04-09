#include <iostream>
#include <chrono>
#include "../src/astroio.hpp"
#include "../src/utils.hpp"
#include "common.hpp"


std::string dataRootDir;


void test_from_dat_file(){
    using namespace std::chrono;

    high_resolution_clock::time_point volt1_start = high_resolution_clock::now();
    auto voltages = Voltages::from_dat_file(dataRootDir + "/offline_correlator/1240826896_1240827191_ch146.dat", VCS_OBSERVATION_INFO, 100);
    high_resolution_clock::time_point volt1_stop = high_resolution_clock::now();
    //auto voltages_optim = Voltages::from_dat_file_optim(dataRootDir + "/offline_correlator/1240826896_1240827191_ch146.dat", VCS_OBSERVATION_INFO, 100);
    high_resolution_clock::time_point volt2_stop = high_resolution_clock::now();
    auto voltages_gpu = Voltages::from_dat_file_gpu(dataRootDir + "/offline_correlator/1240826896_1240827191_ch146.dat", VCS_OBSERVATION_INFO, 100);
    high_resolution_clock::time_point volt3_stop = high_resolution_clock::now();
    
    duration<double> volt1_dur = duration_cast<duration<double>>(volt1_stop - volt1_start);
    // // duration<double> volt2_dur = duration_cast<duration<double>>(volt2_stop - volt1_stop);
    duration<double> volt3_dur = duration_cast<duration<double>>(volt3_stop - volt2_stop);

    std::cout << "Original method took " << volt1_dur.count() << " seconds." << std::endl;
    // // std::cout << "New method took " << volt2_dur.count() << " seconds." << std::endl;
    std::cout << "GPU method took " << volt3_dur.count() << " seconds." << std::endl;
    voltages.to_cpu();
    voltages_gpu.to_cpu();
    if(voltages.size() != voltages_gpu.size())
        throw TestFailed("test_from_dat_file: voltage objects are not of the same size.");
    for(size_t i {0}; i < voltages.size(); i++){
        if(voltages[i] != voltages_gpu[i]){
            std::stringstream ss;
            ss << "test_from_dat_file: voltages[" << i << "] != voltages_gpu[" << i << "] - " << voltages[i] << " != " << voltages_gpu[i] << std::endl;
            throw TestFailed(ss.str().c_str());
        }
    }
    std::cout << "'test_from_dat_file' passed." << std::endl;
}



void test_from_memory(){
    char *input_char;
    size_t insize;
    read_data_from_file(dataRootDir + "/xGPU/input_array_128_128_128_100.bin", input_char, insize);
    ObservationInfo obsInfo {.nAntennas = 128, .nFrequencies = 128, .nPolarizations = 2, .nTimesteps=100};
    int8_t *input_data {reinterpret_cast<int8_t *>(input_char)};
    auto voltages = Voltages::from_memory(input_data, insize, obsInfo, 100);
    std::cout << "'test_from_memory' passed." << std::endl;
}



void test_simply_writing_and_reading_fits_file(){
    ObservationInfo obsInfo;
    obsInfo.nAntennas = 128;
    obsInfo.nFrequencies = 128;
    obsInfo.nPolarizations = 2;
    obsInfo.nTimesteps=100;
    obsInfo.timeResolution=0.0001;
    obsInfo.startTime=0;
    obsInfo.id="id";

    const unsigned int n_baselines {(obsInfo.nAntennas + 1) * (obsInfo.nAntennas / 2)};
    const size_t matrixSize {n_baselines * obsInfo.nPolarizations * obsInfo.nPolarizations};
    const size_t nIntervals {(obsInfo.nTimesteps + 100 - 1) / 100};
    const size_t nValuesInTimeInterval {matrixSize * obsInfo.nFrequencies};
    const size_t outSize {nValuesInTimeInterval * nIntervals};
    MemoryBuffer<std::complex<float>> xcorr {outSize};
    for(unsigned long long i {0}; i < outSize; i++){
        xcorr[i].real(i % 20 + 1);
        xcorr[i].imag(i % 20 + 1);
    }
    Visibilities v {std::move(xcorr), obsInfo, 100, 1};
    std::string tmpfile {dataRootDir + "/test_fits.bin.tmp"};
    v.to_fits_file(tmpfile);
    auto v2 = Visibilities::from_fits_file(tmpfile, obsInfo);

    if(v.size() != v2.size()) throw TestFailed("test_simply_writing_and_reading_fits_file: lengths differ!");
    
    for(unsigned long long i {0}; i < outSize; i++){
        if (v[i].real() != v2[i].real() ||  v[i].imag() != v2[i].imag()){
            throw TestFailed("test_simply_writing_and_reading_fits_file: elements differ!");
        }
    }
    std::cout << "'test_simply_writing_and_reading_fits_file' passed." << std::endl;
}



int main(void){
    char *pathToData {std::getenv(ENV_DATA_ROOT_DIR)};
    if(!pathToData){
        std::cerr << "'" << ENV_DATA_ROOT_DIR << "' environment variable is not set." << std::endl;
        return -1;
    }
    dataRootDir = std::string {pathToData};
    try{
        test_from_dat_file();
        test_from_memory();
        test_simply_writing_and_reading_fits_file();
    } catch (std::exception& ex){
        std::cerr << ex.what() << std::endl;
        return 1;
    }
    std::cout << "All tests passed." << std::endl;
}
