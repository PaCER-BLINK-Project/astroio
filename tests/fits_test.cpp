#include <iostream>
#include <stdexcept>

#include "common.hpp"
#include "../src/FITS.hpp"


std::string dataRootDir;


void test_fits_equal(){
    char data[] {1, 2, 3, 4};
    FITS::HDU newHDU;
    newHDU.set_image(data, 2, 2);
    
    FITS::HDU newOtherHDU;
    newOtherHDU.set_image(data, 2, 2);
    
    if(!(newOtherHDU == newHDU)) throw TestFailed("Implementation of operator== for the FITS class failed.");
    std::cout << "'test_fits_equal' passed." << std::endl;
}


void test_write_read_simple_fits(){
    const std::string filename {"myTestFits.fits"};
    char data[] {1, 2, 3, 4};
    FITS myFITSImage;
    FITS::HDU newHDU;
    newHDU.add_keyword("BITPIXOO", 8, "My bitpix keyword.");
    newHDU.set_image(data, 2, 2);
    myFITSImage.add_HDU(std::move(newHDU));
    myFITSImage.to_file(filename);
    // Read back the same FITS file.
    auto myFITSImageAgain = FITS::from_file(filename);
    std::remove(filename.c_str());
    auto hdu = myFITSImageAgain[0];
    if(hdu.get_keyword<int>("BITPIXOO").first != 8){
        throw TestFailed("test_write_read_simple_fits: could not retrieve the same value for the 'BITPIXOO' keyword.");
    }
    if(hdu != newHDU) 
        throw TestFailed("Creating, writing and reading back again the same FITS file yield different results.");
    std::cout << "'test_write_read_simple_fits' passed." << std::endl;
}


int main(void){
    char *pathToData {std::getenv(ENV_DATA_ROOT_DIR)};
    if(!pathToData){
        std::cerr << "'" << ENV_DATA_ROOT_DIR << "' environment variable is not set." << std::endl;
        return -1;
    }
    dataRootDir = std::string {pathToData};
    try{
        test_fits_equal();
        test_write_read_simple_fits();
    } catch (std::exception& ex){
        std::cerr << ex.what() << std::endl;
        return 1;
    }
    
    std::cout << "All tests passed." << std::endl;
    return 0;
}
