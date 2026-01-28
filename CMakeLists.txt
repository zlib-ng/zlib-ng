cmake_minimum_required(VERSION 3.14...4.2.1)
message(STATUS "Using CMake version ${CMAKE_VERSION}")

# If not specified on the command line, enable C11 as the default
# Configuration items that affect the global compiler environment standards
# should be issued before the "project" command.
if(NOT DEFINED CMAKE_C_STANDARD)
    set(CMAKE_C_STANDARD 11)          # The C standard whose features are requested to build this target
endif()
if(NOT DEFINED CMAKE_C_STANDARD_REQUIRED)
    set(CMAKE_C_STANDARD_REQUIRED ON) # Boolean describing whether the value of C_STANDARD is a requirement
endif()
if(NOT DEFINED CMAKE_C_EXTENSIONS)
    set(CMAKE_C_EXTENSIONS OFF)       # Boolean specifying whether compiler specific extensions are requested
endif()
set(VALID_C_STANDARDS "99" "11")
if(CMAKE_VERSION VERSION_GREATER 3.20)
    list(APPEND VALID_C_STANDARDS "17" "23")
endif()
if(NOT CMAKE_C_STANDARD IN_LIST VALID_C_STANDARDS)
    MESSAGE(FATAL_ERROR "CMAKE_C_STANDARD:STRING=${CMAKE_C_STANDARD} not in known standards list\n ${VALID_C_STANDARDS}")
endif()

# Parse the full version number from zlib.h.in and include in ZLIB_FULL_VERSION
file(READ ${CMAKE_CURRENT_SOURCE_DIR}/zlib.h.in _zlib_h_contents)
string(REGEX REPLACE ".*#define[ \t]+ZLIB_VERSION[ \t]+\"([0-9]+.[0-9]+.[0-9]+).*\".*"
        "\\1" ZLIB_HEADER_VERSION ${_zlib_h_contents})
string(REGEX REPLACE ".*#define[ \t]+ZLIBNG_VERSION[ \t]+\"([-0-9A-Za-z.]+)\".*"
        "\\1" ZLIBNG_HEADER_VERSION ${_zlib_h_contents})
message(STATUS "ZLIB_HEADER_VERSION: ${ZLIB_HEADER_VERSION}")
message(STATUS "ZLIBNG_HEADER_VERSION: ${ZLIBNG_HEADER_VERSION}")

project(zlib VERSION ${ZLIB_HEADER_VERSION} LANGUAGES C)

include(CheckTypeSize)
include(CheckSymbolExists)
include(CheckFunctionExists)
include(CheckIncludeFile)
include(CheckCSourceCompiles)
include(CheckCSourceRuns)
include(CheckCCompilerFlag)
include(CMakeDependentOption)
include(CMakePackageConfigHelpers)
include(CTest)
include(FeatureSummary)

# We need to enable C++ before trying to check for coverage
option(BUILD_TESTING "Build test binaries." ON) # Depends on CMP0077 (3.13)
if (DEFINED ZLIB_ENABLE_TESTS)
    message(DEPRECATION "ZLIB_ENABLE_TESTS is deprecated. Please use BUILD_TESTING instead.")
    if (NOT ZLIB_ENABLE_TESTS)
        set(BUILD_TESTING OFF)
    endif()
endif()
cmake_dependent_option(WITH_GTEST "Build gtest_zlib" ON "BUILD_TESTING" OFF)
cmake_dependent_option(WITH_FUZZERS "Build test/fuzz" OFF "BUILD_TESTING" OFF)
cmake_dependent_option(WITH_BENCHMARKS "Build test/benchmarks" OFF "BUILD_TESTING" OFF)
cmake_dependent_option(WITH_BENCHMARK_APPS "Build application benchmarks" OFF "BUILD_TESTING" OFF)

if(WITH_GTEST OR WITH_FUZZERS OR WITH_BENCHMARKS)
  enable_language(CXX)
endif()

include(cmake/detect-arch.cmake)
include(cmake/detect-install-dirs.cmake)
include(cmake/detect-coverage.cmake)
include(cmake/detect-intrinsics.cmake)
include(cmake/detect-sanitizer.cmake)
include(cmake/fallback-macros.cmake)

if(CMAKE_TOOLCHAIN_FILE)
    message(STATUS "Using CMake toolchain: ${CMAKE_TOOLCHAIN_FILE}")
endif()

