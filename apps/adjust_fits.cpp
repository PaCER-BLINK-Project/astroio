/**
 * This program transforms a FITS file produced by the FITS C++ class in AstroIO to make it
 * as if it were produced by offline_correlator.
 * 
 * The reason this is needed is that offline_correlator informs the cfitsio library that it
 * is going to write integer values (BITPIX = LONG_IMG = 32) but then writes TFLOAT.
*/
#include <iostream>
#include <fstream>
#include "../src/FITS.hpp"

inline void print_fits_error(int errorCode){
    char statusStr[FLEN_STATUS], errmsg[FLEN_ERRMSG];
    fits_get_errstatus(errorCode, statusStr);
    std::cerr << "Error occurred during a cfitsio call.\n\tCode: " << errorCode << ": " << std::string {statusStr} << std::endl;
    // get all the messages
    while(fits_read_errmsg(errmsg))
        std::cerr << "\t" << std::string {errmsg} << std::endl;
}


#define CHECK_FITS_ERROR(X)({\
    if((X)){\
        print_fits_error(status);\
        throw std::exception();\
    }\
})


int main(int argc, char **argv){

    if(argc < 3){
        std::cout << argv[0] << " <AstroIO input FITS> <output file>" << std::endl;
        return 0;
    }

    auto myFITSImage = FITS::from_file(argv[1]);

    std::ifstream fp {argv[2]};
    // remove file if exists already - we overwrite by default.
    if(fp.good()){
        fp.close();
        std::remove(argv[2]);
    }
    fitsfile *fitsFP;
    int status = 0;
    long axes[2];
    CHECK_FITS_ERROR(fits_create_file(&fitsFP, argv[2], &status));
    for(FITS::HDU& cHDU : myFITSImage){
        status = 0;
        axes[0] = cHDU.get_ydim();
        axes[1] = cHDU.get_xdim();
        CHECK_FITS_ERROR(fits_create_img(fitsFP, LONG_IMG, 2,  axes, &status));
        long fPixel[2] {1, 1};
        CHECK_FITS_ERROR(fits_write_pix(fitsFP, TFLOAT, fPixel, axes[0] * axes[1], (char *) cHDU.get_image_data(), &status));
        for(auto& header_entry : cHDU.get_header()){
            auto key = header_entry.first;
	    auto entry = header_entry.second;
            status = 0;
            if(entry.data_type == TSTRING){
                CHECK_FITS_ERROR(fits_update_key(fitsFP, entry.data_type, key.c_str(), entry.data.sval, entry.comment.c_str(), &status));
            }else{
                CHECK_FITS_ERROR(fits_update_key(fitsFP, entry.data_type, key.c_str(), &entry.data, entry.comment.c_str(), &status));
            }
        }
    }
    CHECK_FITS_ERROR(fits_close_file(fitsFP, &status));
    return 0;
}
