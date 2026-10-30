#pragma once
#define __CLANG_HIP_CMATH_H__ 1

#if defined(USE_HIP)

    #include <hip/hip_runtime.h>

    using gpuError_t    = hipError_t;
    using gpuDeviceProp = hipDeviceProp_t;
    using gpuStream_t   = hipStream_t;

    #define gpuSuccess                 hipSuccess
    #define gpuGetDeviceCount          hipGetDeviceCount
    #define gpuSetDevice               hipSetDevice
    #define gpuGetDevice               hipGetDevice
    #define gpuGetDeviceProperties     hipGetDeviceProperties
    #define gpuMalloc                  hipMalloc
    #define gpuFree                    hipFree
    #define gpuMemcpy                  hipMemcpy
    #define gpuMemset                  hipMemset
    #define gpuDeviceSynchronize       hipDeviceSynchronize
    #define gpuGetLastError            hipGetLastError
    #define gpuGetErrorString          hipGetErrorString
    #define gpuMemcpyHostToDevice      hipMemcpyHostToDevice
    #define gpuMemcpyDeviceToHost      hipMemcpyDeviceToHost
    #define gpuMemcpyDeviceToDevice    hipMemcpyDeviceToDevice

#elif defined(USE_CUDA)

    #include <cuda_runtime.h>

    using gpuError_t    = cudaError_t;
    using gpuDeviceProp = cudaDeviceProp;
    using gpuStream_t   = cudaStream_t;

    #define gpuSuccess                 cudaSuccess
    #define gpuGetDeviceCount          cudaGetDeviceCount
    #define gpuSetDevice               cudaSetDevice
    #define gpuGetDevice               cudaGetDevice
    #define gpuGetDeviceProperties     cudaGetDeviceProperties
    #define gpuMalloc                  cudaMalloc
    #define gpuFree                    cudaFree
    #define gpuMemcpy                  cudaMemcpy
    #define gpuMemset                  cudaMemset
    #define gpuDeviceSynchronize       cudaDeviceSynchronize
    #define gpuGetLastError            cudaGetLastError
    #define gpuGetErrorString          cudaGetErrorString
    #define gpuMemcpyHostToDevice      cudaMemcpyHostToDevice
    #define gpuMemcpyDeviceToHost      cudaMemcpyDeviceToHost
    #define gpuMemcpyDeviceToDevice    cudaMemcpyDeviceToDevice

#else
    #error "Enable a GPU backend: define USE_CUDA or USE_HIP"
#endif