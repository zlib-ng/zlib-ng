/* compare256_lsx.c -- LSX version of compare256, based on Intel SSE implementation
 * Copyright Adam Stylinski <kungfujesus06@gmail.com>
 * Copyright (C) 2025 Vladislav Shchapov <vladislav@shchapov.ru>
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#include "zbuild.h"
#include "zendian.h"
#include "zmemory.h"
#include "deflate.h"
#include "fallback_builtins.h"

#ifdef LOONGARCH_LSX

#include <lsxintrin.h>
#include "lsxintrin_ext.h"

static inline uint32_t compare256_lsx_static(const uint8_t *src0, const uint8_t *src1) {
    __m128i xmm_src0, xmm_src1, xmm_cmp;

    /* Do the first load unaligned, than all subsequent ones we have at least
     * one aligned load. Sadly aligning both loads is probably unrealistic */
    xmm_src0 = __lsx_vld(src0, 0);
    xmm_src1 = __lsx_vld(src1, 0);
    xmm_cmp = __lsx_vseq_b(xmm_src0, xmm_src1);

    unsigned mask = (unsigned)lsx_movemask_b(xmm_cmp);

    /* Compiler _may_ turn this branch into a ptest + movemask,
     * since a lot of those uops are shared and fused */
    if (mask != 0xFFFF)
        return zng_ctz32(~mask);

    const uint8_t *last0 = src0 + 240;
    const uint8_t *last1 = src1 + 240;

    int align_offset = ((uintptr_t)src0) & 15;
    int align_adv = 16 - align_offset;
    uint32_t len = align_adv;

    src0 += align_adv;
    src1 += align_adv;

    for (int i = 0; i < 15; i++) {
        xmm_src0 = __lsx_vld(src0, 0);
        xmm_src1 = __lsx_vld(src1, 0);
        xmm_cmp = __lsx_vseq_b(xmm_src0, xmm_src1);

        mask = (unsigned)lsx_movemask_b(xmm_cmp);

        /* Compiler _may_ turn this branch into a ptest + movemask,
         * since a lot of those uops are shared and fused */
        if (mask != 0xFFFF)
            return len + zng_ctz32(~mask);

        len += 16, src0 += 16, src1 += 16;
    }

    if (align_offset) {
        xmm_src0 = __lsx_vld(last0, 0);
        xmm_src1 = __lsx_vld(last1, 0);
        xmm_cmp = __lsx_vseq_b(xmm_src0, xmm_src1);

        mask = (unsigned)lsx_movemask_b(xmm_cmp);

        if (mask != 0xFFFF)
            return 240 + zng_ctz32(~mask);
    }

    return 256;
}

Z_INTERNAL uint32_t compare256_lsx(const uint8_t *src0, const uint8_t *src1) {
    return compare256_lsx_static(src0, src1);
}

#define LONGEST_MATCH       longest_match_lsx
#define COMPARE256          compare256_lsx_static

#include "match_tpl.h"

#define LONGEST_MATCH_SLOW
#define LONGEST_MATCH       longest_match_slow_lsx
#define COMPARE256          compare256_lsx_static

#include "match_tpl.h"

#endif
