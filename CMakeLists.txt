cmake_minimum_required(VERSION 3.5.1)
if(CMAKE_VERSION VERSION_LESS 3.12)
  cmake_policy(VERSION ${CMAKE_VERSION})
else()
  cmake_policy(VERSION 3.5.1...3.13.2)
endif()
message(STATUS "Using CMake version ${CMAKE_VERSION}")

set(CMAKE_MACOSX_RPATH 1)

# If not specified on the command line, enable C99 as the default
# Configuration items that affect the global compiler envirionment standards
# should be issued before the "project" command.
if(NOT CMAKE_C_STANDARD)
  set (CMAKE_C_STANDARD 99)          # The C standard whose features are requested to build this target
endif()
if(NOT CMAKE_C_STANDARD_REQUIRED)
  set (CMAKE_C_STANDARD_REQUIRED ON) # Boolean describing whether the value of C_STANDARD is a requirement
endif()
if(NOT CMAKE_C_EXTENSIONS)
  set (CMAKE_C_EXTENSIONS OFF)       # Boolean specifying whether compiler specific extensions are requested
endif()
set(VALID_C_STANDARDS "99" "11")
if(NOT CMAKE_C_STANDARD IN_LIST VALID_C_STANDARDS)
   MESSAGE(FATAL_ERROR "CMAKE_C_STANDARD:STRING=${CMAKE_C_STANDARD} not in know standards list\n ${VALID_C_STANDARDS}")
endif()

# Parse the full version number from zlib.h and include in ZLIB_FULL_VERSION
file(READ ${CMAKE_CURRENT_SOURCE_DIR}/zlib${SUFFIX}.h _zlib_h_contents)
string(REGEX REPLACE ".*#define[ \t]+ZLIB_VERSION[ \t]+\"([0-9]+.[0-9]+.[0-9]+).*\".*"
        "\\1" ZLIB_HEADER_VERSION ${_zlib_h_contents})
string(REGEX REPLACE ".*#define[ \t]+ZLIBNG_VERSION[ \t]+\"([-0-9A-Za-z.]+)\".*"
        "\\1" ZLIBNG_HEADER_VERSION ${_zlib_h_contents})
message(STATUS "ZLIB_HEADER_VERSION: ${ZLIB_HEADER_VERSION}")
message(STATUS "ZLIBNG_HEADER_VERSION: ${ZLIBNG_HEADER_VERSION}")

project(zlib
  VERSION ${ZLIB_HEADER_VERSION}
  LANGUAGES C)

set(INSTALL_BIN_DIR "${CMAKE_INSTALL_PREFIX}/bin" CACHE PATH "Installation directory for executables")
set(INSTALL_LIB_DIR "${CMAKE_INSTALL_PREFIX}/lib" CACHE PATH "Installation directory for libraries")
set(INSTALL_INC_DIR "${CMAKE_INSTALL_PREFIX}/include" CACHE PATH "Installation directory for headers")
set(INSTALL_MAN_DIR "${CMAKE_INSTALL_PREFIX}/share/man" CACHE PATH "Installation directory for manual pages")
set(INSTALL_PKGCONFIG_DIR "${CMAKE_INSTALL_PREFIX}/share/pkgconfig" CACHE PATH "Installation directory for pkgconfig (.pc) files")

include(CheckTypeSize)
include(CheckSymbolExists)
include(CheckFunctionExists)
include(CheckIncludeFile)
include(CheckCSourceCompiles)
include(CheckCSourceRuns)
include(CMakeDependentOption)
include(FeatureSummary)

include(cmake/detect-arch.cmake)
include(cmake/detect-sanitizer.cmake)

if(CMAKE_TOOLCHAIN_FILE)
    message(STATUS "Using CMake toolchain: ${CMAKE_TOOLCHAIN_FILE}")
endif()

# Make sure we use an appropriate BUILD_TYPE by default, "Release" to be exact
# this should select the maximum generic optimisation on the current platform (i.e. -O3 for gcc/clang)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING
        "Choose the type of build, standard options are: Debug Release RelWithDebInfo MinSizeRel."
        FORCE)
    add_feature_info(CMAKE_BUILD_TYPE 1 "Build type: ${CMAKE_BUILD_TYPE} (default)")
else()
    add_feature_info(CMAKE_BUILD_TYPE 1 "Build type: ${CMAKE_BUILD_TYPE} (selected)")
endif()

check_include_file(sys/types.h HAVE_SYS_TYPES_H)
check_include_file(stdint.h    HAVE_STDINT_H)
check_include_file(stddef.h    HAVE_STDDEF_H)
check_include_file(sys/sdt.h   HAVE_SYS_SDT_H)

#
# Options parsing
#
option(WITH_GZFILEOP "Compile with support for gzFile related functions" OFF)
option(ZLIB_COMPAT "Compile with zlib compatible API" OFF)
option(ZLIB_ENABLE_TESTS "Build test binaries" ON)
option(ZLIB_DUAL_LINK "Dual link tests against system zlib" OFF)
option(WITH_SANITIZERS "Build with address sanitizer and all supported sanitizers other than memory sanitizer" OFF)
option(WITH_MSAN "Build with memory sanitizer" OFF)
option(WITH_FUZZERS "Build test/fuzz" OFF)
option(WITH_OPTIM "Build with optimisation" ON)
option(WITH_NEW_STRATEGIES "Use new strategies" ON)
option(WITH_NATIVE_INSTRUCTIONS
    "Instruct the compiler to use the full instruction set on this host (gcc/clang -march=native)" OFF)
option(WITH_MAINTAINER_WARNINGS "Build with project maintainer warnings" OFF)
option(WITH_CODE_COVERAGE "Enable code coverage reporting" OFF)
option(WITH_INFLATE_STRICT "Build with strict inflate distance checking" OFF)
option(WITH_INFLATE_ALLOW_INVALID_DIST "Build with zero fill for inflate invalid distances" OFF)

if(BASEARCH_ARM_FOUND)
    option(WITH_ACLE "Build with ACLE" ON)
    option(WITH_NEON "Build with NEON intrinsics" ON)
elseif(BASEARCH_PPC_FOUND)
    option(WITH_POWER8 "Build with optimisations for POWER8" ON)
elseif(BASEARCH_S360_FOUND AND "${ARCH}" MATCHES "s390x")
    option(WITH_DFLTCC_DEFLATE "Use DEFLATE CONVERSION CALL instruction for compression on IBM Z" OFF)
    option(WITH_DFLTCC_INFLATE "Use DEFLATE CONVERSION CALL instruction for decompression on IBM Z" OFF)
elseif(BASEARCH_X86_FOUND)
    option(WITH_AVX2 "Build with AVX2" ON)
    option(WITH_SSE2 "Build with SSE2" ON)
    option(WITH_SSE4 "Build with SSE4" ON)
    option(WITH_PCLMULQDQ "Build with PCLMULQDQ" ON)
endif()

