cmake_minimum_required(VERSION 3.5.1)
if(CMAKE_VERSION VERSION_LESS 3.12)
  cmake_policy(VERSION ${CMAKE_VERSION})
else()
  cmake_policy(VERSION 3.5.1...3.13.2)
endif()

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
if(NOT CMAKE_C_STANDARD IN_LIST VALID_C_STANDARDS )
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

if(CMAKE_OSX_ARCHITECTURES)
    # If multiple architectures are requested (universal build), pick only the first
    list(GET CMAKE_OSX_ARCHITECTURES 0 ARCH)
else()
    set(ARCH ${CMAKE_SYSTEM_PROCESSOR})
endif()
message(STATUS "Architecture: ${ARCH}")
if(CMAKE_TOOLCHAIN_FILE)
    message(STATUS "Using cmake toolchain: ${CMAKE_TOOLCHAIN_FILE}")
endif()

#
# Options parsing
#
option(WITH_GZFILEOP "Compile with support for gzFile related functions" OFF)
option(ZLIB_COMPAT "Compile with zlib compatible API" OFF)
option(ZLIB_ENABLE_TESTS "Build test binaries" ON)
option(WITH_SANITIZERS "Build with address sanitizer and all supported sanitizers other than memory sanitizer" OFF)
option(WITH_MSAN "Build with memory sanitizer" OFF)
option(WITH_FUZZERS "Build test/fuzz" OFF)
option(WITH_OPTIM "Build with optimisation" ON)
option(WITH_NEW_STRATEGIES "Use new strategies" ON)
option(WITH_NATIVE_INSTRUCTIONS
    "Instruct the compiler to use the full instruction set on this host (gcc/clang -march=native)" OFF)
if("${ARCH}" MATCHES "arm" OR "${ARCH}" MATCHES "aarch64")
    option(WITH_ACLE "Build with ACLE CRC" ON)
    option(WITH_NEON "Build with NEON intrinsics" ON)
elseif("${ARCH}" MATCHES "s390x")
    option(WITH_DFLTCC_DEFLATE "Use DEFLATE CONVERSION CALL instruction for compression on IBM Z" OFF)
    option(WITH_DFLTCC_INFLATE "Use DEFLATE CONVERSION CALL instruction for decompression on IBM Z" OFF)
endif()

add_feature_info(ZLIB_COMPAT ZLIB_COMPAT "Provide a zlib-compatible API")
add_feature_info(WITH_GZFILEOP WITH_GZFILEOP "Compile with support for gzFile-related functions")
add_feature_info(WITH_OPTIM WITH_OPTIM "Build with optimisation")
add_feature_info(WITH_SANITIZERS WITH_SANITIZERS "Build with address sanitizer and all supported sanitizers other than memory sanitizer")
add_feature_info(WITH_MSAN WITH_MSAN "Build with memory sanitizer")
add_feature_info(WITH_FUZZERS WITH_FUZZERS "Build test/fuzz")
add_feature_info(WITH_NEW_STRATEGIES WITH_NEW_STRATEGIES "Use new strategies")
if("${ARCH}" MATCHES "arm" OR "${ARCH}" MATCHES "aarch64")
    add_feature_info(WITH_ACLE WITH_ACLE "Build with ACLE CRC")
    add_feature_info(WITH_NEON WITH_NEON "Build with NEON intrinsics")
endif()

if (ZLIB_COMPAT)
    add_definitions(-DZLIB_COMPAT)
    set(WITH_GZFILEOP ON)
    set(LIBNAME1 libz)
    set(LIBNAME2 zlib)
    set(SUFFIX "")
else()
    set(LIBNAME1 libz-ng)
    set(LIBNAME2 zlib-ng)
    set(SUFFIX "-ng")
endif()

if(WITH_GZFILEOP)
    add_definitions(-DWITH_GZFILEOP)
endif()

if(${CMAKE_C_COMPILER} MATCHES "icc" OR ${CMAKE_C_COMPILER} MATCHES "icpc" OR ${CMAKE_C_COMPILER} MATCHES "icl")
    if(WITH_NATIVE_INSTRUCTIONS)
        message(STATUS "Ignoring WITH_NATIVE_INSTRUCTIONS; not supported on this configuration")
    endif()
    if(CMAKE_HOST_UNIX)
        if(NOT SSE2FLAG)
            set(SSE2FLAG "-msse2")
        endif()
        if(NOT SSE4FLAG)
            set(SSE4FLAG "-msse4.2")
        endif()
    else()
        if(NOT SSE2FLAG)
            set(SSE2FLAG "/arch:SSE2")
        endif()
        if(NOT SSE4FLAG)
            set(SSE4FLAG "/arch:SSE4.2")
        endif()
    endif()
