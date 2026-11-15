/* slide_hash_swar.c -- slide hash table SWAR implementation
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#include "zbuild.h"
#include "arch_functions.h"

#ifdef SLIDE_HASH_SWAR_FALLBACK

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
#define SLIDE_SWAR_LANES (sizeof(slide_word_t) / sizeof(Pos))

Z_FORCEINLINE static slide_word_t slide_load(Pos *p) {
    slide_word_t v;
    memcpy(&v, HINT_ALIGNED(p, sizeof(slide_word_t)), sizeof(v));
    return v;
}

Z_FORCEINLINE static void slide_store(Pos *p, slide_word_t v) {
    memcpy(HINT_ALIGNED(p, sizeof(slide_word_t)), &v, sizeof(v));
}

/* Subtract 32768 with unsigned saturation across all 16-bit lanes of a word.
 * Entries below 32768 become 0 and entries at or above 32768 clear bit 15. */
Z_FORCEINLINE static slide_word_t slide_swar_32k(slide_word_t v) {
    slide_word_t m = v & SLIDE_SWAR_MSB;
    return v & (m - (m >> 15));
}

/* Subtract wsize < 32768 with unsigned saturation across all 16-bit lanes of a word.
 * Setting bit 15 before subtracting prevents borrows between adjacent lanes
 * and leaves bit 15 of t set iff the low 15 bits of the lane are >= wsize. */
Z_FORCEINLINE static slide_word_t slide_swar_lt32k(slide_word_t v, slide_word_t sw) {
    slide_word_t t = (v | SLIDE_SWAR_MSB) - sw;
    slide_word_t ge = (v | t) & SLIDE_SWAR_MSB;
    return t & ((ge - (ge >> 15)) | (v & SLIDE_SWAR_MSB));
}

/* Slide the hash table when sliding the window down, two words at a time
 * when the table is word aligned, as guaranteed by alloc_deflate.
 *
 * fill_window slides by W_BUF_SIZE(w_size) - w_size, which never exceeds
 * 32768, so larger values are only reachable from tests and simply take the
 * plain loop below.
 *
 * Not inlined, otherwise Clang duplicates and unrolls the loops at each of the
 * three call sites and roughly doubles the code size.
 */
Z_NOINLINE static void slide_hash_swar_chain(Pos *table, uint32_t entries, Pos wsize) {
    if (((uintptr_t)table & (sizeof(slide_word_t) - 1)) == 0) {
        Pos *q = (Pos *)HINT_ALIGNED(table, sizeof(slide_word_t));
        if (wsize == 32768) {
            while (entries >= 2 * SLIDE_SWAR_LANES) {
                slide_word_t v0 = slide_load(q);
                slide_word_t v1 = slide_load(q + SLIDE_SWAR_LANES);
                slide_store(q, slide_swar_32k(v0));
                slide_store(q + SLIDE_SWAR_LANES, slide_swar_32k(v1));
                q += 2 * SLIDE_SWAR_LANES;
                entries -= 2 * SLIDE_SWAR_LANES;
            }
        } else if (wsize < 32768) {
            slide_word_t sw = (slide_word_t)wsize * SLIDE_SWAR_REP;
            while (entries >= 2 * SLIDE_SWAR_LANES) {
                slide_word_t v0 = slide_load(q);
                slide_word_t v1 = slide_load(q + SLIDE_SWAR_LANES);
                slide_store(q, slide_swar_lt32k(v0, sw));
                slide_store(q + SLIDE_SWAR_LANES, slide_swar_lt32k(v1, sw));
                q += 2 * SLIDE_SWAR_LANES;
                entries -= 2 * SLIDE_SWAR_LANES;
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

Z_INTERNAL void slide_hash_swar(deflate_state *s) {
    Assert(s->slide_len <= UINT16_MAX, "slide_len should fit in uint16_t");
    Pos slide = (Pos)s->slide_len;

    slide_hash_swar_chain(s->head, HASH_SIZE, slide);
    slide_hash_swar_chain(s->prev, s->w_size, slide);
}

Z_INTERNAL void slide_hash_head_swar(deflate_state *s) {
    Assert(s->slide_len <= UINT16_MAX, "slide_len should fit in uint16_t");
    Pos slide = (Pos)s->slide_len;

    slide_hash_swar_chain(s->head, HASH_SIZE, slide);
}

#endif /* SLIDE_HASH_SWAR_FALLBACK */