# Make sure we use an appropriate BUILD_TYPE by default, "Release" to be exact
# this should select the maximum generic optimisation on the current platform (i.e. -O3 for gcc/clang)
get_property(GENERATOR_IS_MULTI_CONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(NOT GENERATOR_IS_MULTI_CONFIG)
    if(NOT CMAKE_BUILD_TYPE)
        set(CMAKE_BUILD_TYPE "Release" CACHE STRING
            "Choose the type of build, standard options are: Debug Release RelWithDebInfo MinSizeRel."
            FORCE)
        add_feature_info(CMAKE_BUILD_TYPE 1 "Build type: ${CMAKE_BUILD_TYPE} (default)")
    else()
        add_feature_info(CMAKE_BUILD_TYPE 1 "Build type: ${CMAKE_BUILD_TYPE} (selected)")
    endif()
endif()

#
# Options parsing
#
option(WITH_GZFILEOP "Compile with support for gzFile related functions" ON)
option(ZLIB_COMPAT "Compile with zlib compatible API" OFF)
option(ZLIB_ALIASES "Provide zlib compatible CMake targets" ON)
option(WITH_OPTIM "Build with optimisation" ON)
option(WITH_ALL_FALLBACKS "Build all generic fallback functions (Useful for Gbench)" OFF)
option(WITH_REDUCED_MEM "Reduced memory usage for special cases (reduces performance)" OFF)
option(WITH_NEW_STRATEGIES "Use new strategies" ON)
option(WITH_CRC32_CHORBA "Enable optimized CRC32 algorithm Chorba" ON)
option(WITH_NATIVE_INSTRUCTIONS
    "Instruct the compiler to use the full instruction set on this host (gcc/clang -march=native)" OFF)
option(WITH_RUNTIME_CPU_DETECTION "Build with runtime detection of CPU architecture" ON)
option(WITH_MAINTAINER_WARNINGS "Build with project maintainer warnings" OFF)
option(WITH_CODE_COVERAGE "Enable code coverage reporting" OFF)
option(WITH_INFLATE_STRICT "Build with strict inflate distance checking" OFF)
option(WITH_INFLATE_ALLOW_INVALID_DIST "Build with zero fill for inflate invalid distances" OFF)

set(ZLIB_SYMBOL_PREFIX "" CACHE STRING "Give this prefix to all publicly exported symbols.
Useful when embedding into a larger library.
Default is no prefix (empty prefix).")

# Add multi-choice option
set(WITH_SANITIZER AUTO CACHE STRING "Enable sanitizer support")
set_property(CACHE WITH_SANITIZER PROPERTY STRINGS "None" "Memory" "Address" "Undefined" "Thread")

if(BASEARCH_ARM_FOUND)
    cmake_dependent_option(WITH_ARMV6 "Build with ARMv6 SIMD" ON ARCH_32BIT OFF)
    option(WITH_ARMV8 "Build with ARMv8 intrinsics" ON)
    option(WITH_NEON "Build with NEON intrinsics" ON)

    # Provided for backward compatibility
    if(DEFINED WITH_ACLE)
        set(WITH_ARMV8 ${WITH_ACLE})
    endif()
elseif(BASEARCH_PPC_FOUND)
    option(WITH_ALTIVEC "Build with AltiVec (VMX) optimisations for PowerPC" ON)
    option(WITH_POWER8 "Build with optimisations for POWER8" ON)
    option(WITH_POWER9 "Build with optimisations for POWER9" ON)
elseif(BASEARCH_RISCV_FOUND)
    option(WITH_RVV "Build with RVV intrinsics" ON)
    option(WITH_RISCV_ZBC "Build with RISCV ZBC" ON)
elseif(BASEARCH_S360_FOUND)
    option(WITH_DFLTCC_DEFLATE "Build with DFLTCC intrinsics for compression on IBM Z" OFF)
    option(WITH_DFLTCC_INFLATE "Build with DFLTCC intrinsics for decompression on IBM Z" OFF)
    option(WITH_CRC32_VX "Build with vectorized CRC32 on IBM Z" ON)
elseif(BASEARCH_LOONGARCH_FOUND)
    option(WITH_CRC32_LA "Build with vectorized CRC32 on LoongArch" ON)
    option(WITH_LSX "Build with vectorized LSX on LoongArch" ON)
    cmake_dependent_option(WITH_LASX "Build with vectorized LASX on LoongArch" ON "WITH_LSX" OFF)
elseif(BASEARCH_X86_FOUND)
    option(WITH_SSE2 "Build with SSE2" ON)
    cmake_dependent_option(WITH_SSSE3 "Build with SSSE3" ON "WITH_SSE2" OFF)
    cmake_dependent_option(WITH_SSE41 "Build with SSE41" ON "WITH_SSSE3" OFF)
    cmake_dependent_option(WITH_SSE42 "Build with SSE42" ON "WITH_SSE41" OFF)
    cmake_dependent_option(WITH_PCLMULQDQ "Build with PCLMULQDQ" ON "WITH_SSE42" OFF)
    cmake_dependent_option(WITH_AVX2 "Build with AVX2" ON "WITH_SSE42" OFF)
    cmake_dependent_option(WITH_AVX512 "Build with AVX512" ON "WITH_AVX2" OFF)
    cmake_dependent_option(WITH_AVX512VNNI "Build with AVX512 VNNI extensions" ON "WITH_AVX512" OFF)
    cmake_dependent_option(WITH_VPCLMULQDQ "Build with VPCLMULQDQ" ON "WITH_PCLMULQDQ;WITH_AVX512" OFF)
endif()

option(INSTALL_UTILS "Copy minigzip and minideflate during install" OFF)

mark_as_advanced(FORCE
    ZLIB_SYMBOL_PREFIX
    WITH_REDUCED_MEM
    WITH_CRC32_CHORBA
    WITH_ALL_FALLBACKS
    WITH_ARMV8 WITH_NEON
    WITH_ARMV6
    WITH_DFLTCC_DEFLATE
    WITH_DFLTCC_INFLATE
    WITH_CRC32_VX
    WITH_AVX2 WITH_SSE2
    WITH_SSSE3 WITH_SSE41 WITH_SSE42
    WITH_PCLMULQDQ
    WITH_ALTIVEC
    WITH_POWER8
    WITH_POWER9
    WITH_RVV
    WITH_RISCV_ZBC
    WITH_CRC32_LA
    WITH_LSX
    WITH_LASX
    WITH_INFLATE_STRICT
    WITH_INFLATE_ALLOW_INVALID_DIST
    INSTALL_UTILS
    )

if(ZLIB_COMPAT)
    add_definitions(-DZLIB_COMPAT)
    set(WITH_GZFILEOP ON)
    set(SUFFIX "")
    set(ZLIB_FULL_VERSION ${ZLIB_HEADER_VERSION}.zlib-ng)
    set(EXPORT_NAME ZLIB)
else()
    set(SUFFIX "-ng")
    set(ZLIB_FULL_VERSION ${ZLIBNG_HEADER_VERSION})
    set(EXPORT_NAME zlib-ng)
endif()

if(WITH_GZFILEOP)
    add_definitions(-DWITH_GZFILEOP)
endif()

if(NOT WITH_CRC32_CHORBA)
    add_definitions(-DWITHOUT_CHORBA)
endif()

if(CMAKE_C_COMPILER_ID MATCHES "^Intel")
    if(CMAKE_HOST_UNIX)
        set(WARNFLAGS -Wall)
        set(WARNFLAGS_MAINTAINER -Wall -Wcheck -Wremarks)
        set(WARNFLAGS_DISABLE)
    else()
        set(WARNFLAGS /Wall)
        set(WARNFLAGS_MAINTAINER /W5)
        set(WARNFLAGS_DISABLE)
    endif()
    check_c_compiler_flag(-diag-disable=10441 HAVE_DIAG_10441)
    if(HAVE_DIAG_10441)
        list(APPEND WARNFLAGS_DISABLE "-diag-disable=10441")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -diag-disable=10441")
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -diag-disable=10441")
    endif()
elseif(MSVC)
    # Minimum supported MSVC version is 1800 = Visual Studio 12.0/2013
    # See also https://cmake.org/cmake/help/latest/variable/MSVC_VERSION.html
    if(MSVC_VERSION VERSION_LESS 1800)
        message(SEND_ERROR "Unsupported Visual Studio compiler version (requires 2013 or later).")
    endif()
    if(MSVC_VERSION VERSION_LESS 1930)
        message(STATUS "Old Visual Studio compiler version, disabling SSE2/SSE4.1 Chorba variants (requires 2022 or later).")
        add_definitions(-DWITHOUT_CHORBA_SSE)
    endif()
    # TODO. ICC can be used through MSVC. I'm not sure if we'd ever see that combination
    # (who'd use cmake from an IDE...) but checking for ICC before checking for MSVC should
    # avoid mistakes.
    # /Oi ?
    set(WARNFLAGS /W3 /w34242 /WX)
    set(WARNFLAGS_MAINTAINER /W4)
    set(WARNFLAGS_DISABLE /wd4206 /wd4054 /wd4324)
    if(BASEARCH_ARM_FOUND)
        add_definitions(-D_ARM_WINAPI_PARTITION_DESKTOP_SDK_AVAILABLE)
        if(ARCH_32BIT)
            set(NEONFLAG "/arch:VFPv4")
        endif()
    endif()

    # Workaround for MSVC. By default MSVC does not define the __SSE*__ macros.
    # We explicitly define the __SSE__ and __SSE2__ macros, since starting with MSVS 2012
    # the default instruction set is SSE2.
    if(BASEARCH_X86_FOUND)
        add_definitions(
            -D__SSE__=1
            -D__SSE2__=1
        )
    endif()
elseif(CMAKE_C_COMPILER_ID MATCHES "GNU" OR CMAKE_C_COMPILER_ID MATCHES "Clang" OR CMAKE_C_COMPILER_ID MATCHES "NVHPC")
    # Enable warnings in GCC and Clang
    set(WARNFLAGS -Wall)
    set(WARNFLAGS_MAINTAINER -Wextra)
    set(WARNFLAGS_DISABLE)
    # Check whether -fno-lto is available
    set(CMAKE_REQUIRED_FLAGS "-fno-lto")
    check_c_source_compiles(
        "int main() { return 0; }"
        FNO_LTO_AVAILABLE FAIL_REGEX "not supported")
    set(CMAKE_REQUIRED_FLAGS)
    if(FNO_LTO_AVAILABLE)
        set(ZNOLTOFLAG "-fno-lto")
    endif()
    if(NOT WITH_NATIVE_INSTRUCTIONS)
        if(BASEARCH_ARM_FOUND)
            if(ARCH_32BIT AND NOT CMAKE_C_FLAGS MATCHES "-mfloat-abi")
                # Auto-detect support for ARM floating point ABI
                check_include_file(features.h HAVE_FEATURES_H)
                if(HAVE_FEATURES_H)
                    set(CMAKE_REQUIRED_FLAGS -mfloat-abi=softfp)
                    check_c_source_compiles(
                        "#include <features.h>
                        int main() { return 0; }"
                        HAVE_FLOATABI_SOFTFP)
                    if(HAVE_FLOATABI_SOFTFP)
                        set(FLOATABI -mfloat-abi=softfp)
                    else()
                        set(CMAKE_REQUIRED_FLAGS -mfloat-abi=hard)
                        check_c_source_compiles(
                            "#include <features.h>
                            int main() { return 0; }"
                            HAVE_FLOATABI_HARD)
                        if(HAVE_FLOATABI_HARD)
                            set(FLOATABI -mfloat-abi=hard)
                        endif()
                    endif()
                    set(CMAKE_REQUIRED_FLAGS)
                endif()
                if(FLOATABI)
                    message(STATUS "ARM floating point arch: ${FLOATABI}")
                    add_compile_options(${FLOATABI})
                else()
                    message(STATUS "ARM floating point arch not auto-detected")
                endif()
            endif()
        elseif(BASEARCH_X86_FOUND AND CMAKE_C_COMPILER_ID MATCHES "NVHPC")
            # nVidia compiler defaults to native build, so set target to any x86-64 processor
            add_compile_options(-tp px)
        endif()
        # Disable LTO unless Native Instructions are enabled
        if(FNO_LTO_AVAILABLE)
            set(NOLTOFLAG ${ZNOLTOFLAG})
        endif()
    endif()
    if(MINGW)
        # Add `-Wno-pedantic-ms-format` only if the toolchain supports it
        check_c_compiler_flag(-Wno-pedantic-ms-format HAVE_NO_PEDANTIC_MS_FORMAT)
        if(HAVE_NO_PEDANTIC_MS_FORMAT)
            list(APPEND WARNFLAGS_DISABLE -Wno-pedantic-ms-format)
        endif()
    endif()
endif()

# Set native march/mcpu
if(WITH_NATIVE_INSTRUCTIONS)
    if(NATIVE_ARCH_OVERRIDE)
        message(STATUS "WARNING: WITH_NATIVE_INSTRUCTIONS enabled, but running with NATIVE_ARCH_OVERRIDE: ${NATIVE_ARCH_OVERRIDE}")
        set(NATIVEFLAG "${NATIVE_ARCH_OVERRIDE}")
    else()
        if(CMAKE_C_COMPILER_ID MATCHES "GNU" OR CMAKE_C_COMPILER_ID MATCHES "Clang")
            check_c_compiler_flag(-march=native HAVE_MARCH_NATIVE)
            if(HAVE_MARCH_NATIVE)
                set(NATIVEFLAG "-march=native")
            else()
                check_c_compiler_flag(-mcpu=native HAVE_MCPU_NATIVE)
                if(HAVE_MCPU_NATIVE)
                    set(NATIVEFLAG "-mcpu=native")
                endif()
            endif()
            # Fall through
        endif()
        if(CMAKE_C_COMPILER_ID MATCHES "NVHPC")
            check_c_compiler_flag("-tp native" HAVE_TP_NATIVE)
            if(HAVE_TP_NATIVE)
                set(NATIVEFLAG "-tp native")
            endif()
        endif()
    endif()
    if(NATIVEFLAG)
        # Apply flags to all source files and compilation checks
        if(WIN32)
            separate_arguments(NATIVEOPTIONS WINDOWS_COMMAND "${NATIVEFLAG}")
        else()
            separate_arguments(NATIVEOPTIONS UNIX_COMMAND "${NATIVEFLAG}")
        endif()
        add_compile_options(${NATIVEOPTIONS})
        set(WITH_RUNTIME_CPU_DETECTION OFF)
    else()
        message(STATUS "Ignoring WITH_NATIVE_INSTRUCTIONS; not implemented yet on this configuration")
        set(WITH_NATIVE_INSTRUCTIONS OFF)
    endif()
endif()

# Compile without functable or CPU detection
if(NOT WITH_RUNTIME_CPU_DETECTION)
    if(MSVC AND BASEARCH_X86_FOUND)
        message(STATUS "WARNING: Microsoft Visual Studio does not support compile time detection of CPU features for \"/arch\" before \"AVX\"")
        # Workaround for MSVC. By default MSVC does not define the __SSE*__ macros.
        # Fix it if AVX is enabled.
        set(CMAKE_REQUIRED_FLAGS "${NATIVEFLAG} ${ZNOLTOFLAG}")
        check_c_source_compiles(
            "#ifndef __AVX__
            #  error \"AVX is not enabled.\"
            #endif
            int main(void) { return 0; }"
            MSVC_IS_ENABLED_AVX
        )
        set(CMAKE_REQUIRED_FLAGS)
        if(MSVC_IS_ENABLED_AVX)
            add_definitions(
                -D__SSE3__=1
                -D__SSSE3__=1
                -D__SSE4_1__=1
                -D__SSE4_2__=1
                -D__PCLMUL__=1
            )
        endif()
    endif()
    add_definitions(-DDISABLE_RUNTIME_CPU_DETECTION)
endif()

# Force disable LTO if WITH_NATIVE_INSTRUCTIONS is not active
if(NOT WITH_NATIVE_INSTRUCTIONS)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION OFF)
    foreach(_cfg_name IN LISTS CMAKE_CONFIGURATION_TYPES)
        string(TOUPPER "${_cfg_name}" _cfg_name_uc)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_${_cfg_name_uc} OFF)
    endforeach()
endif()

# Apply warning compiler flags
if(WITH_MAINTAINER_WARNINGS)
    add_compile_options(${WARNFLAGS} ${WARNFLAGS_MAINTAINER} ${WARNFLAGS_DISABLE})
else()
    add_compile_options(${WARNFLAGS} ${WARNFLAGS_DISABLE})
endif()

# Set code coverage compiler flags
if(WITH_CODE_COVERAGE)
    add_code_coverage()
endif()

# Replace optimization level 3 added by default with level 2
if(NOT WITH_CODE_COVERAGE AND NOT MSVC AND NOT CMAKE_C_FLAGS MATCHES "([\\/\\-]O)3")
    string(REGEX REPLACE "([\\/\\-]O)3" "\\12"
        CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}")
