/*
 * LASX optimized hash slide, based on Intel AVX2 implementation
 *
 * Copyright (C) 2017 Intel Corporation
 * Copyright (C) 2025 Vladislav Shchapov <vladislav@shchapov.ru>
 * Authors:
 *   Arjan van de Ven   <arjan@linux.intel.com>
 *   Jim Kukunas        <james.t.kukunas@linux.intel.com>
 *   Mika T. Lindqvist  <postmaster@raasu.org>
 *
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#ifdef LOONGARCH_LASX

#include "zbuild.h"
#include "deflate.h"

#include <lasxintrin.h>

static inline void slide_hash_chain(Pos *table, uint32_t entries, const __m256i wsize) {
    table += entries;
    table -= 32;

    do {
        __m256i value1, value2, result1, result2;

        value1 = __lasx_xvld(table, 0);
        value2 = __lasx_xvld(table, 32);
        result1 = __lasx_xvssub_hu(value1, wsize);
        result2 = __lasx_xvssub_hu(value2, wsize);
        __lasx_xvst(result1, table, 0);
        __lasx_xvst(result2, table, 32);

        table -= 32;
        entries -= 32;
    } while (entries > 0);
}

Z_INTERNAL void slide_hash_lasx(deflate_state *s) {
    Assert(s->slide_len <= UINT16_MAX, "slide_len should fit in uint16_t");
    uint16_t slide = (uint16_t)s->slide_len;
    const __m256i ymm_slide = __lasx_xvreplgr2vr_h((short)slide);

    slide_hash_chain(s->head, HASH_SIZE, ymm_slide);
    slide_hash_chain(s->prev, s->w_size, ymm_slide);
}

Z_INTERNAL void slide_hash_head_lasx(deflate_state *s) {
    Assert(s->slide_len <= UINT16_MAX, "slide_len should fit in uint16_t");
    uint16_t slide = (uint16_t)s->slide_len;
    const __m256i ymm_slide = __lasx_xvreplgr2vr_h((short)slide);

    slide_hash_chain(s->head, HASH_SIZE, ymm_slide);
}

#endif
