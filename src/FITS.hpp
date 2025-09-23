#ifndef __ASTROIO_FITS_H__
#define __ASTROIO_FITS_H__

#include <map>
#include <vector>
#include <string>
#include <cstring>
#include <sstream>
#include <fitsio.h>
#include <stdexcept>
#include <cmath>
#include <iostream>

/**
 * @brief A class that handles I/O operations on FITS files.
*/
class FITS {

    public:

    /**
     * @brief Represents Header Data Unit in a FITS file.
     * 
     * @bug Need to check whether BITPIX is set.
    */
    class HDU {

        class HeaderEntry {
            public:
            // TODO use std::variant
            union {
                char *sval;
                long long llval;
                double dval;
            } data;
            int data_type;
            std::string comment;
            std::string keyword;

            ~HeaderEntry(){
                if (data_type == TSTRING && data.sval) {
                    delete[] data.sval;
                    data.sval = nullptr;
                }
            };

            HeaderEntry(){};

            HeaderEntry(const HeaderEntry& other){
                this->data = other.data;
                this->data_type = other.data_type;
                this->comment = other.comment;
                this->keyword = other.keyword;
                if(this->data_type == TSTRING){
                    this->data.sval = new char[strlen(other.data.sval) + 1];
                    strcpy(this->data.sval, other.data.sval);
                }
            };

            HeaderEntry(HeaderEntry&& other){
                this->data = other.data;
                this->data_type = other.data_type;
                this->comment = std::move(other.comment);
                this->keyword = std::move(other.keyword);
                other.data.sval = nullptr;
            };


            HeaderEntry& operator=(const HeaderEntry& other){
                this->data = other.data;
                this->data_type = other.data_type;
                this->comment = other.comment;
                this->keyword = other.keyword;
                if(this->data_type == TSTRING){
                    this->data.sval = new char[strlen(other.data.sval) + 1];
                    strcpy(this->data.sval, other.data.sval);
                }
                return *this;
            }

            HeaderEntry& operator=(HeaderEntry&& other){
                this->data = other.data;
                this->data_type = other.data_type;
                this->comment = std::move(other.comment);
                this->keyword = std::move(other.keyword);
                other.data.sval = nullptr;
                return *this;
            };
        };

        std::map<std::string, HeaderEntry> header;
        long axes[2] {0, 0};
        int bitpix = -1;
        int datatype = -1;
        void *data = nullptr;
        
        friend class FITS;
        public:

        /**
         * @brief Add a new (keyword, value, comment) triple to the HDU header.
         * 
         * @param key Unique string identifier for the new header entry.
         * @param value Value associated with the key. Its type must be one of the following:
         * `char`, `char*`, `short`, `unsigned short`, `int`, `unsigned int`, `long`,
         * `unsigned long`, `long long`, `float`, `double`. 
         * @param comment A string describing the entry.
         * 
         * @author Cristian Di Pietrantonio
        */
        void add_keyword(const std::string key, const std::string value, const std::string comment){
            HeaderEntry he;
            he.keyword = key;
            he.comment = comment;
            size_t slen {value.length()};
            he.data.sval = new char[slen + 1];
            std::strcpy(he.data.sval, value.c_str());
            he.data_type = TSTRING;
            this->header.insert({key, he});
        }

        void add_keyword(const std::string key, char* const value, const std::string comment){
            HeaderEntry he;
            he.keyword = key;
            he.comment = comment;
            size_t slen {std::strlen(value)};
            he.data.sval = new char[slen + 1];
            std::strcpy(he.data.sval, value);
            he.data_type = TSTRING;
            this->header.insert({key, he});
        }

        template <typename T>
        void add_keyword(const std::string key, const T value, const std::string comment){
            HeaderEntry he;
            he.keyword = key;
            he.comment = comment;
            if(typeid(T) == typeid(float) || typeid(T) == typeid(double)){
                if(std::isnan(value) || std::isinf(value)){
                    std::cerr << "WARNING: FITS::add_keyword: value for " << key << " is not valid." << std::endl;
                    he.data.dval = 0.0;
                }else{
                    he.data.dval = static_cast<double>(value);
                }
                he.data_type = TDOUBLE;
            }else{
                he.data.llval = static_cast<long long>(value);
                he.data_type = TLONGLONG;
            }
            this->header.insert({key, he});
        }