elseif(MSVC)
    # TODO. ICC can be used through MSVC. I'm not sure if we'd ever see that combination
    # (who'd use cmake from an IDE...) but checking for ICC before checking for MSVC should
    # avoid mistakes.
    # /Oi ?
    if(NOT ${ARCH} MATCHES "AMD64")
        set(SSE2FLAG "/arch:SSE2")
    endif()
    if("${ARCH}" MATCHES "arm")
        add_definitions("-D_ARM_WINAPI_PARTITION_DESKTOP_SDK_AVAILABLE=1")
        set(NEONFLAG "/arch:VFPv4")
    endif()
    if(WITH_NATIVE_INSTRUCTIONS)
        message(STATUS "Ignoring WITH_NATIVE_INSTRUCTIONS; not supported on this configuration")
    endif()
else()
    # catch all GNU C compilers as well as Clang and AppleClang
    if(CMAKE_C_COMPILER_ID MATCHES "GNU" OR CMAKE_C_COMPILER_ID MATCHES "Clang")
        set(__GNUC__ ON)
    endif()
    if(WITH_NATIVE_INSTRUCTIONS)
        if(__GNUC__)
            set(NATIVEFLAG "-march=native")
        else()
            message(STATUS "Ignoring WITH_NATIVE_INSTRUCTIONS; not implemented yet on this configuration")
        endif()
    endif()
    if(__GNUC__ AND "${ARCH}" MATCHES "arm")
        execute_process(COMMAND ${CMAKE_C_COMPILER} "-dumpmachine"
                        OUTPUT_VARIABLE GCC_MACHINE)
        if ("${GCC_MACHINE}" MATCHES "eabihf")
            set(FLOATABI "-mfloat-abi=hard")
        else()
            set(FLOATABI "-mfloat-abi=softfp")
        endif()
    endif()
    if(NOT NATIVEFLAG)
        if(NOT SSE2FLAG)
            if(__GNUC__)
                set(SSE2FLAG "-msse2")
            endif()
        endif()
        if(NOT SSE4FLAG)
            if(__GNUC__)
                set(SSE4FLAG "-msse4")
            endif()
        endif()
        if(NOT PCLMULFLAG)
            if(__GNUC__)
                set(PCLMULFLAG "-mpclmul")
            endif()
        endif()
        if("${ARCH}" MATCHES "arm")
            set(ACLEFLAG "-march=armv8-a+crc")
            set(NEONFLAG "${FLOATABI} -mfpu=neon")
        elseif("${ARCH}" MATCHES "aarch64")
            set(ACLEFLAG "-march=armv8-a+crc")
            set(NEONFLAG "-march=armv8-a+crc+simd")
        endif()
    else()
        set(SSE2FLAG ${NATIVEFLAG})
        set(SSE4FLAG ${NATIVEFLAG})
        set(PCLMULFLAG ${NATIVEFLAG})
        if("${ARCH}" MATCHES "arm")
            set(ACLEFLAG "${NATIVEFLAG}")
            set(NEONFLAG "${FLOATABI} -mfpu=neon")
        elseif("${ARCH}" MATCHES "aarch64")
            set(ACLEFLAG "${NATIVEFLAG}")
            set(NEONFLAG "${NATIVEFLAG}")
        endif()
    endif()
endif()

#
# Check to see if we have large file support
#
set(CMAKE_REQUIRED_DEFINITIONS -D_LARGEFILE64_SOURCE=1 -D__USE_LARGEFILE64)
# We add these other definitions here because CheckTypeSize.cmake
# in CMake 2.4.x does not automatically do so and we want
# compatibility with CMake 2.4.x.
if(HAVE_SYS_TYPES_H)
    list(APPEND CMAKE_REQUIRED_DEFINITIONS -DHAVE_SYS_TYPES_H)
endif()
if(HAVE_STDINT_H)
    list(APPEND CMAKE_REQUIRED_DEFINITIONS -DHAVE_STDINT_H)
endif()
if(HAVE_STDDEF_H)
    list(APPEND CMAKE_REQUIRED_DEFINITIONS -DHAVE_STDDEF_H)
