cmake_minimum_required(VERSION 3.12)

include(FetchContent)

enable_language(CXX)

# Google test requires at least C++11
set(CMAKE_CXX_STANDARD 11)

# Google test requires MSAN instrumented LLVM C++ libraries
if(WITH_SANITIZER STREQUAL "Memory")
    if(NOT DEFINED ENV{LLVM_BUILD_DIR})
        message(FATAL_ERROR "MSAN instrumented C++ libraries required!")
    endif()

    # Must set include and compile options before fetching googletest
    include_directories($ENV{LLVM_BUILD_DIR}/include $ENV{LLVM_BUILD_DIR}/include/c++/v1)
    add_compile_options(-stdlib=libc++ -g)
endif()

# Prevent overriding the parent project's compiler/linker settings for Windows
set(gtest_force_shared_crt ON CACHE BOOL
    "Use shared (DLL) run-time lib even when Google Test is built as static lib." FORCE)
# Disable pthreads for simplicity
set(gtest_disable_pthreads ON CACHE BOOL
    "Disable uses of pthreads in gtest." FORCE)

# Allow specifying alternative Google test repository
if(NOT DEFINED GTEST_REPOSITORY)
    set(GTEST_REPOSITORY https://github.com/google/googletest.git)
endif()
if(NOT DEFINED GTEST_TAG)
    # Use older version of Google test to support older versions of GCC
    if (CMAKE_CXX_COMPILER_ID MATCHES "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS_EQUAL 5.3)
        set(GTEST_TAG release-1.10.0)
    else()
        set(GTEST_TAG main)
    endif()
endif()

# Fetch Google test source code from official repository
FetchContent_Declare(googletest
    GIT_REPOSITORY ${GTEST_REPOSITORY}
    GIT_TAG ${GTEST_TAG})

FetchContent_GetProperties(googletest)
if(NOT googletest_POPULATED)
    FetchContent_Populate(googletest)
    add_subdirectory(${googletest_SOURCE_DIR} ${googletest_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()

file(GLOB TEST_SRCS
    LIST_DIRECTORIES false
    RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/test_*.cc)

add_executable(gtest_zlib ${TEST_SRCS})

target_include_directories(gtest_zlib PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_BINARY_DIR})

if(WITH_SANITIZER STREQUAL "Memory")
    target_link_directories(gtest_zlib PRIVATE $ENV{LLVM_BUILD_DIR}/lib)
    target_link_options(gtest_zlib PRIVATE
        -stdlib=libc++
        -lc++abi
        -fsanitize=memory
        -fsanitize-memory-track-origins)
endif()

target_link_libraries(gtest_zlib zlibstatic gtest)

if(ZLIB_ENABLE_TESTS)
    add_test(NAME gtest_zlib
        COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} $<TARGET_FILE:gtest_zlib>)
endif()
