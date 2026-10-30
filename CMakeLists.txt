cmake_minimum_required(VERSION 3.24)
project(waster_ur_time LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 1. Поиск RocksDB (установленного через vcpkg)
find_package(RocksDB REQUIRED)

add_executable(${PROJECT_NAME})

target_sources(${PROJECT_NAME} PRIVATE 
    PS4_Passcode_Bruteforcer/PS4_Passcode_Bruteforcer.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE 
    ${CMAKE_CURRENT_LIST_DIR}/PS4_Passcode_Bruteforcer/
)

target_link_libraries(${PROJECT_NAME} PRIVATE 
    RocksDB::rocksdb 
    Rpcrt4.lib 
    Shlwapi.lib
)

if(WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

# 2. Поддержка GPU (CUDA или HIP)
option(USE_CUDA "Enable NVIDIA CUDA support" OFF)
option(USE_HIP "Enable AMD HIP support" OFF)

if(USE_CUDA)
    include(CheckLanguage)
    check_language(CUDA)
    if(CMAKE_CUDA_COMPILER)
        enable_language(CUDA)
        message(STATUS "CUDA found - GPU bruteforce enabled")
        
        file(GLOB_RECURSE CUDA_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/PS4_Passcode_Bruteforcer/*.cu")
        target_sources(${PROJECT_NAME} PRIVATE ${CUDA_SOURCES})
        target_compile_definitions(${PROJECT_NAME} PRIVATE USE_CUDA)
        
        set_target_properties(${PROJECT_NAME} PROPERTIES
            CUDA_SEPARABLE_COMPILATION ON
            CUDA_ARCHITECTURES "50;52;60;61;70;75;80;86;89;90"
        )
    else()
        message(FATAL_ERROR "USE_CUDA is ON but CUDA compiler not found")
    endif()

elseif(USE_HIP)
    message(STATUS "Building with AMD HIP/ROCm backend via Isolated Custom Command")
    
    set(GPU_SRC "${CMAKE_CURRENT_LIST_DIR}/PS4_Passcode_Bruteforcer/gpu_bruteforce.cu")
    set(GPU_OBJ "${CMAKE_CURRENT_BINARY_DIR}/gpu_bruteforce.obj")
    
    # Добавлены флаги -D_DLL -D_MT -Xclang --dependent-lib=msvcrt для исправления конфликта линковки /MD vs /MT
    add_custom_command(
        OUTPUT ${GPU_OBJ}
        COMMAND "C:/Program Files/AMD/ROCm/7.2/bin/clang++.exe"
        ARGS -x hip --offload-arch=gfx1101 -std=c++17 
             -D_DLL -D_MT -Xclang --dependent-lib=msvcrt
             -D_ALLOW_COMPILER_AND_STL_VERSION_MISMATCH -DUSE_HIP
             -ID:/Waste_Ur_Time-main/PS4_Passcode_Bruteforcer/gpu_hacks
             -ID:/Waste_Ur_Time-main/PS4_Passcode_Bruteforcer
             -isystem "C:/Program Files/AMD/ROCm/7.2/include"
             -O3 -DNDEBUG -c ${GPU_SRC} -o ${GPU_OBJ}
        DEPENDS ${GPU_SRC}
        COMMENT "Compiling GPU Kernel in pure C++17 isolated environment..."
    )
    
    target_sources(${PROJECT_NAME} PRIVATE ${GPU_OBJ})
    target_compile_definitions(${PROJECT_NAME} PRIVATE USE_HIP)
    
    target_link_libraries(${PROJECT_NAME} PRIVATE "C:/Program Files/AMD/ROCm/7.2/lib/amdhip64.lib")

else()
    message(STATUS "No GPU backend selected - CPU-only bruteforce")
endif()


