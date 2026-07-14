/* slide_hash.c -- slide hash table C implementation
 *
 * Copyright (C) 1995-2024 Jean-loup Gailly and Mark Adler
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#include "zbuild.h"
#include "arch_functions.h"

#ifdef SLIDE_HASH_FALLBACK

#include "deflate.h"

/* ===========================================================================
 * Slide the hash table when sliding the window down
 */
static inline void slide_hash_c_chain(Pos *table, uint32_t entries, Pos wsize) {
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
    Pos wsize = (Pos)s->w_size;

    slide_hash_c_chain(s->head, HASH_SIZE, wsize);
    slide_hash_c_chain(s->prev, wsize, wsize);
}

Z_INTERNAL void slide_hash_head_c(deflate_state *s) {
    uint16_t wsize = (uint16_t)s->w_size;

    slide_hash_c_chain(s->head, HASH_SIZE, wsize);
}

#endif /* SLIDE_HASH_FALLBACK */
