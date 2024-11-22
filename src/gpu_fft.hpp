#ifndef __GPUFFT_H__
#define __GPUFFT_H__

#ifdef __NVCC__
  // NVIDIA / CUFFT :

  #include <cufftw.h>
  #include <cufft.h>
  
  #define gpufftComplex  cufftComplex
  #define gpufftReal     cufftReal
  #define gpufftPlanMany cufftPlanMany
  #define gpufftHandle   cufftHandle
  #define gpufftPlan2d   cufftPlan2d
  #define gpufftExecC2C  cufftExecC2C
  #define gpufftExecC2C  cufftExecC2R
  #define GPUFFT_C2C     CUFFT_C2C
  #define GPUFFT_C2R     CUFFT_C2R
  #define GPUFFT_FORWARD CUFFT_FORWARD
  #define GPUFFT_BACKWARD 1
#else
  // AMD / HIP :
  
  #include <hipfft/hipfft.h>
  
  #define gpufftComplex  hipfftComplex
  #define gpufftReal     hipfftReal
  #define gpufftPlanMany hipfftPlanMany
  #define gpufftHandle   hipfftHandle
  #define gpufftPlan2d   hipfftPlan2d
  #define gpufftExecC2C  hipfftExecC2C
  #define gpufftExecC2R  hipfftExecC2R
  #define GPUFFT_C2C     HIPFFT_C2C
  #define GPUFFT_C2R     HIPFFT_C2R
  #define GPUFFT_FORWARD HIPFFT_FORWARD
  #define GPUFFT_BACKWARD 1
#endif


#endif