mark_as_advanced(FORCE ZLIB_DUAL_LINK WITH_ACLE WITH_NEON WITH_DFLTCC_DEFLATE WITH_DFLTCC_INFLATE
    WITH_AVX2 WITH_SSE2 WITH_SSE4 WITH_PCLMULQDQ WITH_POWER8 WITH_INFLATE_STRICT WITH_INFLATE_ALLOW_INVALID_DIST)

add_feature_info(ZLIB_COMPAT ZLIB_COMPAT "Provide a zlib-compatible API")
add_feature_info(WITH_GZFILEOP WITH_GZFILEOP "Compile with support for gzFile-related functions")
add_feature_info(WITH_OPTIM WITH_OPTIM "Build with optimisation")
add_feature_info(WITH_SANITIZERS WITH_SANITIZERS "Build with address sanitizer and all supported sanitizers other than memory sanitizer")
add_feature_info(WITH_MSAN WITH_MSAN "Build with memory sanitizer")
add_feature_info(WITH_FUZZERS WITH_FUZZERS "Build test/fuzz")
add_feature_info(WITH_NEW_STRATEGIES WITH_NEW_STRATEGIES "Use new strategies")
if(BASEARCH_ARM_FOUND)
    add_feature_info(WITH_ACLE WITH_ACLE "Build with ACLE CRC")
    add_feature_info(WITH_NEON WITH_NEON "Build with NEON intrinsics")
elseif(BASEARCH_PPC_FOUND)
    add_feature_info(WITH_POWER8 WITH_POWER8 "Build with optimisations for POWER8")
endif()
add_feature_info(WITH_MAINTAINER_WARNINGS WITH_MAINTAINER_WARNINGS "Build with project maintainer warnings")
add_feature_info(WITH_CODE_COVERAGE WITH_CODE_COVERAGE "Enable code coverage reporting")

if (ZLIB_COMPAT)
    add_definitions(-DZLIB_COMPAT)
    set(WITH_GZFILEOP ON)
    set(SUFFIX "")
else()
    set(SUFFIX "-ng")
endif()

if(WITH_GZFILEOP)
    add_definitions(-DWITH_GZFILEOP)
endif()

if(${CMAKE_C_COMPILER} MATCHES "icc" OR ${CMAKE_C_COMPILER} MATCHES "icpc" OR ${CMAKE_C_COMPILER} MATCHES "icl")
    if(WITH_NATIVE_INSTRUCTIONS)
        message(STATUS "Ignoring WITH_NATIVE_INSTRUCTIONS; not supported on this configuration")
    endif()
    if(CMAKE_HOST_UNIX OR APPLE)
        set(WARNFLAGS "-W3")
        set(WARNFLAGS_MAINTAINER "-W4 -Wcheck")
        set(WARNFLAGS_DISABLE "")
        if(BASEARCH_X86_FOUND)
            set(AVX2FLAG "-mavx2")
            set(SSE2FLAG "-msse2")
            set(SSE4FLAG "-msse4.2")
        endif()
    else()
        set(WARNFLAGS "/W3")
        set(WARNFLAGS_MAINTAINER "/W4 /Wcheck")
        set(WARNFLAGS_DISABLE "")
        if(BASEARCH_X86_FOUND)
            set(AVX2FLAG "/arch:AVX2")
            set(SSE2FLAG "/arch:SSE2")
            set(SSE4FLAG "/arch:SSE4.2")
        endif()
    endif()
elseif(MSVC)
    # TODO. ICC can be used through MSVC. I'm not sure if we'd ever see that combination
    # (who'd use cmake from an IDE...) but checking for ICC before checking for MSVC should
    # avoid mistakes.
    # /Oi ?
    set(WARNFLAGS "/W3")
    set(WARNFLAGS_MAINTAINER "/W4")
    set(WARNFLAGS_DISABLE "")
    if(BASEARCH_ARM_FOUND)
        add_definitions(-D_ARM_WINAPI_PARTITION_DESKTOP_SDK_AVAILABLE)
        set(NEONFLAG "/arch:VFPv4")
    elseif(BASEARCH_X86_FOUND)
        if(NOT ${ARCH} MATCHES "x86_64")
            set(SSE2FLAG "/arch:SSE2")
        endif()
    endif()
    if(WITH_NATIVE_INSTRUCTIONS)
        message(STATUS "Ignoring WITH_NATIVE_INSTRUCTIONS; not supported on this configuration")
    endif()
else()
    # catch all GNU C compilers as well as Clang and AppleClang
    if(CMAKE_C_COMPILER_ID MATCHES "GNU" OR CMAKE_C_COMPILER_ID MATCHES "Clang")
        set(__GNUC__ ON)
    endif()
    # Enable warnings in GCC and Clang
    if(__GNUC__)
        set(WARNFLAGS "-Wall")
        set(WARNFLAGS_MAINTAINER "-Wextra -Wpedantic")
        set(WARNFLAGS_DISABLE "-Wno-implicit-fallthrough")
    endif()
    if(WITH_NATIVE_INSTRUCTIONS)
        if(__GNUC__)
            if(BASEARCH_PPC_FOUND)
                set(NATIVEFLAG "-mcpu=native")
            else()
                set(NATIVEFLAG "-march=native")
            endif()
        else()
            message(STATUS "Ignoring WITH_NATIVE_INSTRUCTIONS; not implemented yet on this configuration")
        endif()
    endif()
    if(NOT NATIVEFLAG)
        if (__GNUC__)
            if(BASEARCH_ARM_FOUND)
                # Check support for ARM floating point
                execute_process(COMMAND ${CMAKE_C_COMPILER} "-dumpmachine"
                                OUTPUT_VARIABLE GCC_MACHINE)
                if ("${GCC_MACHINE}" MATCHES "eabihf")
                    set(FLOATABI "-mfloat-abi=hard")
                else()
                    set(FLOATABI "-mfloat-abi=softfp")
                endif()
                # NEON
                if("${ARCH}" MATCHES "aarch64")
                    set(NEONFLAG "-march=armv8-a+crc+simd")
                else()
                    # Check whether -mfpu=neon is available
                    set(CMAKE_REQUIRED_FLAGS "-mfpu=neon")
                    check_c_source_compiles(
                        "int main() { return 0; }"
                        MFPU_NEON_AVAILABLE FAIL_REGEX "not supported")
                    set(CMAKE_REQUIRED_FLAGS)
                    if(MFPU_NEON_AVAILABLE)
                        set(NEONFLAG "${FLOATABI} -mfpu=neon")
                    else()
                        set(NEONFLAG "${FLOATABI}")
                    endif()
                endif()
                # ACLE
                set(ACLEFLAG "-march=armv8-a+crc")
            elseif(BASEARCH_PPC_FOUND)
                set(POWER8FLAG "-mcpu=power8")
            elseif(BASEARCH_X86_FOUND)
                set(AVX2FLAG "-mavx2")
                set(SSE2FLAG "-msse2")
                set(SSE4FLAG "-msse4")
                set(PCLMULFLAG "-mpclmul")
            endif()
        endif()
    else()
        if(BASEARCH_ARM_FOUND)
            set(ACLEFLAG "${NATIVEFLAG}")
            if("${ARCH}" MATCHES "aarch64")
                set(NEONFLAG "${NATIVEFLAG}")
            endif()
        elseif(BASEARCH_X86_FOUND)
            set(AVX2FLAG ${NATIVEFLAG})
            set(SSE2FLAG ${NATIVEFLAG})
            set(SSE4FLAG ${NATIVEFLAG})
            set(PCLMULFLAG ${NATIVEFLAG})
        endif()
    endif()
