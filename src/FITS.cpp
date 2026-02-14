#include <iostream>
#include <fstream>
#include <array>
#include <regex>
#include "FITS.hpp"


void print_fits_error(int errorCode){
    char statusStr[FLEN_STATUS], errmsg[FLEN_ERRMSG];
    fits_get_errstatus(errorCode, statusStr);
    std::cerr << "Error occurred during a cfitsio call.\n\tCode: " << errorCode << ": " << std::string {statusStr} << std::endl;
    // get all the messages
    while(fits_read_errmsg(errmsg))
        std::cerr << "\t" << std::string {errmsg} << std::endl;
}



inline bool is_special_keyword(const std::string& key){
    const std::array<std::string, 5> special_keywords {"SIMPLE", "BITPIX", "COMMENT", "EXTEND", "NAXIS"};
    for(auto& special : special_keywords)
        if(key == special) return true;

    static const std::regex naxis_regex {"^NAXIS[1-9][0-9]*$"};
    if(std::regex_search(key, naxis_regex)) return true;
    return false; 
}

FITS::FITS(std::string filename, Mode mode) : filename {filename}, open_mode {mode} {
    if(mode == Mode::READ)
        read();
    else if(mode == Mode::APPEND){
        int status = 0, nHDUs {-1};
        if(std::filesystem::exists(filename)){
            CHECK_FITS_ERROR(fits_open_file(&fitsFP, filename.c_str(), READWRITE, &status));
            CHECK_FITS_ERROR(fits_get_num_hdus(fitsFP, &nHDUs,  &status));
            if(nHDUs > 0) CHECK_FITS_ERROR(fits_movabs_hdu(fitsFP, nHDUs, NULL, &status));
        }else{
            CHECK_FITS_ERROR(fits_create_file(&fitsFP, filename.c_str(), &status));
        }
    }
}

FITS::~FITS() {
    if(fitsFP){
        int status = 0;
        fits_close_file(fitsFP, &status);
    }
}

void FITS::HDU::set_image(int bitpix, char *data, long xDim, long yDim){
    if(this->data) delete[] static_cast<char*>(this->data);
    this->data = data;
    this->bitpix = bitpix;
    axes[0] = xDim;
    axes[1] = yDim;
    switch (bitpix){
    case FLOAT_IMG: 
        this->datatype = TFLOAT;
        break;
    case DOUBLE_IMG:
        this->datatype = TDOUBLE;
        break;
    case BYTE_IMG:
        this->datatype = TBYTE;
        break;
    case LONG_IMG:
        this->datatype = TLONG;
        break;
    default:
        throw std::invalid_argument {"set_image: data type of first argument not recognised."};
        break;
    }
}

void FITS::HDU::set_dynspec_wcs_keywords(float freq_start, float delta_freq, float time_start, float delta_time) {
   char szString[64];
   std::string empty_string;
   
   add_keyword(std::string("CTYPE2"),std::string("FREQUENCY"),empty_string);
   add_keyword(std::string("CUNIT2"),std::string("MHz"),empty_string);
   add_keyword(std::string("CRPIX2"),std::string("1"),empty_string);
   sprintf(szString,"%.6f",freq_start);
   add_keyword(std::string("CRVAL2"),std::string(szString),empty_string);
   sprintf(szString,"%.6f",delta_freq);
   add_keyword(std::string("CDELT2"),std::string(szString),empty_string);
   
   add_keyword(std::string("CTYPE1"),std::string("TIME"),empty_string);
   add_keyword(std::string("CUNIT1"),std::string("sec"),empty_string);
   add_keyword(std::string("CRPIX1"),std::string("1"),empty_string);
   add_keyword(std::string("CRVAL1"),std::string("0"),empty_string);
   sprintf(szString,"%.6f",delta_time);
   add_keyword(std::string("CDELT1"),std::string(szString),empty_string);
   add_keyword(std::string("INTTIME"),std::string(szString),empty_string);
}