        /**
         * @brief retrieve a header entry.
        */
        template <typename T>
        std::pair<T, std::string> get_keyword(std::string key){
            HeaderEntry& he {header.at(key)};
            T val;
            switch (he.data_type) {
                case TSTRING:{
                    std::stringstream extractor;
                    extractor << he.data.sval;
                    extractor >> val;
                    break;
                }
                case TDOUBLE:
                    val = static_cast<T>(he.data.dval);
                    break;
                case TLONGLONG:
                    val = static_cast<T>(he.data.llval);
                    break;
                default:
                    throw std::exception(); // TODO better exception.
            }
            return std::make_pair(val, he.comment);
        }

        /**
         * @brief Set the array of data representing an image. This function is used when the
         * data type cannot be inferred by the associated pointer. One such case is when
         * reading a FITS file using the cfitsio library, where the data type is given by the
         * BITPIX value.
         * 
         * @param bitpix: BITPIX value as defined by the cfitsio library. This value is returned
         * by the `fits_get_img_type` function.
         * @param data: array of values representing the image. It sise is `x_dim * y_dim * abs(bitpix) / 8`.
         * @param x_dim: dimension of the image along the orizontal axis.
         * @param y_dim: dimension of the image along the vertical axis.
        */
        void set_image(int bitpix, char *data, long x_dim, long y_dim);

        template <typename T>
        void set_image(T *data, long xDim, long yDim){
            if(this->data) delete[] static_cast<char*>(this->data);
            this->data = data;
            if(typeid(T) == typeid(float)){
                this->datatype = TFLOAT;
                this->bitpix = FLOAT_IMG;
            }else if(typeid(T) == typeid(double)){ 
                this->datatype = TDOUBLE;
                this->bitpix = DOUBLE_IMG;
            }else if(typeid(T) == typeid(char)){
                this->datatype = TBYTE;
                this->bitpix = BYTE_IMG;
            }else if(typeid(T) == typeid(long)){
                this->datatype = TLONG;
                this->bitpix = LONG_IMG;
            }else 
                throw std::invalid_argument {"set_image: data type of first argument not recognised."};
            axes[0] = xDim;
            axes[1] = yDim;
        }



        bool operator==(const HDU& other) const {
            if(axes[0] != other.axes[0] || axes[1] != other.axes[1] || 
                bitpix != other.bitpix) return false;
            char *pData {reinterpret_cast<char*>(data)};
            char *pOtherData {reinterpret_cast<char*>(other.data)};

            for(long long x {0}; x < axes[0] * axes[1] * std::abs(bitpix) / 8; x++){
                if(*pData++ != *pOtherData++) return false;
            }
            return true;
        }

        bool operator!=(const HDU& other) const {
            return !(*this == other);
        }
        
        void *get_image_data(){ return data; }

        long get_xdim() { return axes[1]; }

        long get_ydim() { return axes[0]; }

        int get_bitpix() { return bitpix; }

        int get_datatype() { return datatype; }

        auto get_header() { return header; }
    };

    private:
    std::vector<HDU> HDUs;

    public:
    void add_HDU(const HDU& hdu, int pos = -1) {
        if(pos < 0)
            HDUs.push_back(hdu);
        else
            HDUs.insert(HDUs.begin() + pos, hdu);
    }

    HDU& operator[](int idx) {
        return HDUs[idx];
    }

    size_t size() const {
        return HDUs.size();
    }
    
    const HDU& operator[](int idx) const {
        return HDUs[idx];
    }

    friend auto begin(FITS& f){ //  -> decltype(f.HDUs.begin()){
        return f.HDUs.begin();
    } 

    friend auto end(FITS& f){ // -> decltype(f.HDUs.end()){
        return f.HDUs.end();
    } 
    
    static FITS from_file(std::string filename);

    void to_file(std::string filename);
};

#endif