endif()

# Set architecture alignment requirements
if(BASEARCH_ARM_FOUND OR (BASEARCH_PPC_FOUND AND "${ARCH}" MATCHES "powerpc64le") OR BASEARCH_X86_FOUND)
    if(NOT DEFINED UNALIGNED_OK)
        set(UNALIGNED_OK TRUE)
    endif()
endif()
if(UNALIGNED_OK)
    add_definitions(-DUNALIGNED_OK)
    message(STATUS "Architecture supports unaligned reads")
endif()

# Apply warning flags to cflags
if(WITH_MAINTAINER_WARNINGS)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${WARNFLAGS} ${WARNFLAGS_MAINTAINER} ${WARNFLAGS_DISABLE}")
else()
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${WARNFLAGS} ${WARNFLAGS_DISABLE}")
endif()
if(WITH_CODE_COVERAGE)
    if(CMAKE_C_COMPILER_ID MATCHES "Clang")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -coverage")
    elseif(__GNUC__)
        # Some versions of GCC don't support -coverage shorthand
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fprofile-arcs -ftest-coverage")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -lgcov")
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -lgcov")
    endif()
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
check_function_exists(fseeko HAVE_FSEEKO)
if(NOT HAVE_FSEEKO)
    add_definitions(-DNO_FSEEKO)
endif()
check_function_exists(strerror HAVE_STRERROR)
if(NOT HAVE_STRERROR)
    add_definitions(-DNO_STRERROR)
endif()

#
# Check for unistd.h and stdarg.h
#
check_include_file(unistd.h Z_HAVE_UNISTD_H)

if(WITH_MSAN AND WITH_SANITIZERS)
    message(FATAL_ERROR "Memory sanitizer is incompatible with address sanitizer")
elseif(WITH_MSAN)
    add_memory_sanitizer_check()
elseif(WITH_SANITIZERS)
    add_sanitizer_checks()
endif()
#
# Check if we can hide zlib internal symbols that are linked between separate source files using hidden
#
check_c_source_compiles(
    "#define ZLIB_INTERNAL __attribute__((visibility (\"hidden\")))
    int ZLIB_INTERNAL foo;
    int main()
    {
      return 0;
    }"
    HAVE_ATTRIBUTE_VISIBILITY_HIDDEN FAIL_REGEX "not supported")
if(HAVE_ATTRIBUTE_VISIBILITY_HIDDEN)
    add_definitions(-DHAVE_VISIBILITY_HIDDEN)
endif()

#
# Check if we can hide zlib internal symbols that are linked between separate source files using internal
#
check_c_source_compiles(
    "#define ZLIB_INTERNAL __attribute__((visibility (\"internal\")))
    int ZLIB_INTERNAL foo;
    int main()
    {
      return 0;
    }"
    HAVE_ATTRIBUTE_VISIBILITY_INTERNAL FAIL_REGEX "not supported")
if(HAVE_ATTRIBUTE_VISIBILITY_INTERNAL)
    add_definitions(-DHAVE_VISIBILITY_INTERNAL)
endif()

