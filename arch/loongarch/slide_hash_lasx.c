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
#include "zbuild.h"
#include "deflate.h"

#include <lasxintrin.h>

static inline void slide_hash_chain(Pos *table, uint32_t entries, const __m256i wsize) {
    table += entries;
    table -= 16;

    do {
        __m256i value, result;

        value = __lasx_xvld(table, 0);
        result = __lasx_xvssub_hu(value, wsize);
        __lasx_xvst(result, table, 0);

        table -= 16;
        entries -= 16;
    } while (entries > 0);
}

Z_INTERNAL void slide_hash_lasx(deflate_state *s) {
    Assert(s->w_size <= UINT16_MAX, "w_size should fit in uint16_t");
    uint16_t wsize = (uint16_t)s->w_size;
    const __m256i ymm_wsize = __lasx_xvreplgr2vr_h((short)wsize);

    slide_hash_chain(s->head, HASH_SIZE, ymm_wsize);
    slide_hash_chain(s->prev, wsize, ymm_wsize);
}