endif()

# Force Visual C++ to use UTF-8
if(MSVC)
    if (NOT CMAKE_C_FLAGS MATCHES "[\\/\\-]utf-8")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /utf-8")
    endif()
    if (NOT CMAKE_CXX_FLAGS MATCHES "[\\/\\-]utf-8")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /utf-8")
    endif()
endif()

#
# Additional flags for features checking
#
set(ADDITIONAL_CHECK_FLAGS )
if(APPLE)
    check_c_compiler_flag(-Werror=unguarded-availability HAVE_W_ERROR_UNGUARDED_AVAILABILITY)
    if(HAVE_W_ERROR_UNGUARDED_AVAILABILITY)
        set(ADDITIONAL_CHECK_FLAGS "${ADDITIONAL_CHECK_FLAGS} -Werror=unguarded-availability")
    endif()
    check_c_compiler_flag(-Werror=unguarded-availability-new HAVE_W_ERROR_UNGUARDED_AVAILABILITY_NEW)
    if(HAVE_W_ERROR_UNGUARDED_AVAILABILITY_NEW)
        set(ADDITIONAL_CHECK_FLAGS "${ADDITIONAL_CHECK_FLAGS} -Werror=unguarded-availability-new")
    endif()
    check_c_compiler_flag(-Werror=unused-command-line-argument HAVE_W_ERROR_UNUSED_COMMAND_LINE_ARGUMENT)
    if(HAVE_W_ERROR_UNUSED_COMMAND_LINE_ARGUMENT)
        set(ADDITIONAL_CHECK_FLAGS "${ADDITIONAL_CHECK_FLAGS} -Werror=unused-command-line-argument")
    endif()
endif()

#
# Check for standard/system includes
#
check_include_file(arm_acle.h  HAVE_ARM_ACLE_H)
if(HAVE_ARM_ACLE_H)
    add_definitions(-DHAVE_ARM_ACLE_H)
endif()
check_include_file(sys/auxv.h  HAVE_SYS_AUXV_H)
if(HAVE_SYS_AUXV_H)
    add_definitions(-DHAVE_SYS_AUXV_H)
endif()
check_include_file(sys/sdt.h   HAVE_SYS_SDT_H)
if(HAVE_SYS_SDT_H)
    add_definitions(-DHAVE_SYS_SDT_H)
endif()
check_include_file(unistd.h    HAVE_UNISTD_H)

#
# Check for Linux includes
#
check_include_file(linux/auxvec.h HAVE_LINUX_AUXVEC_H)
if(HAVE_LINUX_AUXVEC_H)
    add_definitions(-DHAVE_LINUX_AUXVEC_H)
endif()
check_include_file(asm/hwprobe.h  HAVE_ASM_HWPROBE_H)
if(HAVE_ASM_HWPROBE_H)
    add_definitions(-DHAVE_ASM_HWPROBE_H)
endif()

#
# Check to see if we have large file support
#
set(CMAKE_REQUIRED_DEFINITIONS -D_LARGEFILE64_SOURCE=1 -D__USE_LARGEFILE64)
check_type_size(off64_t OFF64_T)
if(HAVE_OFF64_T)
    add_definitions(-D_LARGEFILE64_SOURCE=1 -D__USE_LARGEFILE64)
else()
    check_type_size(_off64_t _OFF64_T)
    if(HAVE__OFF64_T)
        add_definitions(-D_LARGEFILE64_SOURCE=1 -D__USE_LARGEFILE64)
    else()
        check_type_size(__off64_t __OFF64_T)
    endif()
endif()
set(CMAKE_REQUIRED_DEFINITIONS) # clear variable

#
# Check for fseeko and other optional functions
#
set(CMAKE_REQUIRED_FLAGS "${ADDITIONAL_CHECK_FLAGS}")
check_function_exists(fseeko HAVE_FSEEKO)
if(NOT HAVE_FSEEKO)
    add_definitions(-DNO_FSEEKO)
endif()
set(CMAKE_REQUIRED_FLAGS)

set(CMAKE_REQUIRED_FLAGS "${ADDITIONAL_CHECK_FLAGS}")
check_function_exists(strerror HAVE_STRERROR)
if(NOT HAVE_STRERROR)
    add_definitions(-DNO_STRERROR)
endif()
set(CMAKE_REQUIRED_FLAGS)

#
# Check for x86 __cpuid / __cpuid_count support: GNU extensions
# gcc/clang and MSVC have __cpuid, so check __cpuid_count instead
check_symbol_exists(__cpuid_count cpuid.h HAVE_CPUID_GNU)
if(HAVE_CPUID_GNU)
    add_definitions(-DHAVE_CPUID_GNU)
endif()
set(CMAKE_REQUIRED_DEFINITIONS)

#
# Check for x86 __cpuid / __cpuidex support: MSVC extensions
# gcc/clang and MSVC have __cpuid, so check __cpuidex instead
# check_symbol_exists() does not work for intrinsics

check_c_source_compiles(
"
#include <intrin.h>
static inline void cpuid_ms(int info, unsigned int *eax, unsigned int *ebx, unsigned int *ecx, unsigned int *edx) {
    unsigned int registers[4];
    __cpuid((int *)registers, info);
    *eax = registers[0];
    *ebx = registers[1];
    *ecx = registers[2];
    *edx = registers[3];
}

int main(void) {
  unsigned int eax, ebx, ecx, edx;
  cpuid_ms(0, &eax, &ebx, &ecx, &edx);
  return eax;
}"
HAVE_CPUID_MS)

if(HAVE_CPUID_MS)
    add_definitions(-DHAVE_CPUID_MS)
endif()
set(CMAKE_REQUIRED_DEFINITIONS)

#
# Check whether a sanitizer was requested
#
if(WITH_SANITIZER STREQUAL "Address")
    add_address_sanitizer()
elseif(WITH_SANITIZER STREQUAL "Memory")
    add_memory_sanitizer()
elseif(WITH_SANITIZER STREQUAL "Thread")
    add_thread_sanitizer()
elseif(WITH_SANITIZER STREQUAL "Undefined")
    add_undefined_sanitizer()
endif()

#
# Check whether compiler supports -fno-semantic-interposition parameter
#
set(CMAKE_REQUIRED_FLAGS "${ADDITIONAL_CHECK_FLAGS}")
check_c_compiler_flag(-fno-semantic-interposition HAVE_NO_INTERPOSITION)
set(CMAKE_REQUIRED_FLAGS)

#
# Check if we can hide zlib internal symbols that are linked between separate source files using hidden
#
check_c_source_compiles(
    "#define Z_INTERNAL __attribute__((visibility (\"hidden\")))
    int Z_INTERNAL foo;
    int main() {
        return 0;
    }"
    HAVE_ATTRIBUTE_VISIBILITY_HIDDEN FAIL_REGEX "visibility")
if(HAVE_ATTRIBUTE_VISIBILITY_HIDDEN)
    add_definitions(-DHAVE_VISIBILITY_HIDDEN)
endif()

#
# Check if we can hide zlib internal symbols that are linked between separate source files using internal
#
check_c_source_compiles(
    "#define Z_INTERNAL __attribute__((visibility (\"internal\")))
    int Z_INTERNAL foo;
    int main() {
        return 0;
    }"
    HAVE_ATTRIBUTE_VISIBILITY_INTERNAL FAIL_REGEX "visibility")
if(HAVE_ATTRIBUTE_VISIBILITY_INTERNAL)
    add_definitions(-DHAVE_VISIBILITY_INTERNAL)
endif()

#
# Check for __attribute__((aligned(x))) support in the compiler
#
check_c_source_compiles(
    "int main(void) {
        __attribute__((aligned(8))) int test = 0;
        (void)test;
        return 0;
    }"
    HAVE_ATTRIBUTE_ALIGNED FAIL_REGEX "aligned")
if(HAVE_ATTRIBUTE_ALIGNED)
    add_definitions(-DHAVE_ATTRIBUTE_ALIGNED)
endif()

#
# Check for __builtin_assume_aligned(x,n) support in the compiler
#
set(CMAKE_REQUIRED_FLAGS ${ZNOLTOFLAG})
check_c_source_compiles(
    "char *test(char *buffer) {
        char *abuffer = __builtin_assume_aligned(buffer,64);
        return abuffer;
    }
    int main() {
        return 0;
    }"
    HAVE_BUILTIN_ASSUME_ALIGNED)
if(HAVE_BUILTIN_ASSUME_ALIGNED)
    add_definitions(-DHAVE_BUILTIN_ASSUME_ALIGNED)
endif()
set(CMAKE_REQUIRED_FLAGS)

#
# check for __builtin_ctz() support in the compiler
#
set(CMAKE_REQUIRED_FLAGS ${ZNOLTOFLAG})
check_c_source_compiles(
    "int main(void) {
        unsigned int zero = 0;
        long test = __builtin_ctz(zero);
        (void)test;
        return 0;
    }"
    HAVE_BUILTIN_CTZ
)
if(HAVE_BUILTIN_CTZ)
    add_definitions(-DHAVE_BUILTIN_CTZ)
endif()
set(CMAKE_REQUIRED_FLAGS)

#
# check for __builtin_ctzll() support in the compiler
#
set(CMAKE_REQUIRED_FLAGS ${ZNOLTOFLAG})
check_c_source_compiles(
    "int main(void) {
        unsigned int zero = 0;
        long test = __builtin_ctzll(zero);
        (void)test;
        return 0;
    }"
    HAVE_BUILTIN_CTZLL
)
if(HAVE_BUILTIN_CTZLL)
    add_definitions(-DHAVE_BUILTIN_CTZLL)
endif()
set(CMAKE_REQUIRED_FLAGS)

#
# check for ptrdiff_t support
#
check_c_source_compiles(
    "#include <stddef.h>
     int main() {
         ptrdiff_t *a;
         (void)a;
         return 0;
    }"
    HAVE_PTRDIFF_T
)
if(NOT HAVE_PTRDIFF_T)
    set(NEED_PTRDIFF_T 1)

    check_type_size("void *" SIZEOF_DATA_PTR)
    message(STATUS "sizeof(void *) is ${SIZEOF_DATA_PTR} bytes")

    if(${SIZEOF_DATA_PTR} MATCHES "4")
        set(PTRDIFF_TYPE "uint32_t")
    elseif(${SIZEOF_DATA_PTR} MATCHES "8")
        set(PTRDIFF_TYPE "uint64_t")
    else()
        message(FATAL_ERROR "sizeof(void *) is neither 32 nor 64 bit")
    endif()
endif()

add_compile_options($<$<CONFIG:Debug>:-DZLIB_DEBUG>)

