/* compare256_lasx.c -- LASX version of compare256, based on Intel AVX2 implementation
 * Copyright Mika T. Lindqvist  <postmaster@raasu.org>
 * Copyright (C) 2025 Vladislav Shchapov <vladislav@shchapov.ru>
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#include "zbuild.h"
#include "zendian.h"
#include "zmemory.h"
#include "deflate.h"
#include "fallback_builtins.h"

#ifdef LOONGARCH_LASX

#include <lasxintrin.h>
#include "lasxintrin_ext.h"

static inline uint32_t compare256_lasx_static(const uint8_t *src0, const uint8_t *src1) {
    uint32_t len = 0;

    do {
        __m256i ymm_src0, ymm_src1, ymm_cmp;
        ymm_src0 = __lasx_xvld(src0, 0);
        ymm_src1 = __lasx_xvld(src1, 0);
        ymm_cmp = __lasx_xvseq_b(ymm_src0, ymm_src1); /* non-identical bytes = 00, identical bytes = FF */
        unsigned mask = (unsigned)lasx_movemask_b(ymm_cmp);
        if (mask != 0xFFFFFFFF)
            return len + zng_ctz32(~mask); /* Invert bits so identical = 0 */

        src0 += 32, src1 += 32, len += 32;

        ymm_src0 = __lasx_xvld(src0, 0);
        ymm_src1 = __lasx_xvld(src1, 0);
        ymm_cmp = __lasx_xvseq_b(ymm_src0, ymm_src1);
        mask = (unsigned)lasx_movemask_b(ymm_cmp);
        if (mask != 0xFFFFFFFF)
            return len + zng_ctz32(~mask);

        src0 += 32, src1 += 32, len += 32;
    } while (len < 256);

    return 256;
}

Z_INTERNAL uint32_t compare256_lasx(const uint8_t *src0, const uint8_t *src1) {
    return compare256_lasx_static(src0, src1);
}

#define LONGEST_MATCH       longest_match_lasx
#define COMPARE256          compare256_lasx_static

#include "match_tpl.h"

#define LONGEST_MATCH_SLOW
#define LONGEST_MATCH       longest_match_slow_lasx
#define COMPARE256          compare256_lasx_static

#include "match_tpl.h"

#endif
