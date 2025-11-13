/* test_crc32.cc -- crc32 unit test
 * Copyright (C) 2019-2021 IBM Corporation
 * Authors: Rogerio Alves    <rogealve@br.ibm.com>
 *          Matheus Castanho <msc@linux.ibm.com>
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#include <gtest/gtest.h>

extern "C" {
#  include "zutil_p.h"
#  include "zbuild.h"
#  include "arch_functions.h"
#  include "test_cpu_features.h"
#  include "crc32_test_strings_p.h"
}

class crc32_variant : public ::testing::TestWithParam<crc32_test> {
public:
    void hash(crc32_test param, crc32_func crc32) {
        uint32_t crc = 0;
        if (param.buf != NULL) {
            if (param.len) {
                crc = crc32(param.crc, param.buf, param.len);
            } else {
                crc = param.crc;
            }
        }
        EXPECT_EQ(crc, param.expect);
    }
};

/* Specifically to test where we had dodgy alignment in the ARMv8 CRC32
 * function. All others are either byte level access or use intrinsics
 * that work with unaligned access */
class crc32_align : public ::testing::TestWithParam<int> {
public:
    void hash(int param, crc32_func crc32) {
        uint8_t *buf = (uint8_t*)zng_alloc(sizeof(uint8_t) * (128 + param));
        if (buf != NULL) {
            (void)crc32(0, buf + param, 128);
        } else {
            FAIL();
        }
        zng_free(buf);
    }
};

/* Test large 1MB buffer with known CRC32 */
class crc32_large_buf : public ::testing::Test {
protected:
    static uint8_t *buffer;
    static const size_t buffer_size = 1024 * 1024;

    static void SetUpTestSuite() {
        buffer = (uint8_t*)zng_alloc(buffer_size);
        memset(buffer, 0x55, buffer_size);
    }

    static void TearDownTestSuite() {
        zng_free(buffer);
    }

public:
    void hash(crc32_func crc32) {
        EXPECT_EQ(crc32(0, buffer, buffer_size), 0x0026D5FB);
    }
};

uint8_t *crc32_large_buf::buffer = nullptr;

INSTANTIATE_TEST_SUITE_P(crc32, crc32_variant, testing::ValuesIn(crc32_tests));

#define TEST_CRC32(name, func, support_flag) \
    TEST_P(crc32_variant, name) { \
        if (!(support_flag)) { \
            GTEST_SKIP(); \
            return; \
        } \
        hash(GetParam(), func); \
    } \
    TEST_F(crc32_large_buf, name) { \
        if (!(support_flag)) { \
            GTEST_SKIP(); \
            return; \
        } \
        hash(func); \
    }

TEST_CRC32(braid, crc32_braid, 1)

#ifdef DISABLE_RUNTIME_CPU_DETECTION
TEST_CRC32(native, native_crc32, 1)

#else

#if defined(ARM_CRC32) || defined(LOONGARCH_CRC)
static const int align_offsets[] = {
    1, 2, 3, 4, 5, 6, 7
};

#define TEST_CRC32_ALIGN(name, func, support_flag) \
    TEST_P(crc32_align, name) { \
        if (!(support_flag)) { \
            GTEST_SKIP(); \
            return; \
        } \
        hash(GetParam(), func); \
    }
#endif

#ifndef WITHOUT_CHORBA
TEST_CRC32(chorba_c, crc32_chorba, 1)
#endif
#ifdef ARM_CRC32
INSTANTIATE_TEST_SUITE_P(crc32_alignment, crc32_align, testing::ValuesIn(align_offsets));
TEST_CRC32(armv8, crc32_armv8, test_cpu_features.arm.has_crc32)
TEST_CRC32_ALIGN(armv8_align, crc32_armv8, test_cpu_features.arm.has_crc32)
#endif
#ifdef RISCV_CRC32_ZBC
TEST_CRC32(riscv, crc32_riscv64_zbc, test_cpu_features.riscv.has_zbc)
#endif
#ifdef POWER8_VSX_CRC32
TEST_CRC32(power8, crc32_power8, test_cpu_features.power.has_arch_2_07)
#endif
#ifdef S390_CRC32_VX
TEST_CRC32(vx, crc32_s390_vx, test_cpu_features.s390.has_vx)
#endif
#ifdef X86_PCLMULQDQ_CRC
TEST_CRC32(pclmulqdq, crc32_pclmulqdq, test_cpu_features.x86.has_pclmulqdq)
#endif
#ifdef X86_VPCLMULQDQ_CRC
TEST_CRC32(vpclmulqdq, crc32_vpclmulqdq, (test_cpu_features.x86.has_pclmulqdq && test_cpu_features.x86.has_avx512_common && test_cpu_features.x86.has_vpclmulqdq))
#endif
#ifndef WITHOUT_CHORBA_SSE
#   ifdef X86_SSE2
    TEST_CRC32(chorba_sse2, crc32_chorba_sse2, test_cpu_features.x86.has_sse2)
#   endif
#   ifdef X86_SSE41
    TEST_CRC32(chorba_sse41, crc32_chorba_sse41, test_cpu_features.x86.has_sse41)
#   endif
#endif
#if defined(LOONGARCH_CRC)
INSTANTIATE_TEST_SUITE_P(crc32_alignment, crc32_align, testing::ValuesIn(align_offsets));
TEST_CRC32(loongarch64, crc32_loongarch64, test_cpu_features.loongarch.has_crc)
TEST_CRC32_ALIGN(loongarch64_align, crc32_loongarch64, test_cpu_features.loongarch.has_crc)
#endif

#endif
