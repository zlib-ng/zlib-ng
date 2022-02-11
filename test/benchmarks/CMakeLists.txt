cmake_minimum_required(VERSION 3.12)

include(FetchContent)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)
enable_language(CXX)

# Search for Google benchmark package
find_package(benchmark QUIET)
if(NOT benchmark_FOUND)
    # Fetch google benchmark source code from official repository
    set(BENCHMARK_ENABLE_TESTING OFF)
    FetchContent_Declare(benchmark
        GIT_REPOSITORY https://github.com/google/benchmark.git)
    FetchContent_MakeAvailable(benchmark)
    FetchContent_GetProperties(benchmark)

    if(NOT benchmark_POPULATED)
        FetchContent_Populate(benchmark)
    endif()
endif()

add_executable(benchmark_zlib
    benchmark_adler32.cc
    benchmark_compare256.cc
    benchmark_crc32.cc
    benchmark_main.cc
    benchmark_slidehash.cc
    )

target_include_directories(benchmark_zlib PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_BINARY_DIR}
    ${benchmark_SOURCE_DIR}/benchmark/include)

target_link_libraries(benchmark_zlib zlibstatic benchmark::benchmark)
if(WIN32)
    target_link_libraries(benchmark_zlib shlwapi)
endif()

if(ZLIB_ENABLE_TESTS)
    add_test(NAME benchmark_zlib
        COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} $<TARGET_FILE:benchmark_zlib>)
endif()
