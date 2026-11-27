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
#define SLIDE_WORD_ENTRIES ((uint32_t)(sizeof(slide_word_t) / sizeof(Pos)))

Z_FORCEINLINE static slide_word_t slide_load(Pos *ptr) {
    slide_word_t word;
    memcpy(&word, HINT_ALIGNED(ptr, sizeof(slide_word_t)), sizeof(word));
    return word;
}

Z_FORCEINLINE static void slide_store(Pos *ptr, slide_word_t word) {
    memcpy(HINT_ALIGNED(ptr, sizeof(slide_word_t)), &word, sizeof(word));
}

/* Turn each entry's bit-15 flag (0x8000 or 0x0000) into a 15-bit mask (0x7FFF or 0x0000). */
Z_FORCEINLINE static slide_word_t slide_mask_low15(slide_word_t flags) {
    return flags - (flags >> 15);
}

/* Subtract 32768 with unsigned saturation across all 16-bit entries in a word. */
Z_FORCEINLINE static slide_word_t slide_swar_32k(slide_word_t word) {
    slide_word_t keep = word & SLIDE_SWAR_MSB;
    return word & slide_mask_low15(keep);
}

/* Subtract wsize < 32768 with unsigned saturation across all 16-bit entries in a word. */
Z_FORCEINLINE static slide_word_t slide_swar_lt32k(slide_word_t word, slide_word_t slide_word) {
    slide_word_t diff = (word | SLIDE_SWAR_MSB) - slide_word;
    slide_word_t keep = (word | diff) & SLIDE_SWAR_MSB;
    slide_word_t mask = slide_mask_low15(keep) | (word & SLIDE_SWAR_MSB);
    return diff & mask;
}

/* Slide the hash table when sliding the window down, two words at a time.
 * Kept out of line so Clang does not duplicate the loops at all three call sites. */
Z_NOINLINE static void slide_hash_c_chain(Pos *table, uint32_t entries, Pos wsize) {
    Assert(((uintptr_t)table & (sizeof(slide_word_t) - 1)) == 0, "table must be word aligned");
    Assert(entries > 0 && (entries % (2 * SLIDE_WORD_ENTRIES)) == 0, "hash table size err");
    Assert(wsize <= 32768, "slide_len should not exceed 32768");

    table = (Pos *)HINT_ALIGNED(table, sizeof(slide_word_t));
    if (wsize == 32768) {
        do {
            slide_word_t word0 = slide_load(table);
            slide_word_t word1 = slide_load(table + SLIDE_WORD_ENTRIES);
            slide_store(table, slide_swar_32k(word0));
            slide_store(table + SLIDE_WORD_ENTRIES, slide_swar_32k(word1));
            table += 2 * SLIDE_WORD_ENTRIES;
            entries -= 2 * SLIDE_WORD_ENTRIES;
        } while (entries > 0);
    } else {
        slide_word_t slide_word = (slide_word_t)wsize * SLIDE_SWAR_REP;
        do {
            slide_word_t word0 = slide_load(table);
            slide_word_t word1 = slide_load(table + SLIDE_WORD_ENTRIES);
            slide_store(table, slide_swar_lt32k(word0, slide_word));
            slide_store(table + SLIDE_WORD_ENTRIES, slide_swar_lt32k(word1, slide_word));
            table += 2 * SLIDE_WORD_ENTRIES;
            entries -= 2 * SLIDE_WORD_ENTRIES;
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
