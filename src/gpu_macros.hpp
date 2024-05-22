#ifndef __GPU_MACROS_H__
#define __GPU_MACROS_H__

#include <stdio.h>
int num_available_gpus();

#if defined (__NVCC__) || defined (__HIPCC__)

constexpr bool gpu_support() { return true;}
#define __GPU__

// I first define the error handling macro and related definitions. I will
// then use those to wrap all other macros, so that error handling is done
// automatically when using "gpu*" calls.

#ifdef __NVCC__
#define gpuError_t cudaError_t
#define gpuSuccess cudaSuccess
#define gpuGetErrorString cudaGetErrorString
#else
#include <hip/hip_runtime.h>
#define gpuError_t hipError_t
#define gpuSuccess hipSuccess
#define gpuGetErrorString hipGetErrorString
#endif

void __gpu_check_error(gpuError_t x, const char *file, int line);

#define GPU_CHECK_ERROR(X)({\
    __gpu_check_error((X), __FILE__, __LINE__);\
})


#ifdef __NVCC__

#define gpuMalloc(...) GPU_CHECK_ERROR(cudaMalloc(__VA_ARGS__))
#define gpuHostAlloc(...) GPU_CHECK_ERROR(cudaHostAlloc(__VA_ARGS__, 0))
#define gpuHostAllocDefault cudaHostAllocDefault
#define gpuMemcpy(...) GPU_CHECK_ERROR(cudaMemcpy(__VA_ARGS__))
#define gpuMemcpyAsync(...) GPU_CHECK_ERROR(cudaMemcpyAsync(__VA_ARGS__))
#define gpuMemset(...) GPU_CHECK_ERROR(cudaMemset(__VA_ARGS__))
#define gpuDeviceSynchronize(...) GPU_CHECK_ERROR(cudaDeviceSynchronize(__VA_ARGS__))
#define gpuMemcpyDeviceToHost cudaMemcpyDeviceToHost
#define gpuMemcpyHostToDevice cudaMemcpyHostToDevice
#define gpuMemcpyDeviceToDevice cudaMemcpyDeviceToDevice
#define gpuFree(...) GPU_CHECK_ERROR(cudaFree(__VA_ARGS__))
#define gpuHostFree(...) GPU_CHECK_ERROR(cudaFreeHost(__VA_ARGS__))
#define gpuStream_t cudaStream_t
#define gpuStreamCreate(...) GPU_CHECK_ERROR(cudaStreamCreate(__VA_ARGS__))
#define gpuStreamDestroy(...) GPU_CHECK_ERROR(cudaStreamDestroy(__VA_ARGS__))
#define gpuEventCreate(...) GPU_CHECK_ERROR(cudaEventCreate(__VA_ARGS__))
#define gpuGetDeviceCount(...) GPU_CHECK_ERROR(cudaGetDeviceCount(__VA_ARGS__))
#define gpuGetLastError cudaGetLastError
#define gpuGetDevice(...) GPU_CHECK_ERROR(cudaGetDevice(__VA_ARGS__))
#define gpuSetDevice(...) GPU_CHECK_ERROR(cudaSetDevice(__VA_ARGS__))
#define gpuDeviceGetAttribute(...) GPU_CHECK_ERROR(cudaDeviceGetAttribute(__VA_ARGS__))
#define gpuDeviceAttributeWarpSize cudaDevAttrWarpSize
#define __gpu_shfl_down(...) __shfl_down_sync(0xffffffff, __VA_ARGS__)

#else

#define gpuMalloc(...) GPU_CHECK_ERROR(hipMalloc(__VA_ARGS__))
#define gpuHostAlloc(...) GPU_CHECK_ERROR(hipHostMalloc(__VA_ARGS__, 0))
#define gpuHostAllocDefault 0
#define gpuMemcpy(...) GPU_CHECK_ERROR(hipMemcpy(__VA_ARGS__))
#define gpuMemcpyAsync(...) GPU_CHECK_ERROR(hipMemcpyAsync(__VA_ARGS__))
#define gpuMemset(...) GPU_CHECK_ERROR(hipMemset(__VA_ARGS__))
#define gpuDeviceSynchronize(...) GPU_CHECK_ERROR(hipDeviceSynchronize(__VA_ARGS__))
#define gpuMemcpyDeviceToHost hipMemcpyDeviceToHost
#define gpuMemcpyHostToDevice hipMemcpyHostToDevice
#define gpuMemcpyDeviceToDevice hipMemcpyDeviceToDevice
#define gpuFree(...) GPU_CHECK_ERROR(hipFree(__VA_ARGS__))
#define gpuHostFree(...) GPU_CHECK_ERROR(hipHostFree(__VA_ARGS__))
#define gpuStream_t hipStream_t
#define gpuStreamCreate(...) GPU_CHECK_ERROR(hipStreamCreate(__VA_ARGS__))
#define gpuStreamDestroy(...) GPU_CHECK_ERROR(hipStreamDestroy(__VA_ARGS__))
#define gpuEventCreate(...) GPU_CHECK_ERROR(hipEventCreate(__VA_ARGS__))
#define gpuGetDeviceCount(...) hipGetDeviceCount(__VA_ARGS__)
#define gpuGetLastError hipGetLastError
#define gpuGetDevice(...) GPU_CHECK_ERROR(hipGetDevice(__VA_ARGS__))
#define gpuSetDevice(...) GPU_CHECK_ERROR(hipSetDevice(__VA_ARGS__))
#define gpuDeviceGetAttribute(...) GPU_CHECK_ERROR(hipDeviceGetAttribute(__VA_ARGS__))
#define gpuDeviceAttributeWarpSize hipDeviceAttributeWarpSize
#define __gpu_shfl_down(...) __shfl_down(__VA_ARGS__)

#endif
#define gpuCheckLastError(...) GPU_CHECK_ERROR(gpuGetLastError())
#else
constexpr bool gpu_support() { return false;}
#endif
#endif
