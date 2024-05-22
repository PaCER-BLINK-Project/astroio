#include <iostream>
#include <fstream>
#include <cstring>

#define PARSE_BUFFER_SIZE 1024



void read_data_from_file(std::string filename, char*& data, size_t& file_size){
    std::ifstream f;
    f.open(filename, std::ios::binary);
    if(!f){
        std::cerr << "read_data_from_file: error while reading the file." << std::endl;
        data = nullptr;
        return;
    }
    size_t buff_size {4096};
    data = new char[buff_size];
    size_t bytes_read {0};
    const size_t read_size {4096};
    while(f){
        // check if we need to reallocate memory
         if(bytes_read + read_size > buff_size){
            buff_size *= 2;
            char *tmp = new char[buff_size];
            memcpy(tmp, data, bytes_read);
            delete[] data;
            data = tmp;
         }
        f.read(data + bytes_read, read_size);
        bytes_read += f.gcount();
    }
    file_size = bytes_read;
}



double parse_timespec(const char * const spec){
    static char buffer[PARSE_BUFFER_SIZE];
    double result;
    int len = strlen(spec);
    int dots {0};
    if(len == 0) throw std::invalid_argument("Timespec string has zero length.");
    int i = 0;
    while(i < len && (isdigit(spec[i]) || (spec[i] == '.' && dots++ == 0))){
        buffer[i] = spec[i];
        i++;       
    }
    if(i >= PARSE_BUFFER_SIZE - 1) throw std::invalid_argument("Timespec string is too long.");
    buffer[i] = '\0';
    result = static_cast<double>(atof(buffer));
    if(i < len){
        if(!strcmp(spec + i, "ms")) result /= 1000;
        else if(!strcmp(spec + i, "cs")) result /= 100;
        else if(!strcmp(spec + i, "ds")) result /= 10;
        else if(strcmp(spec + i, "s")) throw std::invalid_argument("Invalid timespec string.");
    }else{
        throw std::invalid_argument("Invalid timespec string.");
    }
    return result;
}



time_t gps_to_unix(time_t gps){
    const time_t gps_epoch_in_unix_time {315964800ll};
    const time_t gps_leap_seconds {18ll}; // since 31/12/2016
    return gps_epoch_in_unix_time + gps - gps_leap_seconds;
}