endif()
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

if(WITH_SANITIZERS AND WITH_MSAN)
    message(FATAL_ERROR "Memory sanitizer is incompatible with address sanitizer")
endif()

if(WITH_MSAN)
    set(CMAKE_REQUIRED_FLAGS "-fsanitize=memory")
    check_c_source_compiles("int main() { return 0; }" HAS_MSAN FAIL_REGEX "not supported")
    if(${HAS_MSAN})
        set(SANITIZERS_FLAGS "-fsanitize=memory")
        message(STATUS "Adding memory sanitizer flag: ${SANITIZERS_FLAGS}")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${SANITIZERS_FLAGS}")
    endif()
endif()

if(WITH_SANITIZERS)
    set(_sanitize_flags
        bool
        address
        array-bounds
        float-divide-by-zero
        function
        integer-divide-by-zero
        return
        shift
        signed-integer-overflow
        undefined
        unsigned-integer-overflow
        vla-bound
        vptr
       )
    set(SANITIZERS_FLAGS "")
    foreach(_flag ${_sanitize_flags})
        set(CMAKE_REQUIRED_FLAGS "-fsanitize=${_flag}")
        check_c_source_compiles("int main() { return 0; }"
          HAS_SANITIZER_${_flag} FAIL_REGEX "not supported")
        if(${HAS_SANITIZER_${_flag}})
            if("${SANITIZERS_FLAGS}" STREQUAL "")
                set(SANITIZERS_FLAGS "-fsanitize=${_flag}")
            else()
                set(SANITIZERS_FLAGS "${SANITIZERS_FLAGS},${_flag}")
            endif()
        endif()
        set(CMAKE_REQUIRED_FLAGS)
    endforeach()
    message(STATUS "Adding sanitizers flags: ${SANITIZERS_FLAGS}")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${SANITIZERS_FLAGS}")
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
    add_definitions(-DHAVE_HIDDEN)
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
    add_definitions(-DHAVE_INTERNAL)
endif()