if(MSVC)
    set(CMAKE_DEBUG_POSTFIX "d")
    add_definitions(-D_CRT_SECURE_NO_DEPRECATE)
    add_definitions(-D_CRT_NONSTDC_NO_DEPRECATE)
endif()

#
# Enable deflate_quick at level 1
#
if(NOT WITH_NEW_STRATEGIES)
    add_definitions(-DNO_QUICK_STRATEGY)
endif()
#
# Enable deflate_medium at level 4-6
#
if(NOT WITH_NEW_STRATEGIES)
    add_definitions(-DNO_MEDIUM_STRATEGY)
endif()
#
# Enable inflate compilation options
#
if(WITH_INFLATE_STRICT)
    add_definitions(-DINFLATE_STRICT)
    message(STATUS "Inflate strict distance checking enabled")
endif()
if(WITH_INFLATE_ALLOW_INVALID_DIST)
    add_definitions(-DINFLATE_ALLOW_INVALID_DISTANCE_TOOFAR_ARRR)
    message(STATUS "Inflate zero data for invalid distances enabled")
endif()
#
# Enable reduced memory configuration
#
if(WITH_REDUCED_MEM)
    add_definitions(-DHASH_SIZE=32768u -DGZBUFSIZE=8192 -DNO_LIT_MEM)
    message(STATUS "Configured for reduced memory environment")
endif()

set(GENERIC_ARCHDIR "arch/generic")

set(ZLIB_ARCH_SRCS)
set(ZLIB_ARCH_HDRS ${GENERIC_ARCHDIR}/generic_functions.h)

if(BASEARCH_ARM_FOUND)
    set(ARCHDIR "arch/arm")
elseif(BASEARCH_PPC_FOUND)
    set(ARCHDIR "arch/power")
elseif(BASEARCH_RISCV_FOUND)
    set(ARCHDIR "arch/riscv")
elseif(BASEARCH_S360_FOUND)
    set(ARCHDIR "arch/s390")
elseif(BASEARCH_LOONGARCH_FOUND)
    set(ARCHDIR "arch/loongarch")
elseif(BASEARCH_X86_FOUND)
    set(ARCHDIR "arch/x86")
    if(ARCH_32BIT)
        add_feature_info(SSE2 1 "Support the SSE2 instruction set, using \"${SSE2FLAG}\"")
    endif()
else()
    set(ARCHDIR ${GENERIC_ARCHDIR})
    message(STATUS "No optimized architecture: using ${ARCHDIR}")
endif()

