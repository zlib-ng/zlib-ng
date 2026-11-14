/* test_slide_hash.cc - slide_hash unit tests
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#include <stdint.h>
#include <string.h>

#include <gtest/gtest.h>

extern "C" {
#  include "zbuild.h"
#  include "zutil_p.h"
#  include "deflate.h"
#  include "arch_functions.h"
#  include "test_cpu_features.h"
}

#define MAX_TEST_WSIZE 32768u

static inline Pos slide_ref(Pos val, Pos slide) {
    return (val >= slide) ? val - slide : 0;
}

static void slide_hash_run(slide_hash_func func, uint32_t wsize, Pos slide, uint32_t offset, bool head_only) {
    Pos *head_buf = (Pos *)zng_alloc_aligned((HASH_SIZE + 8u) * sizeof(Pos), 64);
    Pos *prev_buf = (Pos *)zng_alloc_aligned((MAX_TEST_WSIZE + 8u) * sizeof(Pos), 64);
    ASSERT_NE(head_buf, nullptr);
    ASSERT_NE(prev_buf, nullptr);

    Pos *head = head_buf + offset;
    Pos *prev = prev_buf + offset;
    for (uint32_t i = 0; i < HASH_SIZE; i++)
        head[i] = (Pos)(i * 40503u);
    for (uint32_t i = 0; i < wsize; i++)
        prev[i] = head_only ? (Pos)0xA55Au : (Pos)((i * 25173u) ^ 0x9E37u);

    deflate_state s;
    memset(&s, 0, sizeof(s));
    s.head = head;
    s.prev = prev;
    s.w_size = wsize;
    s.slide_len = slide;
    func(&s);

    for (uint32_t i = 0; i < HASH_SIZE; i++)
        ASSERT_EQ(head[i], slide_ref((Pos)(i * 40503u), slide));
    for (uint32_t i = 0; i < wsize; i++) {
        Pos expect = head_only ? (Pos)0xA55Au : slide_ref((Pos)((i * 25173u) ^ 0x9E37u), slide);
        ASSERT_EQ(prev[i], expect);
    }

    zng_free_aligned(head_buf);
    zng_free_aligned(prev_buf);
}

static void slide_hash_check(slide_hash_func func, slide_hash_func head_func) {
    static const uint32_t wsizes[] = {512u, 4096u, MAX_TEST_WSIZE};
    for (size_t k = 0; k < sizeof(wsizes) / sizeof(wsizes[0]); k++) {
        slide_hash_run(func, wsizes[k], (Pos)wsizes[k], 0, false);
        if (head_func != nullptr)
            slide_hash_run(head_func, wsizes[k], (Pos)wsizes[k], 0, true);
    }
}

#define TEST_SLIDE_HASH(name, func, head_func, support_flag) \
    TEST(slide_hash, name) { \
        if (!(support_flag)) { \
            GTEST_SKIP(); \
            Z_UNREACHABLE(); \
            return; \
        } \
        slide_hash_check(func, head_func); \
    }

#if defined(SLIDE_HASH_FALLBACK) || defined(SLIDE_HASH_SWAR_FALLBACK)
static void slide_hash_edge_cases(slide_hash_func func, slide_hash_func head_func) {
    static const Pos slides[] = {0, 1, 511, 28672, 32767, 32768, 32769, 49152, 65535};
    for (size_t k = 0; k < sizeof(slides) / sizeof(slides[0]); k++) {
        slide_hash_run(func, 71u, slides[k], 0, false);
        slide_hash_run(head_func, 71u, slides[k], 0, true);
    }
    for (uint32_t offset = 1; offset < 4; offset++) {
        slide_hash_run(func, 65u, 511, offset, false);
        slide_hash_run(func, 65u, 32768, offset, false);
        slide_hash_run(func, 65u, 49152, offset, false);
    }
    slide_hash_run(func, 0u, 32768, 0, false);
}
#endif

#ifdef SLIDE_HASH_FALLBACK
TEST_SLIDE_HASH(c, slide_hash_c, slide_hash_head_c, 1)

TEST(slide_hash, c_edge_cases) {
    slide_hash_edge_cases(slide_hash_c, slide_hash_head_c);
}
#endif
#ifdef SLIDE_HASH_SWAR_FALLBACK
TEST_SLIDE_HASH(swar, slide_hash_swar, slide_hash_head_swar, 1)

TEST(slide_hash, swar_edge_cases) {
    slide_hash_edge_cases(slide_hash_swar, slide_hash_head_swar);
}
#endif

#ifdef DISABLE_RUNTIME_CPU_DETECTION
#  ifdef native_slide_hash_head
TEST_SLIDE_HASH(native, native_slide_hash, native_slide_hash_head, 1)
#  else
TEST_SLIDE_HASH(native, native_slide_hash, nullptr, 1)
#  endif
#else

#ifdef ARM_SIMD
TEST_SLIDE_HASH(armv6, slide_hash_armv6, slide_hash_head_armv6, test_cpu_features.arm.has_simd)
#endif
#ifdef ARM_NEON
TEST_SLIDE_HASH(neon, slide_hash_neon, slide_hash_head_neon, test_cpu_features.arm.has_neon)
#endif
#ifdef MIPS_MSA
TEST_SLIDE_HASH(msa, slide_hash_msa, nullptr, test_cpu_features.mips.has_msa)
#endif
#ifdef POWER8_VSX
TEST_SLIDE_HASH(power8, slide_hash_power8, slide_hash_head_power8, test_cpu_features.power.has_arch_2_07)
#endif
#ifdef PPC_VMX
TEST_SLIDE_HASH(vmx, slide_hash_vmx, slide_hash_head_vmx, test_cpu_features.power.has_altivec)
#endif
#ifdef S390_VX
TEST_SLIDE_HASH(vx, slide_hash_vx, slide_hash_head_vx, test_cpu_features.s390.has_vx)
#endif
#ifdef RISCV_RVV
TEST_SLIDE_HASH(rvv, slide_hash_rvv, slide_hash_head_rvv, test_cpu_features.riscv.has_rvv)
#endif
#ifdef X86_SSE2
TEST_SLIDE_HASH(sse2, slide_hash_sse2, slide_hash_head_sse2, test_cpu_features.x86.has_sse2)
#endif
#ifdef X86_AVX2
TEST_SLIDE_HASH(avx2, slide_hash_avx2, slide_hash_head_avx2, test_cpu_features.x86.has_avx2)
#endif
#ifdef LOONGARCH_LSX
TEST_SLIDE_HASH(lsx, slide_hash_lsx, slide_hash_head_lsx, test_cpu_features.loongarch.has_lsx)
#endif
#ifdef LOONGARCH_LASX
TEST_SLIDE_HASH(lasx, slide_hash_lasx, slide_hash_head_lasx, test_cpu_features.loongarch.has_lasx)
#endif

#endif
