cmake_minimum_required(VERSION 3.14...4.2.1)

include(FetchContent)

if(NOT DEFINED CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 11)
endif()
if(NOT DEFINED CMAKE_CXX_STANDARD_REQUIRED)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()
if(NOT DEFINED CMAKE_CXX_EXTENSIONS)
    set(CMAKE_CXX_EXTENSIONS ON)
endif()

# Search for Google benchmark package
find_package(benchmark QUIET)
if(NOT benchmark_FOUND)
    # Fetch google benchmark source code from official repository
    set(BENCHMARK_ENABLE_TESTING OFF)

    # Allow specifying alternative Google benchmark repository
    if(NOT DEFINED GBENCHMARK_REPOSITORY)
        set(GBENCHMARK_REPOSITORY https://github.com/google/benchmark.git)
    endif()
    if(NOT DEFINED GBENCHMARK_TAG)
        set(GBENCHMARK_TAG v1.9.4)
    endif()

    FetchContent_Declare(benchmark
        GIT_REPOSITORY ${GBENCHMARK_REPOSITORY}
        GIT_TAG ${GBENCHMARK_TAG}
        ${ZNG_FetchContent_Declare_EXCLUDE_FROM_ALL})

    ZNG_FetchContent_MakeAvailable(benchmark)
endif()

add_executable(benchmark_zlib
    benchmark_adler32.cc
    benchmark_adler32_copy.cc
    benchmark_compare256.cc
    benchmark_compare256_rle.cc
    benchmark_compress.cc
    benchmark_crc32.cc
    benchmark_crc32_copy.cc
    benchmark_insert_string.cc
    benchmark_inflate.cc
    benchmark_main.cc
    benchmark_slidehash.cc
    benchmark_uncompress.cc
    )

target_compile_definitions(benchmark_zlib PRIVATE -DBENCHMARK_STATIC_DEFINE)
target_include_directories(benchmark_zlib PRIVATE
    ${PROJECT_SOURCE_DIR}
    ${PROJECT_BINARY_DIR}
    ${benchmark_SOURCE_DIR}/benchmark/include)

target_link_libraries(benchmark_zlib zlib-ng-static benchmark::benchmark)
if(WIN32)
    target_link_libraries(benchmark_zlib shlwapi)
endif()

add_test(NAME benchmark_zlib
    COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} $<TARGET_FILE:benchmark_zlib> "--benchmark_min_time=0")

if(WITH_BENCHMARK_APPS)
    option(BUILD_ALT_BENCH "Link against alternative zlib implementation" OFF)

    # Search for libpng package
    find_package(PNG QUIET)

    if(NOT PNG_FOUND)
        FetchContent_Declare(PNG
            GIT_REPOSITORY https://github.com/glennrp/libpng.git
            ${ZNG_FetchContent_Declare_EXCLUDE_FROM_ALL})

        ZNG_FetchContent_MakeAvailable(PNG)
        set(PNG_INCLUDE_DIR ${png_SOURCE_DIR})
    endif()

    set(BENCH_APP_SRCS
        benchmark_png_encode.cc
        benchmark_png_decode.cc
        benchmark_main.cc
    )

    add_executable(benchmark_zlib_apps ${BENCH_APP_SRCS})

    if(DEFINED BUILD_ALT_BENCH)
        set(ZLIB_ALT_LIB "libz.a" CACHE FILEPATH "Optional alternative zlib implementation (defaults to stock zlib)")
        add_executable(benchmark_zlib_apps_alt ${BENCH_APP_SRCS})
        target_link_libraries(benchmark_zlib_apps_alt libpng.a ${ZLIB_ALT_LIB} benchmark::benchmark)
        target_compile_definitions(benchmark_zlib_apps_alt PRIVATE BUILD_ALT=1)
        target_include_directories(benchmark_zlib_apps_alt PRIVATE
            ${PROJECT_SOURCE_DIR}
            ${PROJECT_BINARY_DIR}
            ${PNG_INCLUDE_DIR}
            ${benchmark_SOURCE_DIR}/benchmark/include)
    endif()

    target_include_directories(benchmark_zlib_apps PRIVATE
        ${PROJECT_SOURCE_DIR}
        ${PROJECT_BINARY_DIR}
        ${PNG_INCLUDE_DIR}
        ${benchmark_SOURCE_DIR}/benchmark/include)

    # We need the static png library if we're statically linking to zlib,
    # otherwise it will resolve these things in the system provided dynamic
    # libraries (likely linked to stock zlib)
    target_link_libraries(benchmark_zlib_apps libpng.a zlib-ng-static benchmark::benchmark)
endif()