if(WITH_OPTIM)
    add_definitions(-DWITH_OPTIM)
    if(BASEARCH_ARM_FOUND)
        add_definitions(-DARM_FEATURES)
        list(APPEND ZLIB_ARCH_HDRS ${ARCHDIR}/arm_functions.h)
        if(WITH_RUNTIME_CPU_DETECTION)
            list(APPEND ZLIB_ARCH_HDRS ${ARCHDIR}/arm_features.h)
            list(APPEND ZLIB_ARCH_SRCS ${ARCHDIR}/arm_features.c)
        endif()

        if(WITH_NEON)
            check_neon_compiler_flag()
            if(NEON_AVAILABLE)
                add_definitions(-DARM_NEON)
                set(NEON_SRCS ${ARCHDIR}/adler32_neon.c ${ARCHDIR}/chunkset_neon.c
                    ${ARCHDIR}/compare256_neon.c ${ARCHDIR}/slide_hash_neon.c)
                list(APPEND ZLIB_ARCH_SRCS ${NEON_SRCS})
                set_property(SOURCE ${NEON_SRCS} PROPERTY COMPILE_FLAGS "${NEONFLAG} ${NOLTOFLAG}")
                if(MSVC)
                    add_definitions(-D__ARM_NEON__)
                endif()
                add_feature_info(NEON_ADLER32 1 "Support NEON instructions in adler32, using \"${NEONFLAG}\"")
                add_feature_info(NEON_SLIDEHASH 1 "Support NEON instructions in slide_hash, using \"${NEONFLAG}\"")
                check_neon_ld4_intrinsics()
                if(NEON_HAS_LD4)
                    add_definitions(-DARM_NEON_HASLD4)
                endif()
            else()
                set(WITH_NEON OFF)
            endif()
        endif()
        if(WITH_ARMV6)
            check_armv6_compiler_flag()
            if(HAVE_ARMV6_INLINE_ASM OR HAVE_ARMV6_INTRIN)
                add_definitions(-DARM_SIMD)
                set(ARMV6_SRCS ${ARCHDIR}/slide_hash_armv6.c)
                set_property(SOURCE ${ARMV6_SRCS} PROPERTY COMPILE_FLAGS "${ARMV6FLAG} ${NOLTOFLAG}")
                list(APPEND ZLIB_ARCH_SRCS ${ARMV6_SRCS})
                add_feature_info(ARMV6 1 "Support ARMv6 SIMD instructions in slide_hash, using \"${ARMV6FLAG}\"")
                if(HAVE_ARMV6_INTRIN)
                    add_definitions(-DARM_SIMD_INTRIN)
                endif()
            else()
                set(WITH_ARMV6 OFF)
            endif()
        else()
            set(WITH_ARMV6 OFF)
        endif()
        if(WITH_ARMV8)
            check_armv8_compiler_flag()
            if(HAVE_ARMV8_INLINE_ASM OR HAVE_ARMV8_INTRIN)
                add_definitions(-DARM_CRC32)
                set(ARMV8_SRCS ${ARCHDIR}/crc32_armv8.c)
                set_property(SOURCE ${ARMV8_SRCS} PROPERTY COMPILE_FLAGS "${ARMV8FLAG} ${NOLTOFLAG}")
                list(APPEND ZLIB_ARCH_SRCS ${ARMV8_SRCS})
                add_feature_info(ARMV8_CRC 1 "Support ARMv8 optimized CRC hash generation, using \"${ARMV8FLAG}\"")
                if(HAVE_ARMV8_INTRIN)
                    add_definitions(-DARM_CRC32_INTRIN)
                endif()

                # Check for PMULL support (requires NEON too)
                if(WITH_NEON AND NEON_AVAILABLE)
                    check_armv8_pmull_eor3_compiler_flag()
                    if(HAVE_ARMV8_PMULL_EOR3_INTRIN)
                        add_definitions(-DARM_PMULL_EOR3)
                        set(PCMULLEOR3_SRCS ${ARCHDIR}/crc32_armv8_pmull_eor3.c)
                        set_property(SOURCE ${PCMULLEOR3_SRCS} PROPERTY COMPILE_FLAGS "${PMULLEOR3FLAG} ${NOLTOFLAG}")
                        list(APPEND ZLIB_ARCH_SRCS ${PCMULLEOR3_SRCS})
                        add_feature_info(ARMV8_PMULL_EOR3 1 "Support ARMv8 PMULL + EOR3 instructions for CRC, using \"${PMULLEOR3FLAG}\"")
                    endif()
                endif()
            else()
                set(WITH_ARMV8 OFF)
            endif()
        else()
            set(WITH_ARMV8 OFF)
        endif()
    elseif(BASEARCH_PPC_FOUND)
        # Common arch detection code
        if(WITH_ALTIVEC)
            check_ppc_intrinsics()
        endif()
        if(WITH_POWER8)
            check_power8_intrinsics()
        endif()
        if(WITH_POWER9)
            check_power9_intrinsics()
        endif()
        if(POWER8_NEED_AUXVEC_H OR POWER9_NEED_AUXVEC_H)
            add_definitions(-DPOWER_NEED_AUXVEC_H)
        endif()
        if(HAVE_POWER8_INTRIN OR HAVE_POWER9_INTRIN)
            add_definitions(-DPOWER_FEATURES)
        endif()
        if(HAVE_VMX OR HAVE_POWER8_INTRIN OR HAVE_POWER9_INTRIN)
            list(APPEND ZLIB_ARCH_HDRS ${ARCHDIR}/power_functions.h)
            if(WITH_RUNTIME_CPU_DETECTION)
                list(APPEND ZLIB_ARCH_HDRS ${ARCHDIR}/power_features.h)
                list(APPEND ZLIB_ARCH_SRCS ${ARCHDIR}/power_features.c)
            endif()
        endif()
        # VMX specific options and files
        if(WITH_ALTIVEC)
            if(HAVE_VMX)
                add_definitions(-DPPC_FEATURES)
                if(HAVE_ALTIVEC)
                    add_definitions(-DPPC_VMX)
                    set(PPC_SRCS ${ARCHDIR}/adler32_vmx.c ${ARCHDIR}/slide_hash_vmx.c)
                    list(APPEND ZLIB_ARCH_SRCS ${PPC_SRCS})
                    add_feature_info(ALTIVEC 1 "Support the AltiVec instruction set, using \"-maltivec\"")
                    set_property(SOURCE ${PPC_SRCS} PROPERTY COMPILE_FLAGS "${PPCFLAGS}")
                else()
                    set(WITH_ALTIVEC OFF)
                endif()
            endif()
        endif()
        # Power8 specific options and files
        if(WITH_POWER8)
            if(HAVE_POWER8_INTRIN)
                add_definitions(-DPOWER8_VSX)
                set(POWER8_SRCS ${ARCHDIR}/adler32_power8.c ${ARCHDIR}/chunkset_power8.c ${ARCHDIR}/slide_hash_power8.c)
                if(ARCH_64BIT)
                    add_definitions(-DPOWER8_VSX_CRC32)
                    list(APPEND POWER8_SRCS ${ARCHDIR}/crc32_power8.c)
                endif()
                list(APPEND ZLIB_ARCH_SRCS ${POWER8_SRCS})
                set_property(SOURCE ${POWER8_SRCS} PROPERTY COMPILE_FLAGS "${POWER8FLAG} ${NOLTOFLAG}")
            else()
                set(WITH_POWER8 OFF)
            endif()
        endif()
        # Power9 specific options and files
        if(WITH_POWER9)
            if(HAVE_POWER9_INTRIN)
                add_definitions(-DPOWER9)
                set(POWER9_SRCS ${ARCHDIR}/compare256_power9.c)
                list(APPEND ZLIB_ARCH_SRCS ${POWER9_SRCS})
                set_property(SOURCE ${POWER9_SRCS} PROPERTY COMPILE_FLAGS "${POWER9FLAG} ${NOLTOFLAG}")
            else()
                set(WITH_POWER9 OFF)
            endif()
        endif()
    elseif(BASEARCH_RISCV_FOUND)
        if(WITH_RVV)
            check_rvv_intrinsics()
            if(HAVE_RVV_INTRIN)
                add_definitions(-DRISCV_FEATURES)
                add_definitions(-DRISCV_RVV)
                list(APPEND ZLIB_ARCH_HDRS ${ARCHDIR}/riscv_functions.h)
                if(WITH_RUNTIME_CPU_DETECTION)
                    list(APPEND ZLIB_ARCH_HDRS ${ARCHDIR}/riscv_features.h)
                    list(APPEND ZLIB_ARCH_SRCS ${ARCHDIR}/riscv_features.c)
                endif()
                # FIXME: we will not set compile flags for riscv_features.c when
                # the kernels update hwcap or hwprobe for riscv
                set(RVV_SRCS ${ARCHDIR}/adler32_rvv.c ${ARCHDIR}/chunkset_rvv.c ${ARCHDIR}/compare256_rvv.c ${ARCHDIR}/slide_hash_rvv.c)
                list(APPEND ZLIB_ARCH_SRCS ${RVV_SRCS})
                set_property(SOURCE ${RVV_SRCS} PROPERTY COMPILE_FLAGS "${RISCVFLAG} ${NOLTOFLAG}")
            else()
                set(WITH_RVV OFF)
            endif()
        endif()
        if(WITH_RISCV_ZBC)
            check_riscv_zbc_ext()
            if(HAVE_RISCV_ZBC)
                add_definitions(-DRISCV_CRC32_ZBC)
                set(ZBC_SRCS ${ARCHDIR}/crc32_zbc.c)
                list(APPEND ZLIB_ARCH_SRCS ${ZBC_SRCS})
                set_property(SOURCE ${ZBC_SRCS} PROPERTY COMPILE_FLAGS "-march=rv64gc_zbc ${NOLTOFLAG}")
                add_feature_info(RISCV_ZBC 1 "Support RISC-V Zbc extension for CRC32")
            else()
                set(WITH_RISCV_ZBC OFF)
            endif()
        endif()

        if(WITH_RUNTIME_CPU_DETECTION AND BASEARCH_RISCV_FOUND)
            if(WITH_RVV AND WITH_RISCV_ZBC AND HAVE_RVV_INTRIN AND HAVE_RISCV_ZBC)
                set_property(SOURCE ${ARCHDIR}/riscv_features.c PROPERTY COMPILE_FLAGS "${RISCVFLAG}_zbc ${NOLTOFLAG}")
            elseif(WITH_RVV AND HAVE_RVV_INTRIN)
                set_property(SOURCE ${ARCHDIR}/riscv_features.c PROPERTY COMPILE_FLAGS "${RISCVFLAG} ${NOLTOFLAG}")
            elseif(WITH_RISCV_ZBC AND HAVE_RISCV_ZBC)
                set_property(SOURCE ${ARCHDIR}/riscv_features.c PROPERTY COMPILE_FLAGS "${RISCVZBCFLAG} ${NOLTOFLAG}")
            endif()
        endif()
    elseif(BASEARCH_S360_FOUND)
        check_s390_intrinsics()
        if(HAVE_S390_INTRIN)
            add_definitions(-DS390_FEATURES)
            list(APPEND ZLIB_ARCH_HDRS ${ARCHDIR}/s390_functions.h)
            if(WITH_RUNTIME_CPU_DETECTION)
                list(APPEND ZLIB_ARCH_HDRS ${ARCHDIR}/s390_features.h)
                list(APPEND ZLIB_ARCH_SRCS ${ARCHDIR}/s390_features.c)
            endif()
        endif()
        if(WITH_DFLTCC_DEFLATE)
            add_definitions(-DS390_DFLTCC_DEFLATE)
            list(APPEND ZLIB_ARCH_SRCS ${ARCHDIR}/dfltcc_deflate.c)
        endif()
        if(WITH_DFLTCC_INFLATE)
            add_definitions(-DS390_DFLTCC_INFLATE)
            list(APPEND ZLIB_ARCH_SRCS ${ARCHDIR}/dfltcc_inflate.c)
        endif()
        if(WITH_CRC32_VX)
            check_vgfma_intrinsics()
            if(HAVE_VGFMA_INTRIN)
                add_definitions(-DS390_CRC32_VX)
                set(CRC32_VX_SRCS ${ARCHDIR}/crc32-vx.c)
                list(APPEND ZLIB_ARCH_SRCS ${CRC32_VX_SRCS})
                set_property(SOURCE ${CRC32_VX_SRCS} PROPERTY COMPILE_FLAGS "${VGFMAFLAG} ${NOLTOFLAG}")
            else()
                set(WITH_CRC32_VX OFF)
            endif()
        endif()
    elseif(BASEARCH_LOONGARCH_FOUND)
        add_definitions(-DLOONGARCH_FEATURES)
        list(APPEND ZLIB_ARCH_HDRS ${ARCHDIR}/loongarch_functions.h)
        if(WITH_RUNTIME_CPU_DETECTION)
            list(APPEND ZLIB_ARCH_HDRS ${ARCHDIR}/loongarch_features.h)
            list(APPEND ZLIB_ARCH_SRCS ${ARCHDIR}/loongarch_features.c)
        endif()

        if(WITH_CRC32_LA)
            check_la64_crc_intrinsics()
            if(HAVE_LA64_CRC_INTRIN)
                add_definitions(-DLOONGARCH_CRC)
                set(CRC32_LA_SRCS ${ARCHDIR}/crc32_la.c)
                list(APPEND ZLIB_ARCH_SRCS ${CRC32_LA_SRCS})
                set_property(SOURCE ${CRC32_LA_SRCS} PROPERTY COMPILE_FLAGS "${NOLTOFLAG}")
            else()
                set(WITH_CRC32_LA OFF)
            endif()
        endif()

        if(WITH_LSX)
            check_lsx_intrinsics()
            if(HAVE_LSX_INTRIN)
                add_definitions(-DLOONGARCH_LSX)
                set(LSX_SRCS ${ARCHDIR}/adler32_lsx.c ${ARCHDIR}/chunkset_lsx.c ${ARCHDIR}/compare256_lsx.c ${ARCHDIR}/slide_hash_lsx.c)
                list(APPEND ZLIB_ARCH_SRCS ${LSX_SRCS})
                set_property(SOURCE ${LSX_SRCS} PROPERTY COMPILE_FLAGS "${LSXFLAG} ${NOLTOFLAG}")
            else()
                set(HAVE_LSX_INTRIN OFF)
            endif()
        endif()

        if(WITH_LASX)
            check_lasx_intrinsics()
            if(HAVE_LASX_INTRIN AND HAVE_LSX_INTRIN)
                add_definitions(-DLOONGARCH_LASX)
                set(LASX_SRCS ${ARCHDIR}/adler32_lasx.c ${ARCHDIR}/chunkset_lasx.c ${ARCHDIR}/compare256_lasx.c ${ARCHDIR}/slide_hash_lasx.c)
                list(APPEND ZLIB_ARCH_SRCS ${LASX_SRCS})
                set_property(SOURCE ${LASX_SRCS} PROPERTY COMPILE_FLAGS "${LASXFLAG} ${NOLTOFLAG}")
            else()
                set(HAVE_LASX_INTRIN OFF)
            endif()
        endif()

    elseif(BASEARCH_X86_FOUND)
        add_definitions(-DX86_FEATURES)
        list(APPEND ZLIB_ARCH_HDRS ${ARCHDIR}/x86_functions.h)
        if(WITH_RUNTIME_CPU_DETECTION)
            list(APPEND ZLIB_ARCH_HDRS ${ARCHDIR}/x86_features.h)
            list(APPEND ZLIB_ARCH_SRCS ${ARCHDIR}/x86_features.c)
        endif()
        if(MSVC)
            list(APPEND ZLIB_ARCH_HDRS fallback_builtins.h)
        endif()
        check_xsave_intrinsics()
        if(HAVE_XSAVE_INTRIN)
            add_feature_info(XSAVE 1 "Support XSAVE intrinsics using \"${XSAVEFLAG}\"")
            if(WITH_RUNTIME_CPU_DETECTION)
                set_property(SOURCE ${ARCHDIR}/x86_features.c PROPERTY COMPILE_FLAGS "${XSAVEFLAG}")
            endif()
            if(NOT (CMAKE_C_COMPILER_ID MATCHES "GNU" AND CMAKE_C_COMPILER_VERSION VERSION_LESS 8.2))
                add_definitions(-DX86_HAVE_XSAVE_INTRIN)
            endif()
        endif()
        if(WITH_SSE2)
            check_sse2_intrinsics()
            if(HAVE_SSE2_INTRIN)
                add_definitions(-DX86_SSE2)
                set(SSE2_SRCS ${ARCHDIR}/chunkset_sse2.c ${ARCHDIR}/chorba_sse2.c ${ARCHDIR}/compare256_sse2.c ${ARCHDIR}/slide_hash_sse2.c)
                list(APPEND ZLIB_ARCH_SRCS ${SSE2_SRCS})
                if(ARCH_32BIT)
                    set_property(SOURCE ${SSE2_SRCS} PROPERTY COMPILE_FLAGS "${SSE2FLAG} ${NOLTOFLAG}")
                endif()
            else()
                set(WITH_SSE2 OFF)
            endif()
        endif()
        if(WITH_SSSE3)
            check_ssse3_intrinsics()
            if(HAVE_SSSE3_INTRIN AND WITH_SSE2)
                add_definitions(-DX86_SSSE3)
                set(SSSE3_SRCS ${ARCHDIR}/adler32_ssse3.c ${ARCHDIR}/chunkset_ssse3.c)
                add_feature_info(SSSE3_ADLER32 1 "Support SSSE3-accelerated adler32, using \"${SSSE3FLAG}\"")
                list(APPEND ZLIB_ARCH_SRCS ${SSSE3_SRCS})
                set_property(SOURCE ${SSSE3_SRCS} PROPERTY COMPILE_FLAGS "${SSSE3FLAG} ${NOLTOFLAG}")
            else()
                set(WITH_SSSE3 OFF)
            endif()
        endif()
        if(WITH_SSE41)
            check_sse41_intrinsics()
            if(HAVE_SSE41_INTRIN AND WITH_SSSE3)
                add_definitions(-DX86_SSE41)
                set(SSE41_SRCS ${ARCHDIR}/chorba_sse41.c)
                list(APPEND ZLIB_ARCH_SRCS ${SSE41_SRCS})
                set_property(SOURCE ${SSE41_SRCS} PROPERTY COMPILE_FLAGS "${SSE41FLAG} ${NOLTOFLAG}")
            else()
                set(WITH_SSE41 OFF)
            endif()
        endif()
        if(WITH_SSE42)
            check_sse42_intrinsics()
            if(HAVE_SSE42_INTRIN AND WITH_SSE41)
                add_definitions(-DX86_SSE42)
                set(SSE42_SRCS ${ARCHDIR}/adler32_sse42.c)
                add_feature_info(SSE42_CRC 1 "Support SSE4.2 optimized adler32 hash generation, using \"${SSE42FLAG}\"")
                list(APPEND ZLIB_ARCH_SRCS ${SSE42_SRCS})
                set_property(SOURCE ${SSE42_SRCS} PROPERTY COMPILE_FLAGS "${SSE42FLAG} ${NOLTOFLAG}")
            else()
                set(WITH_SSE42 OFF)
            endif()
        endif()
        if(WITH_PCLMULQDQ)
            check_pclmulqdq_intrinsics()
            if(HAVE_PCLMULQDQ_INTRIN AND WITH_SSE42)
                add_definitions(-DX86_PCLMULQDQ_CRC)
                set(PCLMULQDQ_SRCS ${ARCHDIR}/crc32_pclmulqdq.c)
                add_feature_info(PCLMUL_CRC 1 "Support CRC hash generation using PCLMULQDQ, using \"${SSE42FLAG} ${PCLMULFLAG}\"")
                list(APPEND ZLIB_ARCH_SRCS ${PCLMULQDQ_SRCS})
                set_property(SOURCE ${PCLMULQDQ_SRCS} PROPERTY COMPILE_FLAGS "${SSE42FLAG} ${PCLMULFLAG} ${NOLTOFLAG}")
            else()
                set(WITH_PCLMULQDQ OFF)
            endif()
        endif()
        if(WITH_AVX2)
            check_avx2_intrinsics()
            if(HAVE_AVX2_INTRIN AND WITH_SSE42)
                add_definitions(-DX86_AVX2)
                set(AVX2_SRCS ${ARCHDIR}/slide_hash_avx2.c)
                add_feature_info(AVX2_SLIDEHASH 1 "Support AVX2 optimized slide_hash, using \"${AVX2FLAG}\"")
                list(APPEND AVX2_SRCS ${ARCHDIR}/chunkset_avx2.c)
                add_feature_info(AVX2_CHUNKSET 1 "Support AVX2 optimized chunkset, using \"${AVX2FLAG}\"")
                list(APPEND AVX2_SRCS ${ARCHDIR}/compare256_avx2.c)
                add_feature_info(AVX2_COMPARE256 1 "Support AVX2 optimized compare256, using \"${AVX2FLAG}\"")
                list(APPEND AVX2_SRCS ${ARCHDIR}/adler32_avx2.c)
                add_feature_info(AVX2_ADLER32 1 "Support AVX2-accelerated adler32, using \"${AVX2FLAG}\"")
                list(APPEND ZLIB_ARCH_SRCS ${AVX2_SRCS})
                set_property(SOURCE ${AVX2_SRCS} PROPERTY COMPILE_FLAGS "${AVX2FLAG} ${NOLTOFLAG}")
            else()
                set(WITH_AVX2 OFF)
            endif()
        endif()
        if(WITH_AVX512)
            check_avx512_intrinsics()
            if(HAVE_AVX512_INTRIN AND WITH_AVX2)
                add_definitions(-DX86_AVX512)
                list(APPEND AVX512_SRCS ${ARCHDIR}/adler32_avx512.c)
                add_feature_info(AVX512_ADLER32 1 "Support AVX512-accelerated adler32, using \"${AVX512FLAG}\"")
                list(APPEND AVX512_SRCS ${ARCHDIR}/chunkset_avx512.c)
                add_feature_info(AVX512_CHUNKSET 1 "Support AVX512 optimized chunkset, using \"${AVX512FLAG}\"")
                list(APPEND AVX512_SRCS ${ARCHDIR}/compare256_avx512.c)
                add_feature_info(AVX512_COMPARE256 1 "Support AVX512 optimized compare256, using \"${AVX512FLAG}\"")
                list(APPEND ZLIB_ARCH_HDRS ${ARCHDIR}/adler32_avx512_p.h)
                list(APPEND ZLIB_ARCH_SRCS ${AVX512_SRCS})
                set_property(SOURCE ${AVX512_SRCS} PROPERTY COMPILE_FLAGS "${AVX512FLAG} ${NOLTOFLAG}")
            else()
                set(WITH_AVX512 OFF)
            endif()
        endif()
        if(WITH_AVX512VNNI)
            check_avx512vnni_intrinsics()
            if(HAVE_AVX512VNNI_INTRIN AND WITH_AVX2)
                add_definitions(-DX86_AVX512VNNI)
                add_feature_info(AVX512VNNI_ADLER32 1 "Support AVX512VNNI adler32, using \"${AVX512VNNIFLAG}\"")
                list(APPEND AVX512VNNI_SRCS ${ARCHDIR}/adler32_avx512_vnni.c)
                list(APPEND ZLIB_ARCH_SRCS ${AVX512VNNI_SRCS})
                set_property(SOURCE ${AVX512VNNI_SRCS} PROPERTY COMPILE_FLAGS "${AVX512VNNIFLAG} ${NOLTOFLAG}")
            else()
                set(WITH_AVX512VNNI OFF)
            endif()
        endif()
        if(WITH_VPCLMULQDQ)
            check_vpclmulqdq_intrinsics()
            if(HAVE_VPCLMULQDQ_INTRIN AND WITH_PCLMULQDQ AND WITH_AVX512)
                add_definitions(-DX86_VPCLMULQDQ_CRC)
                set(VPCLMULQDQ_SRCS ${ARCHDIR}/crc32_vpclmulqdq.c)
                add_feature_info(VPCLMUL_CRC 1 "Support CRC hash generation using VPCLMULQDQ, using \"${PCLMULFLAG} ${VPCLMULFLAG} ${AVX512FLAG}\"")
                list(APPEND ZLIB_ARCH_SRCS ${VPCLMULQDQ_SRCS})
                set_property(SOURCE ${VPCLMULQDQ_SRCS} PROPERTY COMPILE_FLAGS "${PCLMULFLAG} ${VPCLMULFLAG} ${AVX512FLAG} ${NOLTOFLAG}")
            else()
                set(WITH_VPCLMULQDQ OFF)
            endif()
        endif()
    endif()