void FITS::read(){
    std::ifstream fp {filename.c_str()};
    if(!fp.good()){
        std::cerr << "FITS::from_file: requested file '" << filename << "' does not exist or is inaccessible." << std::endl;
        throw std::exception();
    }
    fp.close();
    int status {0}, nHDUs {-1};
    CHECK_FITS_ERROR(fits_open_file(&fitsFP, filename.c_str(), READONLY, &status));
    CHECK_FITS_ERROR(fits_get_num_hdus(fitsFP, &nHDUs,  &status));
    HDUs.resize(nHDUs);

    int dims;
    int nKeys;

    char *data;
    int bitPix {-1};
    int dataType {-1};
    long axes[2];
    char keyCard[FLEN_CARD];
    char valueCard[FLEN_CARD];
    char commentCard[FLEN_CARD];
    for(int hdu {1}; hdu <= nHDUs; hdu++){
        HDU& cHDU {HDUs[hdu-1]};
        CHECK_FITS_ERROR(fits_movabs_hdu(fitsFP, hdu, NULL, &status));
        CHECK_FITS_ERROR(fits_get_hdrspace(fitsFP, &nKeys, NULL, &status));
        for(int key {1}; key <= nKeys; key++){
            CHECK_FITS_ERROR(fits_read_keyn(fitsFP, key, keyCard, valueCard, commentCard, &status));
            if(!is_special_keyword(keyCard)){
                std::stringstream ss;
                ss << (char*)valueCard;
                int ivalue;
                ss >> ivalue;
                if(!ss.fail() && ss.eof()) {
                    cHDU.add_keyword(keyCard, ivalue, commentCard);
                    continue;
                }
                ss.str(valueCard);
                double dvalue;
                ss >> dvalue;
                if(!ss.fail() && ss.eof()) {
                    cHDU.add_keyword(keyCard, dvalue, commentCard);
                    continue;
                }
                cHDU.add_keyword(keyCard, valueCard, commentCard);
            }    
        }
            
        CHECK_FITS_ERROR(fits_get_img_dim(fitsFP, &dims, &status));
        if(dims == 2){
            // it is an actual image.
            // get the data type used
            CHECK_FITS_ERROR(fits_get_img_type(fitsFP,&bitPix, &status));
            CHECK_FITS_ERROR(fits_get_img_size(fitsFP, dims, axes, &status));
            data = new char[axes[0] * axes[1] * abs(bitPix) / 8];
            switch (bitPix) {
                case BYTE_IMG: dataType = TBYTE; break;
                case LONG_IMG: dataType = TLONG; break;
                case FLOAT_IMG: dataType = TFLOAT; break;
                case DOUBLE_IMG: dataType = TDOUBLE; break;
                default: throw std::runtime_error{"FITS::from_file: data type not supported."};
            }
            // read data
            long fPixel[2] {1, 1};
            CHECK_FITS_ERROR(fits_read_pix(fitsFP, dataType, fPixel, axes[0] * axes[1], nullptr, data, nullptr, &status));
            cHDU.set_image(bitPix, data, axes[0], axes[1]);
        } else if(dims != 0){ // 0 is an empty HDU - it is ok, just header information.
            std::cerr << "Unexpected number of dimensions in fits file: " << dims << " instead of 2." << std::endl;
            throw std::exception();
        }
    }
}


void FITS::append_hdu(const FITS::HDU& hdu){
    long axes[2];
    int status = 0;
    if(hdu.data == nullptr) {
        // It is an empty HDU. Probably the primary HDU.
        // only containing header keywords
        CHECK_FITS_ERROR(fits_create_img(fitsFP, 32, 0,  nullptr, &status));
    }else{
        axes[0] = hdu.get_ydim();
        axes[1] = hdu.get_xdim();
        CHECK_FITS_ERROR(fits_create_img(fitsFP, hdu.bitpix, 2,  axes, &status));
        long fPixel[2] {1, 1};
        CHECK_FITS_ERROR(fits_write_pix(fitsFP, hdu.datatype, fPixel, axes[0] * axes[1], (char *) hdu.get_image_data(), &status));
    }
    for(auto& header_entry : hdu.get_header()){
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



void FITS::write(){
    if(open_mode != Mode::WRITE) throw std::runtime_error {"'FITS::to_file' can only be called in WRITE mode."};
    std::ifstream fp {filename.c_str()};
    // remove file if exists already - we overwrite by default.
    if(fp.good()){
        fp.close();
        std::remove(filename.c_str());
    }
    int status = 0;
    long axes[2];
    CHECK_FITS_ERROR(fits_create_file(&fitsFP, filename.c_str(), &status));
    for(HDU& cHDU : this->HDUs){
        append_hdu(cHDU);
    }
    CHECK_FITS_ERROR(fits_close_file(fitsFP, &status));
    fitsFP = nullptr;
}
