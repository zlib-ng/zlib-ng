# detect-intrinsics.cmake -- Detect compiler intrinsics support
# Licensed under the Zlib license, see LICENSE.md for details

macro(check_acle_intrinsics)
    if(NOT NATIVEFLAG)
        set(ACLEFLAG "-march=armv8-a+crc")
    endif()
    # Check whether compiler supports ACLE flag
    set(CMAKE_REQUIRED_FLAGS "${ACLEFLAG}")
    check_c_source_compiles(
        "int main() { return 0; }"
        HAVE_ACLE_INTRIN FAIL_REGEX "not supported")
    set(CMAKE_REQUIRED_FLAGS)
endmacro()

macro(check_avx2_intrinsics)
    if(CMAKE_C_COMPILER_ID MATCHES "Intel")
        if(CMAKE_HOST_UNIX OR APPLE)
            set(AVX2FLAG "-mavx2")
        else()
            set(AVX2FLAG "/arch:AVX2")
        endif()
    elseif(CMAKE_C_COMPILER_ID MATCHES "GNU" OR CMAKE_C_COMPILER_ID MATCHES "Clang")
        if(NOT NATIVEFLAG)
            set(AVX2FLAG "-mavx2")
        endif()
    endif()
    # Check whether compiler supports AVX2 intrinics
    set(CMAKE_REQUIRED_FLAGS "${AVX2FLAG}")
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
endmacro()

macro(check_neon_intrinsics)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU" OR CMAKE_C_COMPILER_ID MATCHES "Clang")
        if(NOT NATIVEFLAG)
            if("${ARCH}" MATCHES "aarch64")
                set(NEONFLAG "-march=armv8-a+simd")
            else()
                set(NEONFLAG "-mfpu=neon")
            endif()
        endif()
    endif()
    # Check whether compiler supports NEON flag
    set(CMAKE_REQUIRED_FLAGS "${NEONFLAG}")
    check_c_source_compiles(
        "int main() { return 0; }"
        MFPU_NEON_AVAILABLE FAIL_REGEX "not supported")
    set(CMAKE_REQUIRED_FLAGS)
endmacro()

macro(check_pclmulqdq_intrinsics)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU" OR CMAKE_C_COMPILER_ID MATCHES "Clang")
        if(NOT NATIVEFLAG)
            set(PCLMULFLAG "-mpclmul")
        endif()
    endif()
    # Check whether compiler supports PCLMULQDQ intrinsics
    if(NOT (APPLE AND "${ARCH}" MATCHES "i386"))
        # The pclmul code currently crashes on Mac in 32bit mode. Avoid for now.
        set(CMAKE_REQUIRED_FLAGS "${PCLMULFLAG}")
        check_c_source_compile_or_run(
            "#include <immintrin.h>
            int main(void) {
                __m128i a = _mm_setzero_si128();
                __m128i b = _mm_setzero_si128();
                __m128i c = _mm_clmulepi64_si128(a, b, 0x10);
                (void)c;
                return 0;
            }"
            HAVE_PCLMULQDQ_INTRIN
        )
        set(CMAKE_REQUIRED_FLAGS)
    else()
        set(HAVE_PCLMULQDQ_INTRIN OFF)
    endif()
endmacro()

macro(check_power8_intrinsics)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU" OR CMAKE_C_COMPILER_ID MATCHES "Clang")
        if(NOT NATIVEFLAG)
            set(POWER8FLAG "-mcpu=power8")
        endif()
    endif()
    # Check if we have what we need for POWER8 optimizations
    set(CMAKE_REQUIRED_FLAGS "${POWER8FLAG}")
    check_c_source_compiles(
        "#include <sys/auxv.h>
        int main() {
            return (getauxval(AT_HWCAP2) & PPC_FEATURE2_ARCH_2_07);
        }"
        HAVE_POWER8_INTRIN
    )
    set(CMAKE_REQUIRED_FLAGS)
endmacro()

