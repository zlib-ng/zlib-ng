/* slide_hash_avx512.c - AVX512 optimized hash slide for deflate based on AVX2 version
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#ifdef X86_AVX512

#include "zbuild.h"
#include "deflate.h"

#include <immintrin.h>

static inline void slide_hash_chain(Pos *table, uint32_t entries, const __m512i wsize) {
    table += entries;
    table -= 64;

    do {
        __m512i value1, value2, result1, result2;

        value1 = _mm512_load_si512((__m512i *)table);
        value2 = _mm512_load_si512((__m512i *)(table+32));
        result1 = _mm512_subs_epu16(value1, wsize);
        result2 = _mm512_subs_epu16(value2, wsize);
        _mm512_store_si512((__m512i *)table, result1);
        _mm512_store_si512((__m512i *)(table+32), result2);

        table -= 64;
        entries -= 64;
    } while (entries > 0);
}

Z_INTERNAL void slide_hash_avx512(deflate_state *s) {
    Assert(s->w_size <= UINT16_MAX, "w_size should fit in uint16_t");
    uint16_t wsize = (uint16_t)s->w_size;
    const __m512i zmm_wsize = _mm512_set1_epi16((short)wsize);

    slide_hash_chain(s->head, HASH_SIZE, zmm_wsize);
    slide_hash_chain(s->prev, wsize, zmm_wsize);
}
#endif
