/*
 * AVX2 optimized hash slide, based on Intel's slide_sse implementation
 *
 * Copyright (C) 2017 Intel Corporation
 * Authors:
 *   Arjan van de Ven   <arjan@linux.intel.com>
 *   Jim Kukunas        <james.t.kukunas@linux.intel.com>
 *   Mika T. Lindqvist  <postmaster@raasu.org>
 *
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#ifdef X86_AVX2

#include "zbuild.h"
#include "deflate.h"

#include <immintrin.h>

static inline void slide_hash_chain(Pos *table, uint32_t entries, const __m256i wsize) {
    table += entries;
    table -= 32;

    do {
        __m256i value1, value2, result1, result2;

        value1 = _mm256_load_si256((__m256i *)table);
        value2 = _mm256_load_si256((__m256i *)(table+16));
        result1 = _mm256_subs_epu16(value1, wsize);
        result2 = _mm256_subs_epu16(value2, wsize);
        _mm256_store_si256((__m256i *)table, result1);
        _mm256_store_si256((__m256i *)(table+16), result2);

        table -= 32;
        entries -= 32;
    } while (entries > 0);
}

Z_INTERNAL void slide_hash_avx2(deflate_state *s) {
    Assert(s->slide_len <= UINT16_MAX, "slide_len should fit in uint16_t");
    uint16_t slide = (uint16_t)s->slide_len;
    const __m256i ymm_slide = _mm256_set1_epi16((short)slide);

    slide_hash_chain(s->head, HASH_SIZE, ymm_slide);
    slide_hash_chain(s->prev, s->w_size, ymm_slide);
}

Z_INTERNAL void slide_hash_head_avx2(deflate_state *s) {
    Assert(s->slide_len <= UINT16_MAX, "slide_len should fit in uint16_t");
    uint16_t slide = (uint16_t)s->slide_len;
    const __m256i ymm_slide = _mm256_set1_epi16((short)slide);

    slide_hash_chain(s->head, HASH_SIZE, ymm_slide);
}

#endif
