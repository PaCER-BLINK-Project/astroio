cmake_minimum_required(VERSION 3.10.0)
project(blink_astroio)
set(CMAKE_CXX_STANDARD 14)
include(CTest)

option(USE_CUDA "Compile the code with NVIDIA GPU support." OFF)
option(USE_HIP "Compile the code with AMD GPU support." OFF)

if(USE_CUDA)
enable_language(CUDA CXX)
endif()



# External dependencies
find_library(CFITSIO_LIB cfitsio HINTS ENV LD_LIBRARY_PATH)
if( NOT CFITSIO_LIB )
    message(FATAL_ERROR "CFITSIO not found.")
endif()

file(GLOB astroio_sources "src/*.cpp")
file(GLOB astroio_apps "apps/*.cpp")
file(GLOB astroio_tests "tests/*.cpp")
file(GLOB astroio_headers "src/*.hpp")

if(USE_CUDA)
set_source_files_properties( ${astroio_sources} ${astroio_tests} ${astroio_apps} PROPERTIES LANGUAGE CUDA)
endif()

add_library(blink_astroio SHARED ${astroio_sources})
set_target_properties(blink_astroio PROPERTIES PUBLIC_HEADER "${astroio_headers}")
target_link_libraries(blink_astroio ${CFITSIO_LIB})


install(TARGETS blink_astroio
    LIBRARY DESTINATION "lib"
    PUBLIC_HEADER DESTINATION "include"
)

add_executable(blink_adjust_fits apps/adjust_fits.cpp)
target_link_libraries(blink_adjust_fits blink_astroio)
install(TARGETS blink_adjust_fits DESTINATION "bin")

# TESTS
add_executable(blink_astroio_test tests/astroio_test.cpp)
target_link_libraries(blink_astroio_test blink_astroio)
add_test(NAME blink_astroio_test COMMAND blink_astroio_test)

add_executable(fits_test tests/fits_test.cpp)
target_link_libraries(fits_test blink_astroio)
add_test(NAME fits_test COMMAND fits_test)

add_executable(utils_test tests/utils_test.cpp)
target_link_libraries(utils_test blink_astroio)
add_test(NAME utils_test COMMAND utils_test)

add_executable(metadata_test tests/metadata_test.cpp)
target_link_libraries(metadata_test blink_astroio)
add_test(NAME metadata_test COMMAND metadata_test)

if(CMAKE_CXX_COMPILER MATCHES "hipcc" OR CMAKE_CXX_COMPILER MATCHES "nvcc")
add_executable(memory_buffer_test tests/memory_buffer_test.cpp)
target_link_libraries(memory_buffer_test blink_astroio)
add_test(NAME memory_buffer_test COMMAND memory_buffer_test)
endif()