else()
    # If WITH_OPTIM is disabled, we need all the fallbacks.
    set(WITH_ALL_FALLBACKS ON)
endif()

message(STATUS "Architecture-specific source files: ${ZLIB_ARCH_SRCS}")

#============================================================================
# zconf.h
#============================================================================

macro(generate_cmakein input output)
    file(REMOVE ${output})
    file(STRINGS ${input} _lines)
    foreach(_line IN LISTS _lines)
        string(REGEX REPLACE "#ifdef HAVE_UNISTD_H.*" "@ZCONF_UNISTD_LINE@" _line "${_line}")
        string(REGEX REPLACE "#ifdef NEED_PTRDIFF_T.*" "@ZCONF_PTRDIFF_LINE@" _line "${_line}")
        if(NEED_PTRDIFF_T)
            string(REGEX REPLACE "typedef PTRDIFF_TYPE" "typedef @PTRDIFF_TYPE@" _line "${_line}")
        endif()
        file(APPEND ${output} "${_line}\n")
    endforeach()
endmacro(generate_cmakein)

generate_cmakein( ${CMAKE_CURRENT_SOURCE_DIR}/zconf${SUFFIX}.h.in ${CMAKE_CURRENT_BINARY_DIR}/zconf${SUFFIX}.h.cmakein )

if(NOT CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_CURRENT_BINARY_DIR)
    # If we're doing an out of source build and the user has a zconf.h
    # in their source tree...
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/zconf${SUFFIX}.h)
        message(STATUS "Renaming")
        message(STATUS "    ${CMAKE_CURRENT_SOURCE_DIR}/zconf${SUFFIX}.h")
        message(STATUS "to 'zconf${SUFFIX}.h.included' because this file is included with zlib")
        message(STATUS "but CMake generates it automatically in the build directory.")
        file(RENAME ${CMAKE_CURRENT_SOURCE_DIR}/zconf${SUFFIX}.h ${CMAKE_CURRENT_SOURCE_DIR}/zconf${SUFFIX}.h.included)
    endif()

    # If we're doing an out of source build and the user has a zconf.h.cmakein
    # in their source tree...
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/zconf${SUFFIX}.h.cmakein)
        message(STATUS "Renaming")
        message(STATUS "    ${CMAKE_CURRENT_SOURCE_DIR}/zconf${SUFFIX}.h.cmakein")
        message(STATUS "to 'zconf${SUFFIX}.h.cmakeincluded' because this file is included with zlib")
        message(STATUS "but CMake generates it automatically in the build directory.")
        file(RENAME ${CMAKE_CURRENT_SOURCE_DIR}/zconf${SUFFIX}.h.cmakein ${CMAKE_CURRENT_SOURCE_DIR}/zconf${SUFFIX}.h.cmakeincluded)
    endif()
endif()

# The user is allowed (but discouraged) to set absolute CMAKE_INSTALL_*DIR paths.
# If they do, we copy these non-relocatable paths into the pkg-config file.
if(IS_ABSOLUTE "${CMAKE_INSTALL_INCLUDEDIR}")
    set(PC_INC_INSTALL_DIR "${CMAKE_INSTALL_INCLUDEDIR}")
else()
    set(PC_INC_INSTALL_DIR "\${prefix}/${CMAKE_INSTALL_INCLUDEDIR}")
endif()

if(IS_ABSOLUTE "${CMAKE_INSTALL_BINDIR}")
    set(PC_BIN_INSTALL_DIR "${CMAKE_INSTALL_BINDIR}")
else()
    set(PC_BIN_INSTALL_DIR "\${exec_prefix}/${CMAKE_INSTALL_BINDIR}")
endif()

if(IS_ABSOLUTE "${CMAKE_INSTALL_LIBDIR}")
    set(PC_LIB_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}")
else()
    set(PC_LIB_INSTALL_DIR "\${exec_prefix}/${CMAKE_INSTALL_LIBDIR}")
endif()

#============================================================================
# zlib
#============================================================================

