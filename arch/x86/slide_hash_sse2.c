/*
 * SSE optimized hash slide
 *
 * Copyright (C) 2017 Intel Corporation
 * Authors:
 *   Arjan van de Ven   <arjan@linux.intel.com>
 *   Jim Kukunas        <james.t.kukunas@linux.intel.com>
 *
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#ifdef X86_SSE2

#include "zbuild.h"
#include "deflate.h"

#include <immintrin.h>

static inline void slide_hash_chain(Pos *table, uint32_t entries, const __m128i wsize) {
    table += entries;
    table -= 16;

    /* alloc_deflate() ensures this pointer is aligned on an 64 byte boundary */
    do {
        __m128i value1, value2, result1, result2;

        value1 = _mm_load_si128((__m128i *)table);
        value2 = _mm_load_si128((__m128i *)(table + 8));
        result1 = _mm_subs_epu16(value1, wsize);
        result2 = _mm_subs_epu16(value2, wsize);
        _mm_store_si128((__m128i *)table, result1);
        _mm_store_si128((__m128i *)(table + 8), result2);

        table -= 16;
        entries -= 16;
    } while (entries > 0);
}

Z_INTERNAL void slide_hash_sse2(deflate_state *s) {
    Assert(s->w_size <= UINT16_MAX, "w_size should fit in uint16_t");
    uint16_t wsize = (uint16_t)s->w_size;
    const __m128i xmm_wsize = _mm_set1_epi16((short)wsize);

    slide_hash_chain(s->head, HASH_SIZE, xmm_wsize);
    slide_hash_chain(s->prev, wsize, xmm_wsize);
}

Z_INTERNAL void slide_hash_head_sse2(deflate_state *s) {
    Assert(s->w_size <= UINT16_MAX, "w_size should fit in uint16_t");
    uint16_t wsize = (uint16_t)s->w_size;
    const __m128i xmm_wsize = _mm_set1_epi16((short)wsize);

    slide_hash_chain(s->head, HASH_SIZE, xmm_wsize);
}

#endif
