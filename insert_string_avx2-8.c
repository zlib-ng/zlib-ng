#include "zbuild.h"
#include "deflate.h"
#include <immintrin.h>
#include <stdio.h>

void insert_string(deflate_state *const s, uint32_t str, uint32_t count) {
    uint8_t *strstart = s->window + str;                // Start of string
    uint8_t *strend = strstart + count;                 // End of string
    Pos *headp = s->head;                               // Local variabes to avoid indirection
    Pos *prevp = s->prev;                               //  -||-
    uint32_t w_mask = s->w_mask;                        //  -||-
    Pos idx = (Pos)str;                                 // Starting index

    // Use vectorized loop if enough input length
    if (count >= 8) {
        __m256i hash_mask_vec = _mm256_set1_epi32(HASH_MASK);  // mask as vector
        __m256i indices = _mm256_set_epi32(7,6,5,4,3, 2, 1, 0);        // gather indexes
        const __m256i permVec = _mm256_setr_epi8(0, 1, 2, 3,   // load order
                                              1, 2, 3, 4,
                                              2, 3, 4, 5,
                                              3, 4, 5, 6,
                                              4, 5, 6, 7,
                                              5, 6, 7, 8,
                                              6, 7, 8, 9,
                                              7, 8, 9, 10);

        // Main vectorized loop
        for ( ; strstart+8 < strend; idx+=8, strstart+=8) {
            // Load data
             __m256i val_vec = _mm256_i32gather_epi32((const int*)strstart, indices, 1);

            // Hash calculation
            __m256i h_vec = _mm256_mullo_epi32(val_vec, _mm256_set1_epi32(2654435761U));
            h_vec = _mm256_srli_epi32(h_vec, 16);
            h_vec = _mm256_and_si256(h_vec, hash_mask_vec);

            int32_t h_array[8];
            _mm256_storeu_si256((__m256i*)h_array, h_vec);

            for (int i = 0; i < 8; i++) {
                uint32_t h = h_array[i];
                Pos idx0 = idx + i;
                Pos head = headp[h];
                if (head != idx0) {
                    prevp[idx0 & w_mask] = head;
                    headp[h] = idx0;
                }
            }
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
