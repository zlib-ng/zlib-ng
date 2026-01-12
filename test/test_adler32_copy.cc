/* test_adler32_copy.cc -- test for adler32 implementations while copying
 * Copyright (C) 2025 Hans Kristian Rosbach
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#include <gtest/gtest.h>

extern "C" {
#  include "zbuild.h"
#  include "arch_functions.h"
#  include "test_cpu_features.h"
#  include "hash_test_strings_p.h"
}

class adler32_copy_variant : public ::testing::TestWithParam<hash_test> {
protected:
    uint8_t dstbuf[HASH_TEST_MAX_LENGTH];

public:
    /* Ensure that adler32 copy functions returns the correct adler and copies the data */
    void adler32_copy_test(adler32_copy_func copyfunc, hash_test params) {
        ASSERT_LE(params.len, HASH_TEST_MAX_LENGTH);

        uint32_t adler = copyfunc(params.initial_adler, dstbuf, params.buf, params.len);

        EXPECT_EQ(adler, params.expect_adler);
        EXPECT_EQ(0, memcmp(params.buf, dstbuf, params.len));
    }
};

INSTANTIATE_TEST_SUITE_P(adler32_copy, adler32_copy_variant, testing::ValuesIn(hash_tests));

#define TEST_ADLER32_COPY(name, copyfunc, support_flag) \
    TEST_P(adler32_copy_variant, name) { \
        if (!(support_flag)) { \
            GTEST_SKIP(); \
            return; \
        } \
        adler32_copy_test(copyfunc, GetParam()); \
    }

// Base test
TEST_ADLER32_COPY(c, adler32_copy_c, 1)

#ifdef DISABLE_RUNTIME_CPU_DETECTION
    // Native test
    TEST_ADLER32_COPY(native, native_adler32_copy, 1)
#else

#ifdef ARM_NEON
TEST_ADLER32_COPY(neon, adler32_copy_neon, test_cpu_features.arm.has_neon)
#elif defined(POWER8_VSX)
TEST_ADLER32_COPY(power8, adler32_copy_power8, test_cpu_features.power.has_arch_2_07)
#elif defined(PPC_VMX)
TEST_ADLER32_COPY(vmx, adler32_copy_vmx, test_cpu_features.power.has_altivec)
#elif defined(RISCV_RVV)
TEST_ADLER32_COPY(rvv, adler32_copy_rvv, test_cpu_features.riscv.has_rvv)
#endif

#ifdef X86_SSSE3
TEST_ADLER32_COPY(ssse3, adler32_copy_ssse3, test_cpu_features.x86.has_ssse3)
#endif
#ifdef X86_SSE42
TEST_ADLER32_COPY(sse42, adler32_copy_sse42, test_cpu_features.x86.has_sse42)
#endif
#ifdef X86_AVX2
TEST_ADLER32_COPY(avx2, adler32_copy_avx2, test_cpu_features.x86.has_avx2)
#endif
#ifdef X86_AVX512
TEST_ADLER32_COPY(avx512, adler32_copy_avx512, test_cpu_features.x86.has_avx512_common)
#endif
#ifdef X86_AVX512VNNI
TEST_ADLER32_COPY(avx512_vnni, adler32_copy_avx512_vnni, test_cpu_features.x86.has_avx512vnni)
#endif

#ifdef LOONGARCH_LSX
TEST_ADLER32_COPY(lsx, adler32_copy_lsx, test_cpu_features.loongarch.has_lsx)
#endif
#ifdef LOONGARCH_LASX
TEST_ADLER32_COPY(lasx, adler32_copy_lasx, test_cpu_features.loongarch.has_lasx)
#endif

#endif