macro(check_sse2_intrinsics)
    if(CMAKE_C_COMPILER_ID MATCHES "Intel")
        if(CMAKE_HOST_UNIX OR APPLE)
            set(SSE2FLAG "-msse2")
        else()
            set(SSE2FLAG "/arch:SSE2")
        endif()
    elseif(MSVC)
        if(NOT "${ARCH}" MATCHES "x86_64")
            set(SSE2FLAG "/arch:SSE2")
        endif()
    elseif(CMAKE_C_COMPILER_ID MATCHES "GNU" OR CMAKE_C_COMPILER_ID MATCHES "Clang")
        if(NOT NATIVEFLAG)
            set(SSE2FLAG "-msse2")
        endif()
    endif()
    # Check whether compiler supports SSE2 instrinics
    set(CMAKE_REQUIRED_FLAGS "${SSE2FLAG}")
    check_c_source_compile_or_run(
        "#include <immintrin.h>
        int main(void) {
            __m128i zero = _mm_setzero_si128();
            (void)zero;
            return 0;
        }"
        HAVE_SSE2_INTRIN
    )
    set(CMAKE_REQUIRED_FLAGS)
endmacro()

macro(check_ssse3_intrinsics)
    if(CMAKE_C_COMPILER_ID MATCHES "Intel")
        if(CMAKE_HOST_UNIX OR APPLE)
            set(SSSE3FLAG "-mssse3")
        else()
            set(SSSE3FLAG "/arch:SSSE3")
        endif()
    elseif(CMAKE_C_COMPILER_ID MATCHES "GNU" OR CMAKE_C_COMPILER_ID MATCHES "Clang")
        if(NOT NATIVEFLAG)
            set(SSSE3FLAG "-mssse3")
        endif()
    endif()
    # Check whether compiler supports SSSE3 intrinsics
    set(CMAKE_REQUIRED_FLAGS "${SSSE3FLAG}")
    check_c_source_compile_or_run(
        "#include <immintrin.h>
        int main(void) {
            __m128i u, v, w;
            u = _mm_set1_epi32(1);
            v = _mm_set1_epi32(2);
            w = _mm_hadd_epi32(u, v);
            (void)w;
            return 0;
        }"
        HAVE_SSSE3_INTRIN
    )
endmacro()

macro(check_sse4_intrinsics)
    if(CMAKE_C_COMPILER_ID MATCHES "Intel")
        if(CMAKE_HOST_UNIX OR APPLE)
            set(SSE4FLAG "-msse4.2")
        else()
            set(SSE4FLAG "/arch:SSE4.2")
        endif()
    elseif(CMAKE_C_COMPILER_ID MATCHES "GNU" OR CMAKE_C_COMPILER_ID MATCHES "Clang")
        if(NOT NATIVEFLAG)
            set(SSE4FLAG "-msse4")
        endif()
    endif()
    # Check whether compiler supports SSE4 CRC inline asm
    set(CMAKE_REQUIRED_FLAGS "${SSE4FLAG}")
    check_c_source_compile_or_run(
        "int main(void) {
            unsigned val = 0, h = 0;
        #if defined(_MSC_VER)
            { __asm mov edx, h __asm mov eax, val __asm crc32 eax, edx __asm mov val, eax }
        #else
            __asm__ __volatile__ ( \"crc32 %1,%0\" : \"+r\" (h) : \"r\" (val) );
        #endif
            return (int)h;
        }"
        HAVE_SSE42CRC_INLINE_ASM
    )
    # Check whether compiler supports SSE4 CRC intrinsics
    check_c_source_compile_or_run(
        "#include <immintrin.h>
        int main(void) {
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
        int main(void) {
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
endmacro()

macro(check_vgfma_intrinsics)
    if(NOT NATIVEFLAG)
        set(VGFMAFLAG "-march=z13")
        if(CMAKE_C_COMPILER_ID MATCHES "GNU")
            set(VGFMAFLAG "${VGFMAFLAG} -mzarch")
        endif()
        if(CMAKE_C_COMPILER_ID MATCHES "Clang")
            set(VGFMAFLAG "${VGFMAFLAG} -fzvector")
        endif()
    endif()
    # Check whether compiler supports "VECTOR GALOIS FIELD MULTIPLY SUM AND ACCUMULATE" intrinsic
    set(CMAKE_REQUIRED_FLAGS "${VGFMAFLAG}")
    check_c_source_compiles(
        "#include <vecintrin.h>
        int main(void) {
            unsigned long long a __attribute__((vector_size(16))) = { 0 };
            unsigned long long b __attribute__((vector_size(16))) = { 0 };
            unsigned char c __attribute__((vector_size(16))) = { 0 };
            c = vec_gfmsum_accum_128(a, b, c);
            return c[0];
        }"
        HAVE_VGFMA_INTRIN FAIL_REGEX "not supported")
    set(CMAKE_REQUIRED_FLAGS)
endmacro()
