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

    printf("%d\n", count);

    // Use vectorized loop if enough input length
    if (count >= 4) {
        __m128i hash_mask_vec = _mm_set1_epi32(HASH_MASK);  // mask as vector
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
            Pos idx1 = idx + 1;
            Pos idx2 = idx + 2;
            Pos idx3 = idx + 3;

            // Hash calculation
            __m128i h_vec = _mm_mullo_epi32(val_vec, _mm_set1_epi32(2654435761U));
            h_vec = _mm_srli_epi32(h_vec, 16);
            h_vec = _mm_and_si128(h_vec, hash_mask_vec);

            // Extract the hashed values
            uint32_t h0 = _mm_extract_epi32(h_vec, 0);
            uint32_t h1 = _mm_extract_epi32(h_vec, 1);
            uint32_t h2 = _mm_extract_epi32(h_vec, 2);
            uint32_t h3 = _mm_extract_epi32(h_vec, 3);

            // Insert into hash table
            Pos head0 = headp[h0];
            if (head0 != idx) {
                prevp[idx & w_mask] = head0;
                headp[h0] = idx;
            }
            Pos head1 = headp[h1];
            if (head1 != idx1) {
                prevp[idx1 & w_mask] = head1;
                headp[h1] = idx1;
            }
            Pos head2 = headp[h2];
            if (head2 != idx2) {
                prevp[idx2 & w_mask] = head2;
                headp[h2] = idx2;
            }
            Pos head3 = headp[h3];
            if (head3 != idx3) {
                prevp[idx3 & w_mask] = head3;
                headp[h3] = idx3;
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