set(ZLIB_PUBLIC_HDRS
    ${CMAKE_CURRENT_BINARY_DIR}/zconf${SUFFIX}.h
    ${CMAKE_CURRENT_BINARY_DIR}/zlib_name_mangling${SUFFIX}.h
    ${CMAKE_CURRENT_BINARY_DIR}/zlib${SUFFIX}.h
)
set(ZLIB_PRIVATE_HDRS
    arch/generic/chunk_permute_table.h
    arch/generic/compare256_p.h
    arch/generic/generic_functions.h
    adler32_p.h
    arch_functions.h
    chunkset_tpl.h
    compare256_rle.h
    crc32.h
    crc32_braid_p.h
    crc32_braid_comb_p.h
    crc32_braid_tbl.h
    deflate.h
    deflate_p.h
    fallback_builtins.h
    functable.h
    inffast_tpl.h
    inffixed_tbl.h
    inflate.h
    inflate_p.h
    inftrees.h
    insert_string_p.h
    insert_string_tpl.h
    match_tpl.h
    trees.h
    trees_emit.h
    trees_tbl.h
    zbuild.h
    zendian.h
    zmemory.h
    zutil.h
    zutil_p.h
)
set(ZLIB_SRCS
    adler32.c
    compress.c
    crc32.c
    crc32_braid_comb.c
    deflate.c
    deflate_fast.c
    deflate_huff.c
    deflate_medium.c
    deflate_quick.c
    deflate_rle.c
    deflate_slow.c
    deflate_stored.c
    functable.c
    infback.c
    inflate.c
    inftrees.c
    insert_string.c
    trees.c
    uncompr.c
    zutil.c
)

set(ZLIB_ALL_FALLBACK_SRCS
    arch/generic/adler32_c.c
    arch/generic/chunkset_c.c
    arch/generic/compare256_c.c
    arch/generic/crc32_braid_c.c
    arch/generic/slide_hash_c.c
)

if(WITH_ALL_FALLBACKS)
    list(APPEND ZLIB_GENERIC_SRCS ${ZLIB_ALL_FALLBACK_SRCS})
    add_definitions(-DWITH_ALL_FALLBACKS)
elseif(BASEARCH_X86_FOUND AND ARCH_64BIT AND WITH_SSE2)
    # x86_64 always has SSE2, so let the SSE2 functions act as fallbacks.
    list(APPEND ZLIB_GENERIC_SRCS
        arch/generic/adler32_c.c
        arch/generic/crc32_braid_c.c
    )

    # x86_64 does not need compare256 fallback if we have BUILTIN_CTZ
    if(NOT HAVE_BUILTIN_CTZ)
        list(APPEND ZLIB_GENERIC_SRCS arch/generic/compare256_c.c)
    endif()
else()
    list(APPEND ZLIB_GENERIC_SRCS ${ZLIB_ALL_FALLBACK_SRCS})
    add_definitions(-DWITH_ALL_FALLBACKS)
endif()

if(WITH_CRC32_CHORBA)
    list(APPEND ZLIB_SRCS arch/generic/crc32_chorba_c.c)
endif()

if(WITH_RUNTIME_CPU_DETECTION)
    list(APPEND ZLIB_PRIVATE_HDRS cpu_features.h)
    list(APPEND ZLIB_SRCS cpu_features.c)
endif()

set(ZLIB_GZFILE_PRIVATE_HDRS
    gzguts.h
)
set(ZLIB_GZFILE_SRCS
    gzlib.c
    gzread.c
    gzwrite.c
)

set(ZLIB_ALL_SRCS ${ZLIB_GENERIC_SRCS} ${ZLIB_SRCS} ${ZLIB_ARCH_HDRS} ${ZLIB_ARCH_SRCS} ${ZLIB_PUBLIC_HDRS} ${ZLIB_PRIVATE_HDRS})
if(WITH_GZFILEOP)
    list(APPEND ZLIB_ALL_SRCS ${ZLIB_GZFILE_PRIVATE_HDRS} ${ZLIB_GZFILE_SRCS})
endif()

if(NOT DEFINED BUILD_SHARED_LIBS OR BUILD_SHARED_LIBS)
    set(ZLIB_DLL_SRCS win32/zlib${SUFFIX}1.rc)
endif()

if(NOT DEFINED BUILD_SHARED_LIBS)
    add_library(zlib-ng SHARED ${ZLIB_ALL_SRCS} ${ZLIB_DLL_SRCS})
    add_library(zlib-ng-static STATIC ${ZLIB_ALL_SRCS})
    add_library(zlib-ng::zlib ALIAS zlib-ng)
    add_library(zlib-ng::zlibstatic ALIAS zlib-ng-static)

    set(ZLIB_INSTALL_LIBRARIES zlib-ng zlib-ng-static)
else()
    add_library(zlib-ng ${ZLIB_ALL_SRCS})
    add_library(zlib-ng::zlib ALIAS zlib-ng)

    if(BUILD_SHARED_LIBS)
        target_sources(zlib-ng PRIVATE ${ZLIB_DLL_SRCS})
    else()
        add_library(zlib-ng-static ALIAS zlib-ng)
        add_library(zlib-ng::zlibstatic ALIAS zlib-ng)
    endif()

    set(ZLIB_INSTALL_LIBRARIES zlib-ng)
endif()

# INFO: Mimics official zlib CMake target
# Generates ZLIB.cmake in case ZLIB_COMPAT=ON and always exports the CMake target ZLIB::ZLIB
# In case ZLIB_COMPAT=OFF, the CMake target and file follows zlib-ng naming convention
if (ZLIB_COMPAT)
    set_target_properties(zlib-ng PROPERTIES EXPORT_NAME ZLIB)
    if (NOT DEFINED BUILD_SHARED_LIBS)
        set_target_properties(zlib-ng-static PROPERTIES EXPORT_NAME ZLIBSTATIC)
    endif()
else()
    set_target_properties(zlib-ng PROPERTIES EXPORT_NAME zlib)
    if (NOT DEFINED BUILD_SHARED_LIBS)
        set_target_properties(zlib-ng-static PROPERTIES EXPORT_NAME zlibstatic)
    endif()
endif()

if (ZLIB_ALIASES)
    add_library(zlib ALIAS zlib-ng)
    if (NOT DEFINED BUILD_SHARED_LIBS)
        add_library(zlibstatic ALIAS zlib-ng-static)
    elseif (NOT BUILD_SHARED_LIBS)
        add_library(zlibstatic ALIAS zlib-ng)
    endif()
endif()

foreach(ZLIB_INSTALL_LIBRARY ${ZLIB_INSTALL_LIBRARIES})
    if(NOT ZLIB_COMPAT)
        target_compile_definitions(${ZLIB_INSTALL_LIBRARY} PUBLIC ZLIBNG_NATIVE_API)
    endif()
    target_include_directories(${ZLIB_INSTALL_LIBRARY} PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}$<SEMICOLON>${CMAKE_CURRENT_SOURCE_DIR}>"
        "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")
endforeach()

if(WIN32)
    # Shared library
    if(NOT DEFINED BUILD_SHARED_LIBS OR BUILD_SHARED_LIBS)
        set_target_properties(zlib-ng PROPERTIES OUTPUT_NAME zlib${SUFFIX})
    endif()
    # Static library
    if(NOT DEFINED BUILD_SHARED_LIBS)
        if(MSVC)
            set_target_properties(zlib-ng-static PROPERTIES OUTPUT_NAME zlibstatic${SUFFIX})
        else()
            set_target_properties(zlib-ng-static PROPERTIES OUTPUT_NAME z${SUFFIX})
        endif()
    elseif(NOT BUILD_SHARED_LIBS)
        if(MSVC)
            set_target_properties(zlib-ng PROPERTIES OUTPUT_NAME zlibstatic${SUFFIX})
        else()
            set_target_properties(zlib-ng PROPERTIES OUTPUT_NAME z${SUFFIX})
        endif()
    endif()
else()
    # On unix-like platforms the library is almost always called libz
    set_target_properties(${ZLIB_INSTALL_LIBRARIES} PROPERTIES OUTPUT_NAME z${SUFFIX})
endif()

if(NOT DEFINED BUILD_SHARED_LIBS OR BUILD_SHARED_LIBS)
    set_target_properties(zlib-ng PROPERTIES DEFINE_SYMBOL ZLIB_DLL)

    if(ZLIB_COMPAT)
        set_target_properties(zlib-ng PROPERTIES SOVERSION 1)
    else()
        set_target_properties(zlib-ng PROPERTIES SOVERSION 2)
    endif()

    if(NOT CYGWIN)
        # This property causes shared libraries on Linux to have the full version
        # encoded into their final filename.  We disable this on Cygwin because
        # it causes cygz-${ZLIB_FULL_VERSION}.dll to be created when cygz.dll
        # seems to be the default.
        #
        # This has no effect with MSVC, on that platform the version info for
        # the DLL comes from the resource file win32/zlib1.rc
        set_target_properties(zlib-ng PROPERTIES VERSION ${ZLIB_FULL_VERSION})
    endif()

    if(UNIX)
        if(HAVE_NO_INTERPOSITION)
            set_target_properties(zlib-ng PROPERTIES COMPILE_FLAGS "-fno-semantic-interposition")
        endif()
        if(NOT APPLE AND NOT CMAKE_SYSTEM_NAME STREQUAL AIX)
            if(NOT ZLIB_COMPAT)
                add_definitions(-DHAVE_SYMVER)
            endif()
            configure_file(${CMAKE_CURRENT_SOURCE_DIR}/zlib${SUFFIX}.map.in
                ${CMAKE_CURRENT_BINARY_DIR}/zlib${SUFFIX}.map @ONLY)
            set_target_properties(zlib-ng PROPERTIES LINK_FLAGS
                "-Wl,--version-script,\"${CMAKE_CURRENT_BINARY_DIR}/zlib${SUFFIX}.map\"")
        endif()
    endif()
    if(MSYS)
        # Suppress version number from shared library name
        set(CMAKE_SHARED_LIBRARY_NAME_WITH_VERSION 0)
    elseif(WIN32)
        # Creates zlib1.dll when building shared library version
        if(ZLIB_COMPAT)
            set_target_properties(zlib-ng PROPERTIES SUFFIX "1.dll")
        else()
            set_target_properties(zlib-ng PROPERTIES SUFFIX "2.dll")
        endif()
    endif()
endif()

if(HAVE_UNISTD_H)
  SET(ZCONF_UNISTD_LINE "#if 1    /* was set to #if 1 by configure/cmake/etc */")
else()
  SET(ZCONF_UNISTD_LINE "#if 0    /* was set to #if 0 by configure/cmake/etc */")
endif()
if(NEED_PTRDIFF_T)
    SET(ZCONF_PTRDIFF_LINE "#if 1    /* was set to #if 1 by configure/cmake/etc */")
else()
    SET(ZCONF_PTRDIFF_LINE "#ifdef NEED_PTRDIFF_T    /* may be set to #if 1 by configure/cmake/etc */")
endif()

set(ZLIB_PC ${CMAKE_CURRENT_BINARY_DIR}/zlib${SUFFIX}.pc)
if(WITH_GZFILEOP)
    set(PKG_CONFIG_CFLAGS "-DWITH_GZFILEOP")
endif()
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/zlib.pc.cmakein
    ${ZLIB_PC} @ONLY)
