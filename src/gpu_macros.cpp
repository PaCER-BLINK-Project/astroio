#include "gpu_macros.hpp"

#ifdef __GPU__
void __gpu_check_error(gpuError_t x, const char *file, int line){
    if(x != gpuSuccess){
        fprintf(stderr, "GPU error (%s:%d): %s\n", file, line, gpuGetErrorString(x));
        exit(1);
    }
}

int num_available_gpus() {
    int num_gpus;
    gpuGetDeviceCount(&num_gpus);
    return num_gpus;
}

#else
int num_available_gpus() {
    return 0;
}
#endif
