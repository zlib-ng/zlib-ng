/* slide_hash.c -- slide hash table C implementation
 *
 * Copyright (C) 1995-2024 Jean-loup Gailly and Mark Adler
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#include "zbuild.h"
#include "arch_functions.h"

#ifdef SLIDE_HASH_FALLBACK

#include "deflate.h"

#define SLIDE_SWAR_MSB 0x8000800080008000ULL
#define SLIDE_SWAR_REP 0x0001000100010001ULL

Z_FORCEINLINE static uint64_t slide_load64(Pos *p) {
    uint64_t v;
    memcpy(&v, HINT_ALIGNED(p, 8), sizeof(v));
    return v;
}

Z_FORCEINLINE static void slide_store64(Pos *p, uint64_t v) {
    memcpy(HINT_ALIGNED(p, 8), &v, sizeof(v));
}

/* Subtract 32768 with unsigned saturation across four 16-bit lanes.
 * Entries below 32768 become 0 and entries at or above 32768 clear bit 15. */
Z_FORCEINLINE static uint64_t slide_swar_32k(uint64_t v) {
    uint64_t m = v & SLIDE_SWAR_MSB;
    return v & (m - (m >> 15));
}

/* Subtract wsize < 32768 with unsigned saturation across four 16-bit lanes.
 * Setting bit 15 before subtracting prevents borrows between adjacent lanes
 * and leaves bit 15 of t set iff the low 15 bits of the lane are >= wsize. */
Z_FORCEINLINE static uint64_t slide_swar_lt32k(uint64_t v, uint64_t sw) {
    uint64_t t = (v | SLIDE_SWAR_MSB) - sw;
    uint64_t ge = (v | t) & SLIDE_SWAR_MSB;
    return t & ((ge - (ge >> 15)) | (v & SLIDE_SWAR_MSB));
}

/* Subtract wsize > 32768 with unsigned saturation across four 16-bit lanes.
 * Surviving entries have bit 15 set in both v and t and produce results < 32768. */
Z_FORCEINLINE static uint64_t slide_swar_gt32k(uint64_t v, uint64_t sw_lo) {
    uint64_t t = (v | SLIDE_SWAR_MSB) - sw_lo;
    uint64_t ge = v & t & SLIDE_SWAR_MSB;
    return t & (ge - (ge >> 15));
}

/* ===========================================================================
 * Slide the hash table when sliding the window down
 */
static inline void slide_hash_c_chain(Pos *table, uint32_t entries, Pos wsize) {
    if (((uintptr_t)table & 7) == 0) {
        Pos *q = (Pos *)HINT_ALIGNED(table, 8);
        if (wsize == 32768) {
            while (entries >= 8) {
                uint64_t v0 = slide_load64(q);
                uint64_t v1 = slide_load64(q + 4);
                slide_store64(q, slide_swar_32k(v0));
                slide_store64(q + 4, slide_swar_32k(v1));
                q += 8;
                entries -= 8;
            }
        } else if (wsize < 32768) {
            uint64_t sw = (uint64_t)wsize * SLIDE_SWAR_REP;
            while (entries >= 8) {
                uint64_t v0 = slide_load64(q);
                uint64_t v1 = slide_load64(q + 4);
                slide_store64(q, slide_swar_lt32k(v0, sw));
                slide_store64(q + 4, slide_swar_lt32k(v1, sw));
                q += 8;
                entries -= 8;
            }
        } else {
            uint64_t sw_lo = ((uint64_t)(wsize & 0x7FFFU)) * SLIDE_SWAR_REP;
            while (entries >= 8) {
                uint64_t v0 = slide_load64(q);
                uint64_t v1 = slide_load64(q + 4);
                slide_store64(q, slide_swar_gt32k(v0, sw_lo));
                slide_store64(q + 4, slide_swar_gt32k(v1, sw_lo));
                q += 8;
                entries -= 8;
            }
        }
        table = q;
    }

    Pos *q = table;
    for (uint32_t i = 0; i < entries; i++) {
        Pos m = *q;
        *q++ = (m >= wsize) ? m - wsize : 0;
    }
    /* If entries is not on any hash chain, prev[entries] is garbage but
     * its value will never be used.
     */
}

Z_INTERNAL void slide_hash_c(deflate_state *s) {
    Assert(s->slide_len <= UINT16_MAX, "slide_len should fit in uint16_t");
    Pos slide = (Pos)s->slide_len;

    slide_hash_c_chain(s->head, HASH_SIZE, slide);
    slide_hash_c_chain(s->prev, s->w_size, slide);
}

Z_INTERNAL void slide_hash_head_c(deflate_state *s) {
    Assert(s->slide_len <= UINT16_MAX, "slide_len should fit in uint16_t");
    Pos slide = (Pos)s->slide_len;

    slide_hash_c_chain(s->head, HASH_SIZE, slide);
}

#endif /* SLIDE_HASH_FALLBACK */
