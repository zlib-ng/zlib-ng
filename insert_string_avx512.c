#include "zbuild.h"
#include "deflate.h"
#include <immintrin.h>

void insert_string(deflate_state *const s, uint32_t str, uint32_t count) {
    uint8_t *strstart = s->window + str;                // Start of string
    uint8_t *strend = strstart + count;                 // End of string
    Pos *headp = s->head;                               // Local variabes to avoid indirection
    Pos *prevp = s->prev;                               //  -||-
    uint32_t w_mask = s->w_mask;                        //  -||-
    Pos idx = (Pos)str;                                 // Starting index

    // Use vectorized loop if enough input length
    if (count >= 4) {
        __m128i hash_mask_vec = _mm_set1_epi32(HASH_MASK);  // mask as vector
        __m128i w_mask_vec = _mm_set1_epi32(s->w_mask);     // w_mask as vector
        __m128i indices = _mm_set_epi32(3, 2, 1, 0);        // gather indexes
        const __m128i permVec = _mm_setr_epi8(0, 1, 2, 3,   // load order
                                              1, 2, 3, 4,
                                              2, 3, 4, 5,
                                              3, 4, 5, 6);

        // Main vectorized loop
        for ( ; strstart+4 < strend; idx+=4, strstart+=4) {
            // Load data
            __m128i val_vec = _mm_loadl_epi64((__m128i *)strstart);
            val_vec = _mm_shuffle_epi8(val_vec, permVec);

            // Prepare idx
            __m128i idx_vec = _mm_add_epi32(_mm_set1_epi32(idx), indices);

            // prev_indices = idx_vec & w_mask
            __m128i prev_indices = _mm_and_si128(idx_vec, w_mask_vec);

            // Hash calculation
            __m128i h_vec = _mm_mullo_epi32(val_vec, _mm_set1_epi32(2654435761U));
            h_vec = _mm_srli_epi32(h_vec, 16);
            h_vec = _mm_and_si128(h_vec, hash_mask_vec);

            // head_vec = headp[h_vec]
            __m128i head_vec = _mm_i32gather_epi32((const int*)headp, h_vec, sizeof(Pos));

            // Compute mask where head != idx
            __mmask8 mask = _mm_cmpeq_epi32_mask(head_vec, idx_vec);
            mask = ~mask & 0xF;

            // Scatter headp[h] = idx
            _mm_mask_i32scatter_epi32((int*)headp, mask, h_vec, idx_vec, sizeof(Pos));

            // Scatter prevp[idx & w_mask] = headp[h]
            _mm_mask_i32scatter_epi32((int*)prevp, mask, prev_indices, head_vec, sizeof(Pos));
        }
    }

    // Handle remaining elements as scalar
    for ( ; strstart < strend; idx++, strstart++) {
        uint32_t val, h;

        val = zng_memread_4(strstart);
        h = ((val * 2654435761U) >> 16);
        h &= HASH_MASK;

        Pos head = headp[h];
        if (head != idx) {
            prevp[idx & w_mask] = head;
            headp[h] = idx;
        }
    }
}
