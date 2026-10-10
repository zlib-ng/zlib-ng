/* test_adler32.c -- unit test for adler32 in the zlib compression library
 * Copyright (C) 2020 IBM Corporation
 * Author: Rogerio Alves <rcardoso@linux.ibm.com>
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

extern "C" {
#  include "zbuild.h"
#  include "arch_functions.h"
#  include "adler32_p.h"
#  include "test_cpu_features.h"
#  include "hash_test_strings_p.h"
}

#include <gtest/gtest.h>

class adler32_variant : public ::testing::TestWithParam<hash_test> {
public:
    void hash(hash_test param, adler32_func adler32) {
        uint32_t adler = adler32((uint32_t)param.initial_adler, param.buf, param.len);
        EXPECT_EQ(adler, param.expect_adler);
    }
};

INSTANTIATE_TEST_SUITE_P(adler32, adler32_variant, testing::ValuesIn(hash_tests));

/* Bytewise reference that reduces after every byte */
static uint32_t adler32_ref(uint32_t adler, const uint8_t *buf, size_t len) {
    uint32_t sum2 = (adler >> 16) & 0xffff;
    adler &= 0xffff;
    while (len--) {
        adler = (adler + *buf++) % BASE;
        sum2 = (sum2 + adler) % BASE;
    }
    return adler | (sum2 << 16);
}

/* The largest sums before a reduction must not overflow at any source alignment */
static void adler32_unaligned(adler32_func adler32) {
    ALIGNED_(64) static uint8_t buf[NMAX + 128];
    memset(buf, 0xff, sizeof(buf));
    for (size_t len = NMAX - 8; len <= NMAX + 40; len++) {
        uint32_t expect = adler32_ref(0xfff0fff0, buf, len);
        for (size_t offset = 0; offset < 32; offset++)
            EXPECT_EQ(adler32(0xfff0fff0, buf + offset, len), expect) << "offset " << offset << " len " << len;
    }
}

#define TEST_ADLER32(name, func, support_flag) \
    TEST_P(adler32_variant, name) { \
        if (!(support_flag)) { \
            GTEST_SKIP(); \
            Z_UNREACHABLE(); \
            return; \
        } \
        hash(GetParam(), func); \
    } \
    TEST(adler32_unaligned, name) { \
        if (!(support_flag)) { \
            GTEST_SKIP(); \
            Z_UNREACHABLE(); \
            return; \
        } \
        adler32_unaligned(func); \
    }

#ifdef ADLER32_FALLBACK
TEST_ADLER32(c, adler32_c, 1)
#endif

#ifdef DISABLE_RUNTIME_CPU_DETECTION
TEST_ADLER32(native, native_adler32, 1)
#else

#ifdef ARM_NEON
TEST_ADLER32(neon, adler32_neon, test_cpu_features.arm.has_neon)
#endif
#ifdef ARM_NEON_DOTPROD
TEST_ADLER32(neon_dotprod, adler32_neon_dotprod, test_cpu_features.arm.has_neon && test_cpu_features.arm.has_dotprod)
#endif
#if defined(POWER8_VSX)
TEST_ADLER32(power8, adler32_power8, test_cpu_features.power.has_arch_2_07)
#elif defined(PPC_VMX)
TEST_ADLER32(vmx, adler32_vmx, test_cpu_features.power.has_altivec)
#elif defined(RISCV_RVV)
TEST_ADLER32(rvv, adler32_rvv, test_cpu_features.riscv.has_rvv)
#endif

#ifdef X86_SSSE3
TEST_ADLER32(ssse3, adler32_ssse3, test_cpu_features.x86.has_ssse3)
#endif
#ifdef X86_AVX2
TEST_ADLER32(avx2, adler32_avx2, test_cpu_features.x86.has_avx2)
#endif
#ifdef X86_AVX2VNNI
TEST_ADLER32(avx2_vnni, adler32_avx2_vnni, test_cpu_features.x86.has_avx2vnni)
#endif
#ifdef X86_AVX512
TEST_ADLER32(avx512, adler32_avx512, test_cpu_features.x86.has_avx512_common)
#endif
#ifdef X86_AVX512VNNI
TEST_ADLER32(avx512_vnni, adler32_avx512_vnni, test_cpu_features.x86.has_avx512vnni)
#endif

#ifdef LOONGARCH_LSX
TEST_ADLER32(lsx, adler32_lsx, test_cpu_features.loongarch.has_lsx)
#endif
#ifdef LOONGARCH_LASX
TEST_ADLER32(lasx, adler32_lasx, test_cpu_features.loongarch.has_lasx)
#endif

#endif
