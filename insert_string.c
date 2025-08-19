/* insert_string.c -- insert_string integer hash variant
 *
 * Copyright (C) 1995-2024 Jean-loup Gailly and Mark Adler
 * For conditions of distribution and use, see copyright notice in zlib.h
 *
 */

#include "zbuild.h"
#include "deflate.h"

#define HASH_SLIDE           16

#define HASH_CALC(h, val)    h = ((val * 2654435761U) >> HASH_SLIDE);
#define HASH_CALC_VAR        h
#define HASH_CALC_VAR_INIT   uint32_t h = 0

#define UPDATE_HASH          update_hash
#define INSERT_STRING        insert_string2
#define QUICK_INSERT_STRING  quick_insert_string

#include "insert_string_tpl.h"

void insert_string(deflate_state *const s, uint32_t str, uint32_t count) {
    uint8_t *strstart = s->window + str;                // Start of string
    uint8_t *strend = strstart + count;                 // End of string
    Pos *headp = s->head;                               // Local variabes to avoid indirection
    Pos *prevp = s->prev;                               //  -||-
    uint32_t w_mask = s->w_mask;                        //  -||-
    Pos idx = (Pos)str;                                 // Starting index

    for ( ; strstart + 3 < strend; idx+=3, strstart+=3) {
        uint32_t val_0 = zng_memread_4(strstart);
        uint32_t val_1 = zng_memread_4(strstart + 1);
        uint32_t val_2 = zng_memread_4(strstart + 2);

        uint32_t h_0 = ((val_0 * 2654435761U) >> 16) & HASH_MASK;
        uint32_t h_1 = ((val_1 * 2654435761U) >> 16) & HASH_MASK;
        uint32_t h_2 = ((val_2 * 2654435761U) >> 16) & HASH_MASK;

        Pos idx_1 = idx + 1;
        Pos idx_2 = idx + 2;

        Pos head_0 = headp[h_0];
        if (head_0 != idx) {
            prevp[idx & w_mask] = head_0;
            headp[h_0] = idx;
        }

        Pos head_1 = headp[h_1];
        if (head_1 != idx_1) {
            prevp[idx_1 & w_mask] = head_1;
            headp[h_1] = idx_1;
        }

        Pos head_2 = headp[h_2];
        if (head_2 != idx_2) {
            prevp[idx_2 & w_mask] = head_2;
            headp[h_2] = idx_2;
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
