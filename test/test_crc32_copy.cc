/* test_crc32_copy.cc -- test for crc32 implementations while copying
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

class crc32_copy_variant : public ::testing::TestWithParam<hash_test> {
protected:
    uint8_t dstbuf[HASH_TEST_MAX_LENGTH];

public:
    /* Ensure that crc32 copy functions returns the correct crc and copies the data */
    void crc32_copy_test(crc32_copy_func copyfunc, hash_test params) {
        ASSERT_LE(params.len, HASH_TEST_MAX_LENGTH);

        uint32_t crc = copyfunc(params.initial_crc, dstbuf, params.buf, params.len);

        EXPECT_EQ(crc, params.expect_crc);
        EXPECT_EQ(0, memcmp(params.buf, dstbuf, params.len));
    }
};

INSTANTIATE_TEST_SUITE_P(crc32_copy, crc32_copy_variant, testing::ValuesIn(hash_tests));

#define TEST_CRC32_COPY(name, copyfunc, support_flag) \
    TEST_P(crc32_copy_variant, name) { \
        if (!(support_flag)) { \
            GTEST_SKIP(); \
            return; \
        } \
        crc32_copy_test(copyfunc, GetParam()); \
    }

// Base test
TEST_CRC32_COPY(braid, crc32_copy_braid, 1)

#ifdef DISABLE_RUNTIME_CPU_DETECTION
    // Native test
    TEST_CRC32_COPY(native, native_crc32_copy, 1)
#else
    // Optimized functions
#  ifndef WITHOUT_CHORBA
    TEST_CRC32_COPY(chorba, crc32_copy_chorba, 1)
#  endif
#  ifndef WITHOUT_CHORBA_SSE
#    ifdef X86_SSE2
    TEST_CRC32_COPY(chorba_sse2, crc32_copy_chorba_sse2, test_cpu_features.x86.has_sse2)
#    endif
#    ifdef X86_SSE41
    TEST_CRC32_COPY(chorba_sse41, crc32_copy_chorba_sse41, test_cpu_features.x86.has_sse41)
#    endif
#  endif
#  ifdef ARM_CRC32
    TEST_CRC32_COPY(armv8, crc32_copy_armv8, test_cpu_features.arm.has_crc32)
#  endif
#  ifdef ARM_PMULL_EOR3
    TEST_CRC32_COPY(armv8_pmull_eor3, crc32_copy_armv8_pmull_eor3, test_cpu_features.arm.has_crc32 && test_cpu_features.arm.has_pmull && test_cpu_features.arm.has_eor3)
#  endif
#  ifdef LOONGARCH_CRC
    TEST_CRC32_COPY(loongarch, crc32_copy_loongarch64, test_cpu_features.loongarch.has_crc)
#  endif
#  ifdef RISCV_CRC32_ZBC
    TEST_CRC32_COPY(riscv, crc32_copy_riscv64_zbc, test_cpu_features.riscv.has_zbc)
#  endif
#  ifdef X86_PCLMULQDQ_CRC
    TEST_CRC32_COPY(pclmulqdq, crc32_copy_pclmulqdq, test_cpu_features.x86.has_pclmulqdq)
#  endif
#  ifdef X86_VPCLMULQDQ_CRC
    TEST_CRC32_COPY(vpclmulqdq, crc32_copy_vpclmulqdq, (test_cpu_features.x86.has_pclmulqdq && test_cpu_features.x86.has_avx512_common && test_cpu_features.x86.has_vpclmulqdq))
#  endif

#endif
