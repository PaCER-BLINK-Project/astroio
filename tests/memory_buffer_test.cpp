#include <iostream>
#include <cstring>
#include <string>
#include <sstream>
#include "common.hpp"
#include "../src/memory_buffer.hpp"


std::string data_root_dir;

__global__ void test_values(int* values, int n, int* out){
    if(threadIdx.x < n){
        atomicAdd(out, values[threadIdx.x]);
    }
}

void test_memory_buffer(){
    // TODO: Run valgrind to check for memory leaks
    MemoryBuffer<int> mem_cpu (5, false, false);
    auto ptr = mem_cpu.data();
    for(int i {0}; i < 5; i++){
        ptr[i] = i;
    }
    mem_cpu.to_gpu();
    int out, *dev_out;
    gpuMalloc(&dev_out, sizeof(int));
    gpuMemset(dev_out, 0, sizeof(int));
    test_values<<<1, 5>>>(mem_cpu.data(), mem_cpu.size(), dev_out);
    gpuMemcpy(&out, dev_out, sizeof(int), gpuMemcpyDeviceToHost);
    gpuDeviceSynchronize();
    mem_cpu.to_cpu();
    
    int expected_out {0};
    auto another_ptr = mem_cpu.data();
    for(int i {0}; i < 5; i++){
        expected_out += another_ptr[i];
    }
    if(out != expected_out){
        std::stringstream ss;
        ss << "'test_memory_buffer' failed: wrong result (" << out << " != " << expected_out << ").\n";
        throw TestFailed(ss.str());
    }
    
    std::cout << "'test_memory_buffer' passed." << std::endl;
}



int main(void){
    char *path_to_data {std::getenv(ENV_DATA_ROOT_DIR)};
    if(!path_to_data){
        std::cerr << "'" << ENV_DATA_ROOT_DIR << "' environment variable is not set." << std::endl;
        return -1;
    }
    data_root_dir = std::string{path_to_data};
    try{
        
        test_memory_buffer();

    } catch (TestFailed ex){
        std::cerr << ex.what() << std::endl;
        return 1;
    }
    
    std::cout << "All tests passed." << std::endl;
    return 0;
}
