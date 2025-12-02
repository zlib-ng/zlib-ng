/* test_crc32_fold_copy.cc -- test for crc32 implementations doing folded copying
 * Copyright (C) 2025 Hans Kristian Rosbach
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#include <gtest/gtest.h>

extern "C" {
#  include "zbuild.h"
#  include "arch_functions.h"
#  include "test_cpu_features.h"
#  include "crc32_test_strings_p.h"
}

#define BUFSIZE 615336U

class crc32_fc_variant : public ::testing::TestWithParam<crc32_test> {
protected:
    uint8_t dstbuf[BUFSIZE];

public:
    /* Ensure that crc32 fold copy functions returns the correct crc and copies the data */
    void crc32_fold_test(size_t minlen, int onlyzero, crc32_fold_reset_func fold_reset, crc32_fold_copy_func fold_copy,
                         crc32_fold_final_func fold_final, crc32_test params) {
        ALIGNED_(16) crc32_fold crc_st;
        uint32_t crc;

        ASSERT_LE(params.len, BUFSIZE);

        // Some optimized functions cannot take a crc value as start point
        // and some have minimum length requirements
        if (params.buf == NULL || params.len < minlen || (onlyzero && params.crc != 0)) {
            GTEST_SKIP();
        }

        fold_reset(&crc_st);
        crc_st.value = params.crc;

        fold_copy(&crc_st, dstbuf, params.buf, params.len);
        crc = fold_final(&crc_st);

        EXPECT_EQ(crc, params.expect);
        EXPECT_EQ(0, memcmp(params.buf, dstbuf, params.len));
    }
};

INSTANTIATE_TEST_SUITE_P(crc32_fc, crc32_fc_variant, testing::ValuesIn(crc32_tests));

#define TEST_CRC32_FOLD(name, minlen, onlyzero, resfunc, copyfunc, finfunc, support_flag) \
    TEST_P(crc32_fc_variant, name) { \
        if (!support_flag) { \
            GTEST_SKIP(); \
            return; \
        } \
        crc32_fold_test(minlen, onlyzero, resfunc, copyfunc, finfunc, GetParam()); \
    }

// Generic test
TEST_CRC32_FOLD(generic, 0, 0, crc32_fold_reset_c, crc32_fold_copy_c, crc32_fold_final_c, 1)

#ifdef DISABLE_RUNTIME_CPU_DETECTION
    // Native test
    TEST_CRC32_FOLD(native, 16, 1, native_crc32_fold_reset, native_crc32_fold_copy, native_crc32_fold_final, 1)
#else

    // Tests of optimized functions
#  ifdef ARM_CRC32
    TEST_CRC32_FOLD(armv8, 0, 0, crc32_fold_reset_c, crc32_fold_copy_armv8, crc32_fold_final_c, test_cpu_features.arm.has_crc32)
#  endif
#  ifdef X86_PCLMULQDQ_CRC
    // Is 16 bytes len the minimum for pclmul functions?
    TEST_CRC32_FOLD(pclmulqdq, 16, 1, crc32_fold_pclmulqdq_reset, crc32_fold_pclmulqdq_copy, crc32_fold_pclmulqdq_final, test_cpu_features.x86.has_pclmulqdq)
#  endif
#  ifdef X86_VPCLMULQDQ_CRC
    TEST_CRC32_FOLD(vpclmulqdq, 16, 1, crc32_fold_pclmulqdq_reset, crc32_fold_vpclmulqdq_copy, crc32_fold_pclmulqdq_final, (test_cpu_features.x86.has_pclmulqdq && test_cpu_features.x86.has_avx512_common && test_cpu_features.x86.has_vpclmulqdq))
#  endif
#  ifdef LOONGARCH_CRC
    TEST_CRC32_FOLD(loongarch64, 0, 0, crc32_fold_reset_c, crc32_fold_copy_loongarch64, crc32_fold_final_c, test_cpu_features.loongarch.has_crc)
#  endif

#endif