#
# check for __builtin_ctzl() support in the compiler
#
check_c_source_compiles(
    "int main(void)
    {
        unsigned int zero = 0;
        long test = __builtin_ctzl(zero);
        (void)test;
        return 0;
    }"
    HAVE_BUILTIN_CTZL
)
if(HAVE_BUILTIN_CTZL)
    add_definitions(-DHAVE_BUILTIN_CTZL)
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
else()
    #
    # Not MSVC, so we need to check if we have the MS-style SSE etc. intrinsics
    #
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

    if(WITH_NATIVE_INSTRUCTIONS)
        set(CMAKE_REQUIRED_FLAGS "${NATIVEFLAG}")
    else()
        # Use the generic SSE4 enabler option to check for the SSE4.2 instruction we require:
        set(CMAKE_REQUIRED_FLAGS "${SSE4FLAG}")
    endif()
    check_c_source_compile_or_run(
        "int main(void)
        {
            unsigned val = 0, h = 0;
            __asm__ __volatile__ ( \"crc32 %1,%0\" : \"+r\" (h) : \"r\" (val) );
            return (int) h;
        }"
        HAVE_SSE42_INTRIN
    )
    check_c_source_compile_or_run(
        "int main(void)
        {
            unsigned crc = 0;
            char c = 'c';
            crc = __builtin_ia32_crc32qi(crc, c);
            (void)crc;
            return 0;
        }"
        HAVE_SSE42CRC_INTRIN
    )
    set(CMAKE_REQUIRED_FLAGS)

    if(WITH_NATIVE_INSTRUCTIONS)
        set(CMAKE_REQUIRED_FLAGS "${NATIVEFLAG}")
    else()
        set(CMAKE_REQUIRED_FLAGS "${PCLMULFLAG}")
    endif()
    if(NOT (APPLE AND ${ARCH} MATCHES "i386"))
        # The pclmul code currently crashes on Mac in 32bit mode. Avoid for now.
        check_c_source_compile_or_run(
            "#include <immintrin.h>
            #include <wmmintrin.h>
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
endif()

# Check whether -mfpu=neon is available
set(CMAKE_REQUIRED_FLAGS "-mfpu=neon")
check_c_source_compiles(
    "int main()
    {
      return 0;
    }"
    MFPU_NEON_AVAILABLE FAIL_REGEX "not supported")
set(CMAKE_REQUIRED_FLAGS)

# FORCE_SSE2 option will only be shown if HAVE_SSE2_INTRIN is true
if("${ARCH}" MATCHES "i[3-6]86")
    cmake_dependent_option(FORCE_SSE2 "Always assume CPU is SSE2 capable" OFF "HAVE_SSE2_INTRIN" OFF)
endif()

#
# Enable deflate_medium at level 4-6
#
if(NOT WITH_NEW_STRATEGIES)
    add_definitions(-DNO_MEDIUM_STRATEGY)
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
set(ARCHDIR "arch/generic")
if("${ARCH}" MATCHES "x86_64" OR "${ARCH}" MATCHES "AMD64" OR "${ARCH}" MATCHES "i[3-6]86")
    set(ARCHDIR "arch/x86")
    add_definitions(-DUNALIGNED_OK)
    add_feature_info(SSE2 1 "Support the SSE2 instruction set, using \"${SSE2FLAG}\"")
elseif("${ARCH}" MATCHES "arm" OR "${ARCH}" MATCHES "aarch64")
    set(ARCHDIR "arch/arm")
    add_definitions(-DUNALIGNED_OK)
elseif("${ARCH}" MATCHES "s390x")
    set(ARCHDIR "arch/s390")
else()
    message(STATUS "No optimized architecture: using ${ARCHDIR}")
endif()
if("${ARCH}" MATCHES "arm" OR "${ARCH}" MATCHES "aarch64")
    set(ZLIB_ARCH_SRCS ${ZLIB_ARCH_SRCS} ${ARCHDIR}/armfeature.c ${ARCHDIR}/fill_window_arm.c)
endif()
if(WITH_OPTIM)
    if("${ARCH}" MATCHES "arm")
        if(WITH_ACLE)
            set(ZLIB_ARCH_SRCS ${ZLIB_ARCH_SRCS} ${ARCHDIR}/crc32_acle.c ${ARCHDIR}/insert_string_acle.c)
            set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${ACLEFLAG}")
            add_definitions("-DARM_ACLE_CRC_HASH")
            add_feature_info(ACLE_CRC 1 "Support CRC hash generation using the ACLE instruction set, using \"${ACLEFLAG}\"")
        endif()
        if(WITH_NEON)
            if(MFPU_NEON_AVAILABLE)
                set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${NEONFLAG}")
            endif()
            add_definitions("-DARM_NEON_ADLER32")
            if(MSVC)
                add_definitions("-D__ARM_NEON__=1")
            endif(MSVC)
            set(ZLIB_ARCH_SRCS ${ZLIB_ARCH_SRCS} ${ARCHDIR}/adler32_neon.c)
            add_feature_info(NEON_FILLWINDOW 1 "Support NEON instructions in fill_window_arm, using \"${NEONFLAG}\"")
        endif()
    elseif("${ARCH}" MATCHES "aarch64")
        if(WITH_ACLE)
            set(ZLIB_ARCH_SRCS ${ZLIB_ARCH_SRCS} ${ARCHDIR}/crc32_acle.c ${ARCHDIR}/insert_string_acle.c)
            add_definitions("-DARM_ACLE_CRC_HASH")
            add_feature_info(ACLE_CRC 1 "Support CRC hash generation using the ACLE instruction set, using \"${ACLEFLAG}\"")
        endif()
        # We need to check WITH_NEON first
        if(WITH_NEON)
            add_definitions("-DARM_NEON_ADLER32")
            set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${NEONFLAG}")
            set(ZLIB_ARCH_SRCS ${ZLIB_ARCH_SRCS} ${ARCHDIR}/adler32_neon.c)
            add_feature_info(NEON_FILLWINDOW 1 "Support NEON instructions in fill_window_arm, using \"${NEONFLAG}\"")
        elseif(WITH_ACLE)
            set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${ACLEFLAG}")
        endif()
    elseif("${ARCHDIR}" MATCHES "arch/x86")
        add_definitions("-DX86_CPUID")
        set(ZLIB_ARCH_SRCS ${ZLIB_ARCH_SRCS} ${ARCHDIR}/x86.c)
        if(HAVE_SSE42_INTRIN)
            add_definitions(-DX86_SSE4_2_CRC_HASH)
            set(ZLIB_ARCH_SRCS ${ZLIB_ARCH_SRCS} ${ARCHDIR}/insert_string_sse.c)
            add_feature_info(SSE4_CRC 1 "Support CRC hash generation using the SSE4.2 instruction set, using \"${SSE4FLAG}\"")
            add_intrinsics_option("${SSE4FLAG}")
            if(HAVE_SSE42CRC_INTRIN)
                add_definitions(-DX86_SSE4_2_CRC_INTRIN)
            endif()
            if(WITH_NEW_STRATEGIES)
                add_definitions(-DX86_QUICK_STRATEGY)
                set(ZLIB_ARCH_SRCS ${ZLIB_ARCH_SRCS} ${ARCHDIR}/deflate_quick.c)
                add_feature_info(SSE4DEFLATE 1 "Support SSE4.2-accelerated quick compression")
            endif()
        endif()
        if(HAVE_SSE2_INTRIN)
            add_definitions(-DX86_SSE2)
            set(ZLIB_ARCH_SRCS ${ZLIB_ARCH_SRCS} ${ARCHDIR}/fill_window_sse.c)
            if(NOT ${ARCH} MATCHES "x86_64")
                add_intrinsics_option("${SSE2FLAG}")
                add_feature_info(FORCE_SSE2 FORCE_SSE2 "Assume CPU is SSE2 capable")
                if(FORCE_SSE2)
                    add_definitions(-DX86_NOCHECK_SSE2)
                endif()
            endif()
        endif()
        if(HAVE_PCLMULQDQ_INTRIN)
            add_definitions(-DX86_PCLMULQDQ_CRC)
            set(ZLIB_ARCH_SRCS ${ZLIB_ARCH_SRCS} ${ARCHDIR}/crc_folding.c)
            add_intrinsics_option("${PCLMULFLAG}")
            if(HAVE_SSE42_INTRIN)
                add_feature_info(PCLMUL_CRC 1 "Support CRC hash generation using PCLMULQDQ, using \"${PCLMULFLAG}\"")
            else()
                add_feature_info(PCLMUL_CRC 1 "Support CRC hash generation using PCLMULQDQ, using \"${PCLMULFLAG} ${SSE4FLAG}\"")
            endif()
        endif()
    elseif("${ARCH}" MATCHES "s390x")
        if(WITH_DFLTCC_DEFLATE OR WITH_DFLTCC_INFLATE)
            set(ZLIB_ARCH_SRCS ${ZLIB_ARCH_SRCS} ${ARCHDIR}/dfltcc_common.c)
        endif()
        if(WITH_DFLTCC_DEFLATE)
            add_definitions(-DS390_DFLTCC_DEFLATE)
            set(ZLIB_ARCH_SRCS ${ZLIB_ARCH_SRCS} ${ARCHDIR}/dfltcc_deflate.c)
        endif()
        if(WITH_DFLTCC_INFLATE)
            add_definitions(-DS390_DFLTCC_INFLATE)
            set(ZLIB_ARCH_SRCS ${ZLIB_ARCH_SRCS} ${ARCHDIR}/dfltcc_inflate.c)
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

set(ZLIB_PC ${CMAKE_CURRENT_BINARY_DIR}/${LIBNAME2}.pc)
configure_file( ${CMAKE_CURRENT_SOURCE_DIR}/zlib.pc.cmakein
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
    crc32.h
    deflate.h
    functable.h
    gzguts.h
    inffast.h
    inffixed.h
    inflate.h
    inftrees.h
    trees.h
    zutil.h
)
set(ZLIB_SRCS
    adler32.c
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

add_library(zlib SHARED ${ZLIB_SRCS} ${ZLIB_ARCH_SRCS} ${ZLIB_ASMS} ${ZLIB_DLL_SRCS} ${ZLIB_PUBLIC_HDRS} ${ZLIB_PRIVATE_HDRS})
target_include_directories(zlib PUBLIC ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR})
add_library(zlibstatic STATIC ${ZLIB_SRCS} ${ZLIB_ARCH_SRCS} ${ZLIB_ASMS} ${ZLIB_PUBLIC_HDRS} ${ZLIB_PRIVATE_HDRS})
target_include_directories(zlibstatic PUBLIC ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR})

if(WITH_GZFILEOP)
    target_sources(zlib PRIVATE ${ZLIB_GZFILE_SRCS})
    target_sources(zlibstatic PRIVATE ${ZLIB_GZFILE_SRCS})
endif()

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
    # On unix-like platforms the library is almost always called libz
    set_target_properties(zlib zlibstatic PROPERTIES OUTPUT_NAME z${SUFFIX})
    if(NOT APPLE)
        set_target_properties(zlib PROPERTIES LINK_FLAGS "-Wl,--version-script,\"${CMAKE_CURRENT_SOURCE_DIR}/${LIBNAME2}.map\"")
    endif()
elseif(MSYS)
    # Suppress version number from shared library name
    set(CMAKE_SHARED_LIBRARY_NAME_WITH_VERSION 0)
elseif(BUILD_SHARED_LIBS AND WIN32)
    # Creates zlib1.dll when building shared library version
    set_target_properties(zlib PROPERTIES SUFFIX "1.dll")
endif()

if(NOT SKIP_INSTALL_LIBRARIES AND NOT SKIP_INSTALL_ALL )
    install(TARGETS zlib zlibstatic
        RUNTIME DESTINATION "${INSTALL_BIN_DIR}"
        ARCHIVE DESTINATION "${INSTALL_LIB_DIR}"
        LIBRARY DESTINATION "${INSTALL_LIB_DIR}" )
endif()
if(NOT SKIP_INSTALL_HEADERS AND NOT SKIP_INSTALL_ALL )
    install(FILES zlib${SUFFIX}.h DESTINATION "${INSTALL_INC_DIR}" RENAME zlib${SUFFIX}.h)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/zconf${SUFFIX}.h DESTINATION "${INSTALL_INC_DIR}" RENAME zconf${SUFFIX}.h)
