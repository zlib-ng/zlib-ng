/* slide_hash.c -- slide hash table C implementation
 *
 * Copyright (C) 1995-2024 Jean-loup Gailly and Mark Adler
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#include "zbuild.h"
#include "arch_functions.h"

#ifdef SLIDE_HASH_FALLBACK

#include "deflate.h"

/* WebAssembly has native 64-bit integer ops even on wasm32. */
#if defined(ARCH_64BIT) || defined(__wasm__)
#  define SLIDE_SWAR_MSB 0x8000800080008000ULL
#  define SLIDE_SWAR_REP 0x0001000100010001ULL
    typedef uint64_t slide_word_t;
#else
#  define SLIDE_SWAR_MSB 0x80008000U
#  define SLIDE_SWAR_REP 0x00010001U
    typedef uint32_t slide_word_t;
#endif
#define SLIDE_SWAR_LANES ((uint32_t)(sizeof(slide_word_t) / sizeof(Pos)))

Z_FORCEINLINE static slide_word_t slide_load(Pos *p) {
    slide_word_t v;
    memcpy(&v, HINT_ALIGNED(p, sizeof(slide_word_t)), sizeof(v));
    return v;
}

Z_FORCEINLINE static void slide_store(Pos *p, slide_word_t v) {
    memcpy(HINT_ALIGNED(p, sizeof(slide_word_t)), &v, sizeof(v));
}

/* Subtract 32768 with unsigned saturation across all 16-bit lanes of a word. */
Z_FORCEINLINE static slide_word_t slide_swar_32k(slide_word_t v) {
    slide_word_t m = v & SLIDE_SWAR_MSB;
    return v & (m - (m >> 15));
}

/* Subtract wsize < 32768 with unsigned saturation across all 16-bit lanes of a word.
 * Setting bit 15 prevents cross-lane borrows and leaves bit 15 of t set iff (v & 0x7FFF) >= wsize. */
Z_FORCEINLINE static slide_word_t slide_swar_lt32k(slide_word_t v, slide_word_t sw) {
    slide_word_t t = (v | SLIDE_SWAR_MSB) - sw;
    slide_word_t ge = (v | t) & SLIDE_SWAR_MSB;
    return t & ((ge - (ge >> 15)) | (v & SLIDE_SWAR_MSB));
}

/* Slide the hash table when sliding the window down, two words at a time.
 * Kept out of line so Clang does not duplicate the loops at all three call sites. */
Z_NOINLINE static void slide_hash_c_chain(Pos *table, uint32_t entries, Pos wsize) {
    Assert(((uintptr_t)table & (sizeof(slide_word_t) - 1)) == 0, "table must be word aligned");
    Assert(entries > 0 && (entries % (2 * SLIDE_SWAR_LANES)) == 0, "hash table size err");
    Assert(wsize <= 32768, "slide_len should not exceed 32768");

    Pos *q = (Pos *)HINT_ALIGNED(table, sizeof(slide_word_t));
    if (wsize == 32768) {
        do {
            slide_word_t v0 = slide_load(q);
            slide_word_t v1 = slide_load(q + SLIDE_SWAR_LANES);
            slide_store(q, slide_swar_32k(v0));
            slide_store(q + SLIDE_SWAR_LANES, slide_swar_32k(v1));
            q += 2 * SLIDE_SWAR_LANES;
            entries -= 2 * SLIDE_SWAR_LANES;
        } while (entries > 0);
    } else {
        slide_word_t sw = (slide_word_t)wsize * SLIDE_SWAR_REP;
        do {
            slide_word_t v0 = slide_load(q);
            slide_word_t v1 = slide_load(q + SLIDE_SWAR_LANES);
            slide_store(q, slide_swar_lt32k(v0, sw));
            slide_store(q + SLIDE_SWAR_LANES, slide_swar_lt32k(v1, sw));
            q += 2 * SLIDE_SWAR_LANES;
            entries -= 2 * SLIDE_SWAR_LANES;
        } while (entries > 0);
    }
}

Z_INTERNAL void slide_hash_c(deflate_state *s) {
    Assert(s->slide_len <= 32768, "slide_len should not exceed 32768");
    Pos slide = (Pos)s->slide_len;

    slide_hash_c_chain(s->head, HASH_SIZE, slide);
    slide_hash_c_chain(s->prev, s->w_size, slide);
}

Z_INTERNAL void slide_hash_head_c(deflate_state *s) {
    Assert(s->slide_len <= 32768, "slide_len should not exceed 32768");
    Pos slide = (Pos)s->slide_len;

    slide_hash_c_chain(s->head, HASH_SIZE, slide);
}

#endif /* SLIDE_HASH_FALLBACK */
