/* x86_features.h -- check for CPU features
 * Copyright (C) 2013 Intel Corporation Jim Kukunas
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#ifndef X86_FEATURES_H_
#define X86_FEATURES_H_

struct x86_cpu_features {
    int has_avx2;
    int has_avx512f;
    int has_avx512dq;
    int has_avx512bw;
    int has_avx512vl;
    int has_avx512_common; // Enabled when AVX512(F,DQ,BW,VL) are all enabled.
    int has_avx512vnni;
    int has_bmi2;
    int has_sse2;
    int has_ssse3;
    int has_sse41;
    int has_sse42;
    int has_pclmulqdq;
    int has_vpclmulqdq;
    int has_os_save_ymm;
    int has_os_save_zmm;
};

void Z_INTERNAL x86_check_features(struct x86_cpu_features *features);

#if defined(__SSE2__)
#define has_static_sse2 1
#else
#define has_static_sse2 0
#endif
#if defined(__SSSE3__)
#define has_static_ssse3 1
#else
#define has_static_ssse3 0
#endif
#if defined(__SSE4_1__)
#define has_static_sse41 1
#else
#define has_static_sse41 0
#endif
#if defined(__SSE4_2__)
#define has_static_sse42 1
#else
#define has_static_sse42 0
#endif
#if defined(__PCLMUL__)
#define has_static_pclmulqdq 1
#else
#define has_static_pclmulqdq 0
#endif
#if defined(__AVX2__)
#define has_static_avx2 1
#else
#define has_static_avx2 0
#endif
#if defined(__BMI2__)
#define has_static_bmi2 1
#else
#define has_static_bmi2 0
#endif
#if defined(__AVX512F__) && defined(__AVX512DQ__) && defined(__AVX512BW__) && defined(__AVX512VL__) && defined(__BMI2__)
#define has_static_avx512_common 1
#else
#define has_static_avx512_common 0
#endif
#if defined(__AVX512VNNI__)
#define has_static_avx512vnni 1
#else
#define has_static_avx512vnni 0
#endif
#if defined(__VPCLMULQDQ__)
#define has_static_vpclmulqdq 1
#else
#define has_static_vpclmulqdq 0
#endif

#endif /* X86_FEATURES_H_ */