endif()
if(NOT SKIP_INSTALL_FILES AND NOT SKIP_INSTALL_ALL )
    install(FILES zlib.3 DESTINATION "${INSTALL_MAN_DIR}/man3" RENAME zlib${SUFFIX}.3)
endif()
if(NOT SKIP_INSTALL_FILES AND NOT SKIP_INSTALL_ALL )
    install(FILES ${ZLIB_PC} DESTINATION "${INSTALL_PKGCONFIG_DIR}")
endif()

#============================================================================
# Example binaries
#============================================================================

option(ZLIB_ENABLE_TESTS "Build test binaries" ON)
if (ZLIB_ENABLE_TESTS)
    enable_testing()
    macro(configure_test_executable target)
        target_link_libraries(${target} zlib)
        if(NOT WITH_GZFILEOP)
            target_compile_definitions(${target} PUBLIC -DWITH_GZFILEOP)
            target_sources(${target} PRIVATE ${ZLIB_GZFILE_SRCS})
        endif()
    endmacro()

    add_executable(example test/example.c)
    configure_test_executable(example)
    add_test(NAME example COMMAND example${CMAKE_EXECUTABLE_SUFFIX})

    add_executable(minigzip test/minigzip.c)
    configure_test_executable(minigzip)

    if(HAVE_OFF64_T)
        add_executable(example64 test/example.c)
        configure_test_executable(example64)
        set_target_properties(example64 PROPERTIES COMPILE_FLAGS "-D_FILE_OFFSET_BITS=64")
        add_test(NAME example64 COMMAND example64${CMAKE_EXECUTABLE_SUFFIX})

        add_executable(minigzip64 test/minigzip.c)
        configure_test_executable(minigzip64)
        set_target_properties(minigzip64 PROPERTIES COMPILE_FLAGS "-D_FILE_OFFSET_BITS=64")
    endif()

    if(WITH_FUZZERS)
        set(FUZZERS checksum compress example_small example_large example_flush example_dict minigzip)
        file(GLOB ALL_SRC_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*")
        foreach(FUZZER ${FUZZERS})
            add_executable(${FUZZER}_fuzzer test/fuzz/${FUZZER}_fuzzer.c test/fuzz/standalone_fuzz_target_runner.c)
            configure_test_executable(${FUZZER}_fuzzer)
            add_test(${FUZZER}_fuzzer ${FUZZER}_fuzzer${CMAKE_EXECUTABLE_SUFFIX} ${ALL_SRC_FILES})
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

    if(NOT WIN32 AND ZLIB_COMPAT)
        add_executable(CVE-2003-0107 test/CVE-2003-0107.c)
        target_link_libraries(CVE-2003-0107 zlib)
        add_test(NAME CVE-2003-0107 COMMAND CVE-2003-0107)
    endif()
endif()

FEATURE_SUMMARY(WHAT ALL INCLUDE_QUIET_PACKAGES)