configure_file(${CMAKE_CURRENT_BINARY_DIR}/zconf${SUFFIX}.h.cmakein
    ${CMAKE_CURRENT_BINARY_DIR}/zconf${SUFFIX}.h @ONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/zlib${SUFFIX}.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/zlib${SUFFIX}.h @ONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/gzread_mangle.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/gzread_mangle.h @ONLY)

if (NOT ZLIB_SYMBOL_PREFIX STREQUAL "")
    add_feature_info(ZLIB_SYMBOL_PREFIX ON "Publicly exported symbols have a custom prefix")
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/zlib_name_mangling${SUFFIX}.h.in
        ${CMAKE_CURRENT_BINARY_DIR}/zlib_name_mangling${SUFFIX}.h @ONLY)
else()
    add_feature_info(ZLIB_SYMBOL_PREFIX OFF "Publicly exported symbols DO NOT have a custom prefix")
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/zlib_name_mangling.h.empty
        ${CMAKE_CURRENT_BINARY_DIR}/zlib_name_mangling${SUFFIX}.h COPYONLY)
endif()
# add_definitions(-DZLIB_SYMBOL_PREFIX=${ZLIB_SYMBOL_PREFIX}) # not needed


if(NOT SKIP_INSTALL_LIBRARIES AND NOT SKIP_INSTALL_ALL)
    install(TARGETS ${ZLIB_INSTALL_LIBRARIES}
        EXPORT ${EXPORT_NAME}-targets
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}")
endif()
if(NOT SKIP_INSTALL_HEADERS AND NOT SKIP_INSTALL_ALL)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/zlib${SUFFIX}.h
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}" RENAME zlib${SUFFIX}.h)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/zlib_name_mangling${SUFFIX}.h
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}" RENAME zlib_name_mangling${SUFFIX}.h)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/zconf${SUFFIX}.h
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}" RENAME zconf${SUFFIX}.h)
endif()
if(NOT SKIP_INSTALL_FILES AND NOT SKIP_INSTALL_ALL)
    install(FILES ${ZLIB_PC} DESTINATION "${PKGCONFIG_INSTALL_DIR}")
    install(EXPORT ${EXPORT_NAME}-targets
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${EXPORT_NAME}"
        NAMESPACE ${EXPORT_NAME}::)
    # Use GNU-style variable names
    set(INCLUDE_INSTALL_DIR ${CMAKE_INSTALL_INCLUDEDIR})
    set(LIB_INSTALL_DIR ${CMAKE_INSTALL_LIBDIR})
    if (ZLIB_COMPAT)
       set(PACKAGE_CONFIGNAME zlib)
       set(PACKAGE_VERSION ${ZLIB_HEADER_VERSION})
    else()
       set(PACKAGE_CONFIGNAME zlib-ng)
       set(PACKAGE_VERSION ${ZLIBNG_HEADER_VERSION})
    endif()
    configure_package_config_file(${PACKAGE_CONFIGNAME}-config.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/${PACKAGE_CONFIGNAME}-config.cmake
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${EXPORT_NAME}
        PATH_VARS INCLUDE_INSTALL_DIR LIB_INSTALL_DIR)
    write_basic_package_version_file(
        ${CMAKE_CURRENT_BINARY_DIR}/${PACKAGE_CONFIGNAME}-config-version.cmake
        VERSION ${PACKAGE_VERSION}
        COMPATIBILITY AnyNewerVersion)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${PACKAGE_CONFIGNAME}-config.cmake
                  ${CMAKE_CURRENT_BINARY_DIR}/${PACKAGE_CONFIGNAME}-config-version.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${EXPORT_NAME})
endif()

#============================================================================
# Example binaries
#============================================================================

if(BUILD_TESTING)
    enable_testing()

    if(BUILD_SHARED_LIBS)
        if(WITH_GTEST)
            message(STATUS "Disabling gtest because only shared libraries are enabled")
            set(WITH_GTEST OFF)
        endif()

        if(WITH_BENCHMARKS OR WITH_BENCHMARK_APPS)
            message(STATUS "Disabling google benchmarks because only shared libraries are enabled")
            set(WITH_BENCHMARKS OFF)
            set(WITH_BENCHMARK_APPS OFF)
        endif()
    endif()

    add_subdirectory(test)
endif()

#============================================================================
# Config summary
#============================================================================
message("")
message(STATUS "The following defines have been set (partial list):\n")
get_property(propvalraw DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY COMPILE_DEFINITIONS)
list(SORT propvalraw)
string(REPLACE ";" " " propval "${propvalraw}")
message("CURRENT_SOURCE_DIR: ${propval}\n")

add_feature_info(WITH_GZFILEOP WITH_GZFILEOP "Compile with support for gzFile related functions")
add_feature_info(ZLIB_COMPAT ZLIB_COMPAT "Compile with zlib compatible API")
add_feature_info(ZLIB_ALIASES ZLIB_ALIASES "Compile with zlib compatible CMake targets")
add_feature_info(BUILD_TESTING BUILD_TESTING "Build test binaries")
add_feature_info(WITH_SANITIZER WITH_SANITIZER "Enable sanitizer testing support")
add_feature_info(WITH_GTEST WITH_GTEST "Build tests using Gtest framework")
add_feature_info(WITH_FUZZERS WITH_FUZZERS "Build test/fuzz")
add_feature_info(WITH_BENCHMARKS WITH_BENCHMARKS "Build benchmarks using Google Benchmark framework")
add_feature_info(WITH_BENCHMARK_APPS WITH_BENCHMARK_APPS "Build application benchmarks (currently libpng)")
add_feature_info(WITH_ALL_FALLBACKS WITH_ALL_FALLBACKS "Build all generic fallback functions")
add_feature_info(WITH_OPTIM WITH_OPTIM "Build with optimisation")
add_feature_info(WITH_NEW_STRATEGIES WITH_NEW_STRATEGIES "Use new strategies")
add_feature_info(WITH_CRC32_CHORBA WITH_CRC32_CHORBA "Use optimized CRC32 algorithm Chorba")
add_feature_info(WITH_NATIVE_INSTRUCTIONS WITH_NATIVE_INSTRUCTIONS
    "Instruct the compiler to use the full instruction set on this host (gcc/clang -march=native)")
add_feature_info(WITH_RUNTIME_CPU_DETECTION WITH_RUNTIME_CPU_DETECTION "Build with runtime CPU detection")
add_feature_info(WITH_MAINTAINER_WARNINGS WITH_MAINTAINER_WARNINGS "Build with project maintainer warnings")
add_feature_info(WITH_CODE_COVERAGE WITH_CODE_COVERAGE "Enable code coverage reporting")
add_feature_info(WITH_INFLATE_STRICT WITH_INFLATE_STRICT "Build with strict inflate distance checking")
add_feature_info(WITH_INFLATE_ALLOW_INVALID_DIST WITH_INFLATE_ALLOW_INVALID_DIST "Build with zero fill for inflate invalid distances")

if(BASEARCH_ARM_FOUND)
    add_feature_info(WITH_ARMV6 WITH_ARMV6 "Build with ARMv6 SIMD")
    add_feature_info(WITH_ARMV8 WITH_ARMV8 "Build with ARMv8 intrinsics")
    add_feature_info(WITH_NEON WITH_NEON "Build with NEON intrinsics")
elseif(BASEARCH_PPC_FOUND)
    add_feature_info(WITH_ALTIVEC WITH_ALTIVEC "Build with AltiVec optimisations")
    add_feature_info(WITH_POWER8 WITH_POWER8 "Build with optimisations for POWER8")
    add_feature_info(WITH_POWER9 WITH_POWER9 "Build with optimisations for POWER9")
elseif(BASEARCH_RISCV_FOUND)
    add_feature_info(WITH_RVV WITH_RVV "Build with RVV intrinsics")
    add_feature_info(WITH_RISCV_ZBC WITH_RISCV_ZBC "Build with RISCV ZBC")
elseif(BASEARCH_S360_FOUND)
    add_feature_info(WITH_CRC32_VX WITH_CRC32_VX "Build with vectorized CRC32 on IBM Z")
    add_feature_info(WITH_DFLTCC_DEFLATE WITH_DFLTCC_DEFLATE "Build with DFLTCC intrinsics for compression on IBM Z")
    add_feature_info(WITH_DFLTCC_INFLATE WITH_DFLTCC_INFLATE "Build with DFLTCC intrinsics for decompression on IBM Z")
elseif(BASEARCH_LOONGARCH_FOUND)
    add_feature_info(WITH_CRC32_LA WITH_CRC32_LA "Build with vectorized CRC32 on LoongArch")
    add_feature_info(WITH_LSX WITH_LSX "Build with vectorized LSX on LoongArch")
    add_feature_info(WITH_LASX WITH_LASX "Build with vectorized LASX on LoongArch")
elseif(BASEARCH_X86_FOUND)
    add_feature_info(WITH_SSE2 WITH_SSE2 "Build with SSE2")
    add_feature_info(WITH_SSSE3 WITH_SSSE3 "Build with SSSE3")
    add_feature_info(WITH_SSE41 WITH_SSE41 "Build with SSE41")
    add_feature_info(WITH_SSE42 WITH_SSE42 "Build with SSE42")
    add_feature_info(WITH_PCLMULQDQ WITH_PCLMULQDQ "Build with PCLMULQDQ")
    add_feature_info(WITH_AVX2 WITH_AVX2 "Build with AVX2")
    add_feature_info(WITH_AVX512 WITH_AVX512 "Build with AVX512")
    add_feature_info(WITH_AVX512VNNI WITH_AVX512VNNI "Build with AVX512 VNNI")
    add_feature_info(WITH_VPCLMULQDQ WITH_VPCLMULQDQ "Build with VPCLMULQDQ")
endif()

add_feature_info(INSTALL_UTILS INSTALL_UTILS "Copy minigzip and minideflate during install")

feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES)

#============================================================================
# CPack
#============================================================================
set(CPACK_GENERATOR "TGZ")
set(CPACK_SOURCE_GENERATOR "TGZ")
set(CPACK_SOURCE_IGNORE_FILES .git/ _CPack_Packages/ "${PROJECT_BINARY_DIR}/")

set(CPACK_PACKAGE_NAME "zlib${SUFFIX}")
set(CPACK_PACKAGE_VERSION ${ZLIB_FULL_VERSION})
set(CPACK_PACKAGE_DIRECTORY "${PROJECT_BINARY_DIR}/package")

if("${PROJECT_BINARY_DIR}" STREQUAL "${PROJECT_SOURCE_DIR}")
    message(WARNING "Building to source folder is not recommended. Cpack will be unable to generate source package.")
endif()

include(CPack)
