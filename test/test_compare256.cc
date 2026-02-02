/* test_compare256.cc -- compare256 unit tests
 * Copyright (C) 2022 Nathan Moinvaziri
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

extern "C" {
#  include "zbuild.h"
#  include "zutil.h"
#  include "arch_functions.h"
#  include "test_cpu_features.h"
}

#include <gtest/gtest.h>

#include "test_shared.h"

#define MAX_COMPARE_SIZE (256)


/* Ensure that compare256 returns the correct match length */
static inline void compare256_match_check(compare256_func compare256) {
    int32_t match_len, i;
    uint8_t *str1;
    uint8_t *str2;

    str1 = (uint8_t *)PREFIX(zcalloc)(NULL, 1, MAX_COMPARE_SIZE);
    ASSERT_TRUE(str1 != NULL);
    memset(str1, 'a', MAX_COMPARE_SIZE);

    str2 = (uint8_t *)PREFIX(zcalloc)(NULL, 1, MAX_COMPARE_SIZE);
    ASSERT_TRUE(str2 != NULL);
    memset(str2, 'a', MAX_COMPARE_SIZE);

    for (i = 0; i <= MAX_COMPARE_SIZE; i++) {
        if (i < MAX_COMPARE_SIZE)
            str2[i] = 0;

        match_len = compare256(str1, str2);
        EXPECT_EQ(match_len, i);

        if (i < MAX_COMPARE_SIZE)
            str2[i] = 'a';
    }

    PREFIX(zcfree)(NULL, str1);
    PREFIX(zcfree)(NULL, str2);
}

#define TEST_COMPARE256(name, func, support_flag) \
    TEST(compare256, name) { \
        if (!(support_flag)) { \
            GTEST_SKIP(); \
            return; \
        } \
        compare256_match_check(func); \
    }

#ifdef DISABLE_RUNTIME_CPU_DETECTION
TEST_COMPARE256(native, native_compare256, 1)
#else

#ifdef WITH_ALL_FALLBACKS
TEST_COMPARE256(8, compare256_8, 1)
TEST_COMPARE256(64, compare256_64, 1)
#endif

#ifdef X86_SSE2
TEST_COMPARE256(sse2, compare256_sse2, test_cpu_features.x86.has_sse2)
#endif
#ifdef X86_AVX2
TEST_COMPARE256(avx2, compare256_avx2, test_cpu_features.x86.has_avx2)
#endif
#ifdef X86_AVX512
TEST_COMPARE256(avx512, compare256_avx512, test_cpu_features.x86.has_avx512_common)
#endif
#ifdef ARM_NEON
TEST_COMPARE256(neon, compare256_neon, test_cpu_features.arm.has_neon)
#endif
#ifdef POWER9
TEST_COMPARE256(power9, compare256_power9, test_cpu_features.power.has_arch_3_00)
#endif
#ifdef RISCV_RVV
TEST_COMPARE256(rvv, compare256_rvv, test_cpu_features.riscv.has_rvv)
#endif
#ifdef LOONGARCH_LSX
TEST_COMPARE256(lsx, compare256_lsx, test_cpu_features.loongarch.has_lsx)
#endif
#ifdef LOONGARCH_LASX
TEST_COMPARE256(lasx, compare256_lasx, test_cpu_features.loongarch.has_lasx)
#endif

#endif
