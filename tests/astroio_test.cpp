#include <iostream>
#include "../src/astroio.hpp"
#include "../src/utils.hpp"
#include "common.hpp"


std::string dataRootDir;


void test_from_dat_file(){
    auto voltages = Voltages::from_dat_file(dataRootDir + "/offline_correlator/1240826896_1240827191_ch146.dat", VCS_OBSERVATION_INFO, 100);
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