#
# check for __builtin_ctz() support in the compiler
#
check_c_source_compiles(
    "int main(void)
    {
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
#
# check for __builtin_ctzll() support in the compiler
#
check_c_source_compiles(
    "int main(void)
    {
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

#
# check for ptrdiff_t support
#
check_c_source_compiles(
    "#include <stddef.h>
     int main() { ptrdiff_t *a; return 0; }"
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

# Macro to check if source compiles when cross-compiling
# or runs when compiling natively
macro(check_c_source_compile_or_run source flag)
    if(CMAKE_CROSSCOMPILING)
        check_c_source_compiles("${source}" ${flag})
    else()
        check_c_source_runs("${source}" ${flag})
    endif()
endmacro()

set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -DZLIB_DEBUG")

if(MSVC)
    set(CMAKE_DEBUG_POSTFIX "d")
    add_definitions(-D_CRT_SECURE_NO_DEPRECATE)
    add_definitions(-D_CRT_NONSTDC_NO_DEPRECATE)
endif()

if(BASEARCH_PPC_FOUND)
    # Check if we have what we need for POWER8 optimizations
    set(CMAKE_REQUIRED_FLAGS "${POWER8FLAG}")
    check_c_source_compiles(
        "#include <sys/auxv.h>
        int main() {
          return (getauxval(AT_HWCAP2) & PPC_FEATURE2_ARCH_2_07);
        }"
        HAVE_POWER8
    )
    set(CMAKE_REQUIRED_FLAGS)
elseif(BASEARCH_X86_FOUND)
    # Check whether compiler supports SSE2 instrinics
    if(WITH_NATIVE_INSTRUCTIONS)
        set(CMAKE_REQUIRED_FLAGS "${NATIVEFLAG}")
    else()
        set(CMAKE_REQUIRED_FLAGS "${SSE2FLAG}")
    endif()
    check_c_source_compile_or_run(
        "#include <immintrin.h>
        int main(void)
        {
            __m128i zero = _mm_setzero_si128();
            (void)zero;
            return 0;
        }"
        HAVE_SSE2_INTRIN
    )
    set(CMAKE_REQUIRED_FLAGS)

    # Check whether compiler supports SSE4 CRC inline asm
    if(WITH_NATIVE_INSTRUCTIONS)
        set(CMAKE_REQUIRED_FLAGS "${NATIVEFLAG}")
    else()
        set(CMAKE_REQUIRED_FLAGS "${SSE4FLAG}")
    endif()
    check_c_source_compile_or_run(
        "int main(void)
        {
            unsigned val = 0, h = 0;
        #if defined(_MSC_VER)
            { __asm mov edx, h __asm mov eax, val __asm crc32 eax, edx __asm mov val, eax }
        #else
            __asm__ __volatile__ ( \"crc32 %1,%0\" : \"+r\" (h) : \"r\" (val) );
        #endif
            return (int) h;
        }"
        HAVE_SSE42CRC_INLINE_ASM
    )
    # Check whether compiler supports SSE4 CRC instrinics
    check_c_source_compile_or_run(
        "#include <immintrin.h>
        int main(void)
        {
            unsigned crc = 0;
            char c = 'c';
        #if defined(_MSC_VER)
            crc = _mm_crc32_u32(crc, c);
        #else
            crc = __builtin_ia32_crc32qi(crc, c);
        #endif
            (void)crc;
            return 0;
        }"
        HAVE_SSE42CRC_INTRIN
    )
    # Check whether compiler supports SSE4.2 compare string instrinics
    check_c_source_compile_or_run(
        "#include <immintrin.h>
        int main(void)
        {
            unsigned char a[64] = { 0 };
            unsigned char b[64] = { 0 };
            __m128i xmm_src0, xmm_src1;
            xmm_src0 = _mm_loadu_si128((__m128i *)(char *)a);
            xmm_src1 = _mm_loadu_si128((__m128i *)(char *)b);
            return _mm_cmpestri(xmm_src0, 16, xmm_src1, 16, 0);
        }"
        HAVE_SSE42CMPSTR_INTRIN
    )
    set(CMAKE_REQUIRED_FLAGS)

    # Check whether compiler supports PCLMULQDQ intrinics
    if(WITH_NATIVE_INSTRUCTIONS)
        set(CMAKE_REQUIRED_FLAGS "${NATIVEFLAG}")
    else()
        set(CMAKE_REQUIRED_FLAGS "${PCLMULFLAG}")
    endif()
    if(NOT (APPLE AND ${ARCH} MATCHES "i386"))
        # The pclmul code currently crashes on Mac in 32bit mode. Avoid for now.
        check_c_source_compile_or_run(
            "#include <immintrin.h>
            int main(void)
            {
                __m128i a = _mm_setzero_si128();
                __m128i b = _mm_setzero_si128();
                __m128i c = _mm_clmulepi64_si128(a, b, 0x10);
                (void)c;
                return 0;
            }"
            HAVE_PCLMULQDQ_INTRIN
        )
    else()
        set(HAVE_PCLMULQDQ_INTRIN NO)
    endif()
    set(CMAKE_REQUIRED_FLAGS)

    # Check whether compiler supports AVX2 intrinics
    if(WITH_NATIVE_INSTRUCTIONS)
        set(CMAKE_REQUIRED_FLAGS "${NATIVEFLAG}")
    else()
        set(CMAKE_REQUIRED_FLAGS "${AVX2FLAG}")
    endif()
    check_c_source_compile_or_run(
        "#include <immintrin.h>
        int main(void) {
            __m256i x = _mm256_set1_epi16(2);
            const __m256i y = _mm256_set1_epi16(1);
            x = _mm256_subs_epu16(x, y);
            (void)x;
            return 0;
        }"
        HAVE_AVX2_INTRIN
    )
    set(CMAKE_REQUIRED_FLAGS)

    # FORCE_SSE2 option will only be shown if HAVE_SSE2_INTRIN is true
    if("${ARCH}" MATCHES "i[3-6]86")
        cmake_dependent_option(FORCE_SSE2 "Always assume CPU is SSE2 capable" OFF "HAVE_SSE2_INTRIN" OFF)
    endif()
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
# Macro to add either the given intrinsics option to the global compiler options,
# or ${NATIVEFLAG} (-march=native) if that is appropriate and possible.
# An alternative version of this macro would take a file argument, and set ${flag}
# only for that file as opposed to ${NATIVEFLAG} globally, to limit side-effect of
# using ${flag} globally.
#
macro(add_intrinsics_option flag)
    if(WITH_NATIVE_INSTRUCTIONS AND NATIVEFLAG)
        if (NOT "${CMAKE_C_FLAGS} " MATCHES ".*${NATIVEFLAG} .*")
            set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${NATIVEFLAG}")
        endif()
    else()
        if (NOT "${CMAKE_C_FLAGS} " MATCHES ".*${flag} .*")
            set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${flag}")
        endif()
    endif()
endmacro()

set(ZLIB_ARCH_SRCS)
set(ZLIB_ARCH_HDRS)
set(ARCHDIR "arch/generic")
if(BASEARCH_ARM_FOUND)
    set(ARCHDIR "arch/arm")
elseif(BASEARCH_PPC_FOUND)
    set(ARCHDIR "arch/power")
elseif(BASEARCH_S360_FOUND AND "${ARCH}" MATCHES "s390x")
    set(ARCHDIR "arch/s390")
elseif(BASEARCH_X86_FOUND)
    set(ARCHDIR "arch/x86")
    if(NOT ${ARCH} MATCHES "x86_64")
        add_feature_info(SSE2 1 "Support the SSE2 instruction set, using \"${SSE2FLAG}\"")
    endif()
else()
    message(STATUS "No optimized architecture: using ${ARCHDIR}")
endif()

if(WITH_OPTIM)
    if(BASEARCH_ARM_FOUND)
        add_definitions(-DARM_CPUID)
        list(APPEND ZLIB_ARCH_HDRS ${ARCHDIR}/arm.h)
        list(APPEND ZLIB_ARCH_SRCS ${ARCHDIR}/armfeature.c)
        if(WITH_NEON)
            list(APPEND ZLIB_ARCH_HDRS ${ARCHDIR}/adler32_neon.h)
            list(APPEND ZLIB_ARCH_SRCS ${ARCHDIR}/adler32_neon.c ${ARCHDIR}/slide_neon.c)
            add_definitions(-DARM_NEON_ADLER32 -DARM_NEON_SLIDEHASH)
            add_intrinsics_option("${NEONFLAG}")
            if(MSVC)
                add_definitions(-D__ARM_NEON__)
            endif()
            add_feature_info(NEON_ALDER32 1 "Support NEON instructions in adler32, using \"${NEONFLAG}\"")
            add_feature_info(NEON_SLIDEHASH 1 "Support NEON instructions in slide_hash, using \"${NEONFLAG}\"")
        endif()
        if(WITH_ACLE)
            list(APPEND ZLIB_ARCH_SRCS ${ARCHDIR}/crc32_acle.c ${ARCHDIR}/insert_string_acle.c)
            add_definitions(-DARM_ACLE_CRC_HASH)
            # For ARM aarch64, we need to check WITH_NEON first
            if("${ARCH}" MATCHES "arm" OR NOT WITH_NEON)
                add_intrinsics_option("${ACLEFLAG}")
            endif()
            add_feature_info(ACLE_CRC 1 "Support ACLE optimized CRC hash generation, using \"${ACLEFLAG}\"")
        endif()
    elseif(BASEARCH_PPC_FOUND)
        if(WITH_POWER8 AND HAVE_POWER8)
            add_definitions(-DPOWER_FEATURES)
            add_definitions(-DPOWER8)
            set(ZLIB_POWER8_SRCS
                ${ARCHDIR}/slide_hash_power8.c)
            set_source_files_properties(
                ${ZLIB_POWER8_SRCS}
                PROPERTIES COMPILE_FLAGS ${POWER8FLAG})
            list(APPEND ZLIB_ARCH_HDRS ${ARCHDIR}/power.h)
            list(APPEND ZLIB_ARCH_SRCS ${ARCHDIR}/power.c ${ZLIB_POWER8_SRCS})
        endif()
    elseif(BASEARCH_S360_FOUND AND "${ARCH}" MATCHES "s390x")
        if(WITH_DFLTCC_DEFLATE OR WITH_DFLTCC_INFLATE)
            list(APPEND ZLIB_ARCH_SRCS ${ARCHDIR}/dfltcc_common.c)
            add_definitions(-DGZBUFSIZE=262144)
        endif()
        if(WITH_DFLTCC_DEFLATE)
            add_definitions(-DS390_DFLTCC_DEFLATE)
            list(APPEND ZLIB_ARCH_SRCS ${ARCHDIR}/dfltcc_deflate.c)
        endif()
        if(WITH_DFLTCC_INFLATE)
            add_definitions(-DS390_DFLTCC_INFLATE)
            list(APPEND ZLIB_ARCH_SRCS ${ARCHDIR}/dfltcc_inflate.c)
        endif()
    elseif(BASEARCH_X86_FOUND)
        add_definitions(-DX86_CPUID)
        list(APPEND ZLIB_ARCH_HDRS ${ARCHDIR}/x86.h)
        list(APPEND ZLIB_ARCH_SRCS ${ARCHDIR}/x86.c)
        if(MSVC)
            list(APPEND ZLIB_ARCH_HDRS fallback_builtins.h)
        endif()
        if(WITH_AVX2 AND HAVE_AVX2_INTRIN)
            add_definitions(-DX86_AVX2)
            list(APPEND ZLIB_ARCH_SRCS ${ARCHDIR}/slide_avx.c)
            add_feature_info(AVX2_SLIDEHASH 1 "Support AVX2 optimized slide_hash, using \"${AVX2FLAG}\"")
            list(APPEND ZLIB_ARCH_SRCS ${ARCHDIR}/compare258_avx.c)
            add_feature_info(AVX2_COMPARE258 1 "Support AVX2 optimized compare258, using \"${AVX2FLAG}\"")
            add_intrinsics_option("${AVX2FLAG}")
        endif()
        if(WITH_SSE4 AND (HAVE_SSE42CRC_INLINE_ASM OR HAVE_SSE42CRC_INTRIN))
            add_definitions(-DX86_SSE42_CRC_HASH)
            list(APPEND ZLIB_ARCH_SRCS ${ARCHDIR}/insert_string_sse.c)
            add_feature_info(SSE42_CRC 1 "Support SSE4.2 optimized CRC hash generation, using \"${SSE4FLAG}\"")
            add_intrinsics_option("${SSE4FLAG}")
            if(HAVE_SSE42CRC_INTRIN)
                add_definitions(-DX86_SSE42_CRC_INTRIN)
            endif()
            if(WITH_NEW_STRATEGIES)
                add_definitions(-DX86_QUICK_STRATEGY)
                list(APPEND ZLIB_ARCH_SRCS ${ARCHDIR}/deflate_quick.c)
                add_feature_info(SSE42_DEFLATE_QUICK 1 "Support SSE4.2 accelerated quick compression")
            endif()
        endif()
        if(HAVE_SSE42CMPSTR_INTRIN)
            add_definitions(-DX86_SSE42_CMP_STR)
            list(APPEND ZLIB_ARCH_SRCS ${ARCHDIR}/compare258_sse.c)
            add_feature_info(SSE42_COMPARE258 1 "Support SSE4.2 optimized compare258, using \"${SSE4FLAG}\"")
        endif()
        if(WITH_SSE2 AND HAVE_SSE2_INTRIN)
            add_definitions(-DX86_SSE2)
            list(APPEND ZLIB_ARCH_SRCS ${ARCHDIR}/slide_sse.c)
            if(NOT ${ARCH} MATCHES "x86_64")
                add_intrinsics_option("${SSE2FLAG}")
                add_feature_info(FORCE_SSE2 FORCE_SSE2 "Assume CPU is SSE2 capable")
                if(FORCE_SSE2)
                    add_definitions(-DX86_NOCHECK_SSE2)
                endif()
            endif()
        endif()
        if(WITH_PCLMULQDQ AND HAVE_PCLMULQDQ_INTRIN)
            add_definitions(-DX86_PCLMULQDQ_CRC)
            list(APPEND ZLIB_ARCH_SRCS ${ARCHDIR}/crc_folding.c)
            add_intrinsics_option("${PCLMULFLAG}")
            if(HAVE_SSE42CRC_INLINE_ASM)
                add_feature_info(PCLMUL_CRC 1 "Support CRC hash generation using PCLMULQDQ, using \"${PCLMULFLAG}\"")
            else()
                add_feature_info(PCLMUL_CRC 1 "Support CRC hash generation using PCLMULQDQ, using \"${PCLMULFLAG} ${SSE4FLAG}\"")
            endif()
        endif()
    endif()
endif()
message(STATUS "Architecture-specific source files: ${ZLIB_ARCH_SRCS}")

#============================================================================
# zconf.h
#============================================================================

macro(generate_cmakein input output)
    file(REMOVE ${output})
    file(STRINGS ${input} _lines)
    foreach(_line IN LISTS _lines)
        file(APPEND ${output} "${_line}\n")

        if (_line STREQUAL "#define ZCONF_H" OR _line STREQUAL "#define ZCONFNG_H")
            file(APPEND ${output} "#cmakedefine Z_HAVE_UNISTD_H\n")
            if(NOT HAVE_PTRDIFF_T)
              file(APPEND ${output} "#cmakedefine NEED_PTRDIFF_T\n")
              file(APPEND ${output} "#cmakedefine PTRDIFF_TYPE ${PTRDIFF_TYPE}\n")
            endif()
        endif()
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

set(ZLIB_PC ${CMAKE_CURRENT_BINARY_DIR}/zlib${SUFFIX}.pc)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/zlib.pc.cmakein
    ${ZLIB_PC} @ONLY)
configure_file(${CMAKE_CURRENT_BINARY_DIR}/zconf${SUFFIX}.h.cmakein
    ${CMAKE_CURRENT_BINARY_DIR}/zconf${SUFFIX}.h @ONLY)

#============================================================================
# zlib
#============================================================================

set(ZLIB_PUBLIC_HDRS
    ${CMAKE_CURRENT_BINARY_DIR}/zconf${SUFFIX}.h
    zlib${SUFFIX}.h
)
set(ZLIB_PRIVATE_HDRS
    adler32_p.h
    crc32.h
    crc32_p.h
    deflate.h
    deflate_p.h
    functable.h
    gzguts.h
    inffast.h
    inffixed.h
    inflate.h
    inflate_p.h
    inftrees.h
    insert_string_tpl.h
    match_tpl.h
    memcopy.h
    trees.h
    trees_emit.h
    trees_p.h
    zbuild.h
    zendian.h
    zutil.h
)
set(ZLIB_SRCS
    adler32.c
    compare258.c
    compress.c
    crc32.c
    deflate.c
    deflate_fast.c
    deflate_medium.c
    deflate_slow.c
    functable.c
    inflate.c
    infback.c
    inftrees.c
    inffast.c
    insert_string.c
    trees.c
    uncompr.c
    zutil.c
)

set(ZLIB_GZFILE_SRCS
    gzclose.c
    gzlib.c
    gzread.c
    gzwrite.c
)

if(NOT MINGW AND NOT MSYS)
    set(ZLIB_DLL_SRCS
        win32/zlib${SUFFIX}1.rc # If present will override custom build rule below.
    )
endif()

if(MINGW OR MSYS)
    # This gets us DLL resource information when compiling on MinGW.
    if(NOT CMAKE_RC_COMPILER)
        set(CMAKE_RC_COMPILER windres.exe)
    endif()

    add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/zlib1rc.obj
        COMMAND ${CMAKE_RC_COMPILER}
            -D GCC_WINDRES
            -I ${CMAKE_CURRENT_SOURCE_DIR}
            -I ${CMAKE_CURRENT_BINARY_DIR}
            -o ${CMAKE_CURRENT_BINARY_DIR}/zlib1rc.obj
            -i ${CMAKE_CURRENT_SOURCE_DIR}/win32/zlib${SUFFIX}1.rc)
    set(ZLIB_DLL_SRCS ${CMAKE_CURRENT_BINARY_DIR}/zlib1rc.obj)
endif()

set(ZLIB_ALL_SRCS ${ZLIB_SRCS} ${ZLIB_ARCH_HDRS} ${ZLIB_ARCH_SRCS} ${ZLIB_DLL_SRCS}
    ${ZLIB_PUBLIC_HDRS} ${ZLIB_PRIVATE_HDRS})
if(WITH_GZFILEOP)
    list(APPEND ZLIB_ALL_SRCS ${ZLIB_GZFILE_SRCS})
endif()

if(NOT DEFINED BUILD_SHARED_LIBS)
    add_library(zlib SHARED ${ZLIB_ALL_SRCS})
    add_library(zlibstatic STATIC ${ZLIB_ALL_SRCS})

    set(ZLIB_INSTALL_LIBRARIES zlib zlibstatic)
else()
    add_library(zlib ${ZLIB_ALL_SRCS})

    set(ZLIB_INSTALL_LIBRARIES zlib)
endif()

foreach(ZLIB_INSTALL_LIBRARY ${ZLIB_INSTALL_LIBRARIES})
    target_include_directories(${ZLIB_INSTALL_LIBRARY} PUBLIC
        ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR})
endforeach()

if(WIN32)
    set_target_properties(${ZLIB_INSTALL_LIBRARIES} PROPERTIES OUTPUT_NAME zlib${SUFFIX})
else()
    # On unix-like platforms the library is almost always called libz
    set_target_properties(${ZLIB_INSTALL_LIBRARIES} PROPERTIES OUTPUT_NAME z${SUFFIX})
endif()

if(NOT DEFINED BUILD_SHARED_LIBS OR BUILD_SHARED_LIBS)
    set_target_properties(zlib PROPERTIES DEFINE_SYMBOL ZLIB_DLL)
    set_target_properties(zlib PROPERTIES SOVERSION 1)

    if (ZLIB_COMPAT)
        set(ZLIB_FULL_VERSION ${ZLIB_HEADER_VERSION})
    else()
        set(ZLIB_FULL_VERSION ${ZLIBNG_HEADER_VERSION})
    endif()

    if(NOT CYGWIN)
        # This property causes shared libraries on Linux to have the full version
        # encoded into their final filename.  We disable this on Cygwin because
        # it causes cygz-${ZLIB_FULL_VERSION}.dll to be created when cygz.dll
        # seems to be the default.
        #
        # This has no effect with MSVC, on that platform the version info for
        # the DLL comes from the resource file win32/zlib1.rc
        set_target_properties(zlib PROPERTIES VERSION ${ZLIB_FULL_VERSION})
    endif()

    if(UNIX)
        if(NOT APPLE)
            set_target_properties(zlib PROPERTIES LINK_FLAGS
                "-Wl,--version-script,\"${CMAKE_CURRENT_SOURCE_DIR}/zlib${SUFFIX}.map\"")
        endif()
    elseif(MSYS)
        # Suppress version number from shared library name
        set(CMAKE_SHARED_LIBRARY_NAME_WITH_VERSION 0)
    elseif(WIN32)
        # Creates zlib1.dll when building shared library version
        set_target_properties(zlib PROPERTIES SUFFIX "1.dll")
    endif()
endif()

if(NOT SKIP_INSTALL_LIBRARIES AND NOT SKIP_INSTALL_ALL)
    install(TARGETS ${ZLIB_INSTALL_LIBRARIES}
        RUNTIME DESTINATION "${INSTALL_BIN_DIR}"
        ARCHIVE DESTINATION "${INSTALL_LIB_DIR}"
        LIBRARY DESTINATION "${INSTALL_LIB_DIR}")
endif()
if(NOT SKIP_INSTALL_HEADERS AND NOT SKIP_INSTALL_ALL)
    install(FILES zlib${SUFFIX}.h
        DESTINATION "${INSTALL_INC_DIR}" RENAME zlib${SUFFIX}.h)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/zconf${SUFFIX}.h
        DESTINATION "${INSTALL_INC_DIR}" RENAME zconf${SUFFIX}.h)
endif()
if(NOT SKIP_INSTALL_FILES AND NOT SKIP_INSTALL_ALL)
    install(FILES zlib.3 DESTINATION "${INSTALL_MAN_DIR}/man3" RENAME zlib${SUFFIX}.3)
endif()
if(NOT SKIP_INSTALL_FILES AND NOT SKIP_INSTALL_ALL)
    install(FILES ${ZLIB_PC} DESTINATION "${INSTALL_PKGCONFIG_DIR}")
endif()

#============================================================================
# Example binaries
#============================================================================

option(ZLIB_ENABLE_TESTS "Build test binaries" ON)
if (ZLIB_ENABLE_TESTS)
    enable_testing()
    macro(configure_test_executable target)
        target_include_directories(${target} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
        if(NOT WITH_GZFILEOP)
            target_compile_definitions(${target} PUBLIC -DWITH_GZFILEOP)
            target_sources(${target} PRIVATE ${ZLIB_GZFILE_SRCS})
        endif()
        if(ZLIB_DUAL_LINK)
            find_package(ZLIB)
            if(ZLIB_FOUND)
                target_link_libraries(minigzip ${ZLIB_LIBRARIES})
            endif()
        endif()
    endmacro()

    add_executable(example test/example.c)
    configure_test_executable(example)
    target_link_libraries(example zlib)

    set(EXAMPLE_COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} $<TARGET_FILE:example>)
    add_test(NAME example COMMAND ${EXAMPLE_COMMAND})

    add_executable(minigzip test/minigzip.c)
    configure_test_executable(minigzip)
    if(NOT DEFINED BUILD_SHARED_LIBS)
        target_link_libraries(minigzip zlibstatic)
    else()
        target_link_libraries(minigzip zlib)
    endif()
    if(BASEARCH_S360_FOUND AND "${ARCH}" MATCHES "s390x")
        if(WITH_DFLTCC_DEFLATE OR WITH_DFLTCC_INFLATE)
            set_source_files_properties(test/minigzip.c PROPERTIES COMPILE_DEFINITIONS BUFLEN=262144)
        endif()
    endif()

    add_executable(switchlevels test/switchlevels.c)
    configure_test_executable(switchlevels)
    target_link_libraries(switchlevels zlib)

    add_executable(infcover test/infcover.c inftrees.c)
    configure_test_executable(infcover)
    target_link_libraries(infcover zlib)

    add_executable(makefixed tools/makefixed.c inftrees.c)
    target_include_directories(makefixed PUBLIC ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR})

    add_executable(maketrees tools/maketrees.c trees.c zutil.c)
    target_include_directories(maketrees PUBLIC ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR})

    add_executable(makecrct tools/makecrct.c)
    target_include_directories(makecrct PUBLIC ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR})

    if(HAVE_OFF64_T)
        add_executable(example64 test/example.c)
        configure_test_executable(example64)
        target_link_libraries(example64 zlib)
        set_target_properties(example64 PROPERTIES COMPILE_FLAGS "-D_FILE_OFFSET_BITS=64")
        set(EXAMPLE64_COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} $<TARGET_FILE:example64>)
        add_test(NAME example64 COMMAND ${EXAMPLE64_COMMAND})

        add_executable(minigzip64 test/minigzip.c)
        configure_test_executable(minigzip64)
        if(NOT DEFINED BUILD_SHARED_LIBS)
            target_link_libraries(minigzip64 zlibstatic)
        else()
            target_link_libraries(minigzip64 zlib)
        endif()
        set_target_properties(minigzip64 PROPERTIES COMPILE_FLAGS "-D_FILE_OFFSET_BITS=64")
    endif()

    if(WITH_FUZZERS)
        set(FUZZERS checksum compress example_small example_large example_flush example_dict minigzip)
        file(GLOB ALL_SRC_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*")
        foreach(FUZZER ${FUZZERS})
            add_executable(${FUZZER}_fuzzer test/fuzz/${FUZZER}_fuzzer.c test/fuzz/standalone_fuzz_target_runner.c)
            configure_test_executable(${FUZZER}_fuzzer)
            target_link_libraries(${FUZZER}_fuzzer zlib)
            set(FUZZER_COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} $<TARGET_FILE:${FUZZER}_fuzzer> ${ALL_SRC_FILES})
            add_test(NAME ${FUZZER}_fuzzer COMMAND ${FUZZER_COMMAND})
        endforeach()
    endif()

    set(CVES CVE-2002-0059 CVE-2004-0797 CVE-2005-1849 CVE-2005-2096)
    foreach(CVE ${CVES})
        set(CVE_COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} $<TARGET_FILE:minigzip> -d)
        add_test(NAME ${CVE}
            COMMAND ${CMAKE_COMMAND}
            "-DCOMMAND=${CVE_COMMAND}"
            -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/test/${CVE}/test.gz
            "-DSUCCESS_EXIT=0;1"
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/run-and-redirect.cmake)
    endforeach()

    macro(minigzip_deflate_file target name path)
        set(GZ_COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} $<TARGET_FILE:${target}>
            ${CMAKE_CURRENT_SOURCE_DIR}/${path})
        add_test(NAME ${target}-${name}-file-compr
            COMMAND ${CMAKE_COMMAND}
            "-DCOMMAND=${GZ_COMMAND}"
            "-DSUCCESS_EXIT=0;1"
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/run-and-redirect.cmake)
    endmacro()
    macro(minigzip_inflate_file target name path)
        set(GZ_COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} $<TARGET_FILE:${target}>
            -d ${CMAKE_CURRENT_SOURCE_DIR}/${path})
        add_test(NAME ${target}-${name}-file-uncompr
            COMMAND ${CMAKE_COMMAND}
            "-DCOMMAND=${GZ_COMMAND}"
            "-DSUCCESS_EXIT=0;1"
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/run-and-redirect.cmake)
    endmacro()

    minigzip_deflate_file(minigzip "lcet10" "test/data/lcet10.txt")
    minigzip_inflate_file(minigzip "lcet10" "test/data/lcet10.txt.gz")

    macro(minigzip_stdio_cmp target name path)
        set(GZ_COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} $<TARGET_FILE:${target}> -c)
        foreach(EXTRA_ARG IN ITEMS "${ARGN}")
            list(APPEND GZ_COMMAND ${EXTRA_ARG})
        endforeach()
        string(REPLACE ";" "" GZ_ARGS "${ARGN}")
        string(REPLACE "-" "" GZ_ARGS "${GZ_ARGS}")
        # Test minigzip can decompress minigzip compressed
        add_test(NAME ${target}-${name}-${GZ_ARGS}-compr
            COMMAND ${CMAKE_COMMAND}
            "-DCOMMAND=${GZ_COMMAND}"
            -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/${path}
            -DOUTPUT=${CMAKE_CURRENT_SOURCE_DIR}/${path}.gz
            "-DSUCCESS_EXIT=0;1"
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/run-and-redirect.cmake)
        set(GZ_COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} $<TARGET_FILE:${target}> -d)
        add_test(NAME ${target}-${name}-${GZ_ARGS}-uncompr
            COMMAND ${CMAKE_COMMAND}
            "-DCOMMAND=${GZ_COMMAND}"
            -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/${path}.gz
            -DOUTPUT=${CMAKE_CURRENT_SOURCE_DIR}/${path}.out
            "-DSUCCESS_EXIT=0;1"
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/run-and-redirect.cmake)
        add_test(NAME ${target}-${name}-${GZ_ARGS}-cmp
        COMMAND ${CMAKE_COMMAND} -E compare_files
            ${CMAKE_CURRENT_SOURCE_DIR}/${path}
            ${CMAKE_CURRENT_SOURCE_DIR}/${path}.out)
        if(NOT "${ARGN}" MATCHES "-T")
            # Transparent writing does not use gzip format
            find_program(GZIP gzip)
            if(GZIP)
                # Test gzip can decompress minigzip compressed
                set(GZ_COMMAND ${GZIP} --decompress)
                add_test(NAME ${target}-${name}-${GZ_ARGS}-gzip-uncompr
                    COMMAND ${CMAKE_COMMAND}
                    "-DCOMMAND=${GZ_COMMAND}"
                    -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/${path}.gz
                    -DOUTPUT=${CMAKE_CURRENT_SOURCE_DIR}/${path}.gzip.out
                    "-DSUCCESS_EXIT=0;1"
                    -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/run-and-redirect.cmake)
                add_test(NAME ${target}-${name}-${GZ_ARGS}-gzip-uncompr-cmp
                    COMMAND ${CMAKE_COMMAND} -E compare_files
                        ${CMAKE_CURRENT_SOURCE_DIR}/${path}
                        ${CMAKE_CURRENT_SOURCE_DIR}/${path}.gzip.out)
                # Test minigzip can decompress gzip compressed
                set(GZ_COMMAND ${GZIP} --stdout)
                add_test(NAME ${target}-${name}-${GZ_ARGS}-gzip-compr
                    COMMAND ${CMAKE_COMMAND}
                    "-DCOMMAND=${GZ_COMMAND}"
                    -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/${path}
                    -DOUTPUT=${CMAKE_CURRENT_SOURCE_DIR}/${path}.gzip.gz
                    "-DSUCCESS_EXIT=0;1"
                    -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/run-and-redirect.cmake)
                set(GZ_COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} $<TARGET_FILE:${target}> -d)
                add_test(NAME ${target}-${name}-${GZ_ARGS}-minigzip-uncompr
                    COMMAND ${CMAKE_COMMAND}
                    "-DCOMMAND=${GZ_COMMAND}"
                    -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/${path}.gzip.gz
                    -DOUTPUT=${CMAKE_CURRENT_SOURCE_DIR}/${path}.gzip.out
                    "-DSUCCESS_EXIT=0;1"
                    -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/run-and-redirect.cmake)
                add_test(NAME ${target}-${name}-${GZ_ARGS}-minigzip-cmp
                    COMMAND ${CMAKE_COMMAND} -E compare_files
                        ${CMAKE_CURRENT_SOURCE_DIR}/${path}
                        ${CMAKE_CURRENT_SOURCE_DIR}/${path}.gzip.out)
            endif()
        endif()
    endmacro()

    set(OPEN_MODES -f -h -R -F -T)
    set(COMPRESSION_LEVELS -0 -1 -6 -9)
    foreach(OPEN_MODE ${OPEN_MODES})
        foreach(COMPRESSION_LEVEL ${COMPRESSION_LEVELS})
            minigzip_stdio_cmp(minigzip "lcet10" "test/data/lcet10.txt" ${OPEN_MODE} ${COMPRESSION_LEVEL})
            minigzip_stdio_cmp(minigzip "fireworks" "test/data/fireworks.jpg" ${OPEN_MODE} ${COMPRESSION_LEVEL})
            minigzip_stdio_cmp(minigzip "paper-100k" "test/data/paper-100k.pdf" ${OPEN_MODE} ${COMPRESSION_LEVEL})
        endforeach()
    endforeach()

    if(HAVE_OFF64_T)
        minigzip_stdio_cmp(minigzip64 "offset64" "test/data/paper-100k.pdf")
    endif()

    minigzip_stdio_cmp(minigzip "detect-text" "test/data/lcet10.txt" -A)
    minigzip_stdio_cmp(minigzip "detect-binary" "test/data/paper-100k.pdf" -A)

    if(NOT WIN32 AND ZLIB_COMPAT)
        add_executable(CVE-2003-0107 test/CVE-2003-0107.c)
        target_link_libraries(CVE-2003-0107 zlib)
        set(CVE20030107_COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} $<TARGET_FILE:CVE-2003-0107>)
        add_test(NAME CVE-2003-0107 COMMAND ${CVE20030107_COMMAND})
    endif()

    set(MAKEFIXED_COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} $<TARGET_FILE:makefixed>)
    add_test(NAME makefixed
        COMMAND ${CMAKE_COMMAND}
        "-DCOMMAND=${MAKEFIXED_COMMAND}"
        -DOUTPUT=${CMAKE_CURRENT_SOURCE_DIR}/inffixed._h
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/run-and-redirect.cmake)

    add_test(NAME makefixed-cmp
        COMMAND ${CMAKE_COMMAND} -E compare_files
            ${CMAKE_CURRENT_SOURCE_DIR}/inffixed.h
            ${CMAKE_CURRENT_SOURCE_DIR}/inffixed._h)

    set(MAKETREES_COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} $<TARGET_FILE:maketrees>)
    add_test(NAME maketrees
        COMMAND ${CMAKE_COMMAND}
        "-DCOMMAND=${MAKETREES_COMMAND}"
        -DOUTPUT=${CMAKE_CURRENT_SOURCE_DIR}/trees._h
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/run-and-redirect.cmake)

    add_test(NAME maketrees-cmp
        COMMAND ${CMAKE_COMMAND} -E compare_files
            ${CMAKE_CURRENT_SOURCE_DIR}/trees.h
            ${CMAKE_CURRENT_SOURCE_DIR}/trees._h)

    set(MAKECRCT_COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} $<TARGET_FILE:makecrct>)
    add_test(NAME makecrct
        COMMAND ${CMAKE_COMMAND}
        "-DCOMMAND=${MAKECRCT_COMMAND}"
        -DOUTPUT=${CMAKE_CURRENT_SOURCE_DIR}/crc32._h
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/run-and-redirect.cmake)

    add_test(NAME makecrct-cmp
        COMMAND ${CMAKE_COMMAND} -E compare_files
            ${CMAKE_CURRENT_SOURCE_DIR}/crc32.h
            ${CMAKE_CURRENT_SOURCE_DIR}/crc32._h)

    set(INFCOVER_COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} $<TARGET_FILE:infcover>)
    add_test(NAME infcover COMMAND ${INFCOVER_COMMAND})

    set(GH_361_COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} $<TARGET_FILE:minigzip> -4)
    add_test(NAME GH-361
        COMMAND ${CMAKE_COMMAND}
        "-DCOMMAND=${GH_361_COMMAND}"
        -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/test/GH-361/test.txt
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/run-and-redirect.cmake)

    set(GH_364_COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} $<TARGET_FILE:switchlevels> 1 5 9 3)
    add_test(NAME GH-364
            COMMAND ${CMAKE_COMMAND}
            "-DCOMMAND=${GH_364_COMMAND}"
            -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/test/GH-364/test.bin
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/run-and-redirect.cmake)

    set(GH_536_COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} $<TARGET_FILE:switchlevels> 6 9744 1 91207)
    add_test(NAME GH-536-segfault
            COMMAND ${CMAKE_COMMAND}
            "-DCOMMAND=${GH_536_COMMAND}"
            -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/test/data/lcet10.txt
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/run-and-redirect.cmake)

    set(GH_536_COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} $<TARGET_FILE:switchlevels> 6 88933 1 195840 2 45761)
    add_test(NAME GH-536-incomplete-read
            COMMAND ${CMAKE_COMMAND}
            "-DCOMMAND=${GH_536_COMMAND}"
            -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/test/data/lcet10.txt
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/run-and-redirect.cmake)

    set(GH_536_COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} $<TARGET_FILE:switchlevels> 6 15248 1 1050 2 25217)
    add_test(NAME GH-536-deflate-zero-stored-block
            COMMAND ${CMAKE_COMMAND}
            "-DCOMMAND=${GH_536_COMMAND}"
            -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/test/data/lcet10.txt
            -DOUTPUT=${CMAKE_CURRENT_SOURCE_DIR}/test/data/lcet10-zl.txt.gz
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/run-and-redirect.cmake)

    set(GH_536_COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} $<TARGET_FILE:minigzip> -d)
    add_test(NAME GH-536-inflate-zero-stored-block
        COMMAND ${CMAKE_COMMAND}
        "-DCOMMAND=${GH_536_COMMAND}"
        -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/test/data/lcet10-zl.txt.gz
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/run-and-redirect.cmake)

endif()

FEATURE_SUMMARY(WHAT ALL INCLUDE_QUIET_PACKAGES)
