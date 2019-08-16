/* fill_window_arm.c -- Optimized hash table shifting for ARM with support for NEON instructions
 * Copyright (C) 2017 Mika T. Lindqvist
 *
 * Authors:
 * Mika T. Lindqvist <postmaster@raasu.org>
 * Jun He <jun.he@arm.com>
 *
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

/* @(#) $Id$ */

#include "../../zbuild.h"
#include "../../deflate.h"
#include "../../deflate_p.h"
#include "../../functable.h"

extern ZLIB_INTERNAL int read_buf(PREFIX3(stream) *strm, unsigned char *buf, unsigned size);

#if defined(__ARM_NEON__) || defined(__ARM_NEON)
#include <arm_neon.h>

/* SIMD version of hash_chain rebase */
static inline void slide_hash_chain(Pos *table, unsigned int entries, uint16_t window_size) {
    register uint16x8_t v, *p;
    register size_t n;

    size_t size = entries*sizeof(table[0]);
    Assert((size % sizeof(uint16x8_t) * 8 == 0), "hash table size err");

    Assert(sizeof(Pos) == 2, "Wrong Pos size");
    v = vdupq_n_u16(window_size);

    p = (uint16x8_t *)table;
    n = size / (sizeof(uint16x8_t) * 8);
    do {
        p[0] = vqsubq_u16(p[0], v);
        p[1] = vqsubq_u16(p[1], v);
        p[2] = vqsubq_u16(p[2], v);
        p[3] = vqsubq_u16(p[3], v);
        p[4] = vqsubq_u16(p[4], v);
        p[5] = vqsubq_u16(p[5], v);
        p[6] = vqsubq_u16(p[6], v);
        p[7] = vqsubq_u16(p[7], v);
        p += 8;
    } while (--n);
}
#else
/* generic version for hash rebase */
static inline void slide_hash_chain(Pos *table, unsigned int entries, uint16_t window_size) {
    unsigned int i;
    for (i = 0; i < entries; i++) {
        table[i] = (table[i] >= window_size) ? (table[i] - window_size) : NIL;
    }
}
#endif

void fill_window_arm(deflate_state *s) {
    register unsigned n;
    unsigned long more;  /* Amount of free space at the end of the window. */
    unsigned int wsize = s->w_size;

    Assert(s->lookahead < MIN_LOOKAHEAD, "already enough lookahead");

    do {
        more = s->window_size - s->lookahead - s->strstart;

        /* If the window is almost full and there is insufficient lookahead,
         * move the upper half to the lower one to make room in the upper half.
         */
        if (s->strstart >= wsize+MAX_DIST(s)) {
            memcpy(s->window, s->window+wsize, wsize);
            s->match_start -= wsize;
            s->strstart    -= wsize; /* we now have strstart >= MAX_DIST */
            s->block_start -= wsize;

            /* Slide the hash table (could be avoided with 32 bit values
               at the expense of memory usage). We slide even when level == 0
               to keep the hash table consistent if we switch back to level > 0
               later. (Using level 0 permanently is not an optimal usage of
               zlib, so we don't care about this pathological case.)
             */

            slide_hash_chain(s->head, s->hash_size, wsize);
            slide_hash_chain(s->prev, wsize, wsize);
            more += wsize;
        }
        if (s->strm->avail_in == 0)
            break;

        /* If there was no sliding:
         *    strstart <= WSIZE+MAX_DIST-1 && lookahead <= MIN_LOOKAHEAD - 1 &&
         *    more == window_size - lookahead - strstart
         * => more >= window_size - (MIN_LOOKAHEAD-1 + WSIZE + MAX_DIST-1)
         * => more >= window_size - 2*WSIZE + 2
         * In the BIG_MEM or MMAP case (not yet supported),
         *   window_size == input_size + MIN_LOOKAHEAD  &&
         *   strstart + s->lookahead <= input_size => more >= MIN_LOOKAHEAD.
         * Otherwise, window_size == 2*WSIZE so more >= 2.
         * If there was sliding, more >= WSIZE. So in all cases, more >= 2.
         */
        Assert(more >= 2, "more < 2");

        n = read_buf(s->strm, s->window + s->strstart + s->lookahead, more);
        s->lookahead += n;

        /* Initialize the hash value now that we have some input: */
        if (s->lookahead + s->insert >= MIN_MATCH) {
            unsigned int str = s->strstart - s->insert;
            unsigned int insert_cnt = s->insert;
            unsigned int slen;

            s->ins_h = s->window[str];

            if (unlikely(s->lookahead < MIN_MATCH))
                insert_cnt += s->lookahead - MIN_MATCH;
            slen = insert_cnt;
            if (str >= (MIN_MATCH - 2))
            {
                str += 2 - MIN_MATCH;
                insert_cnt += MIN_MATCH - 2;
            }
            if (insert_cnt > 0)
            {
                functable.insert_string(s, str, insert_cnt);
                s->insert -= slen;
            }
        }
        /* If the whole input has less than MIN_MATCH bytes, ins_h is garbage,
         * but this is not important since only literal bytes will be emitted.
         */
    } while (s->lookahead < MIN_LOOKAHEAD && s->strm->avail_in != 0);

    /* If the WIN_INIT bytes after the end of the current data have never been
     * written, then zero those bytes in order to avoid memory check reports of
     * the use of uninitialized (or uninitialised as Julian writes) bytes by
     * the longest match routines.  Update the high water mark for the next
     * time through here.  WIN_INIT is set to MAX_MATCH since the longest match
     * routines allow scanning to strstart + MAX_MATCH, ignoring lookahead.
     */
    if (s->high_water < s->window_size) {
        unsigned long curr = s->strstart + (unsigned long)s->lookahead;
        unsigned long init;

        if (s->high_water < curr) {
            /* Previous high water mark below current data -- zero WIN_INIT
             * bytes or up to end of window, whichever is less.
             */
            init = s->window_size - curr;
            if (init > WIN_INIT)
                init = WIN_INIT;
            memset(s->window + curr, 0, init);
            s->high_water = curr + init;
        } else if (s->high_water < curr + WIN_INIT) {
            /* High water mark at or above current data, but below current data
             * plus WIN_INIT -- zero out to current data plus WIN_INIT, or up
             * to end of window, whichever is less.
             */
            init = curr + WIN_INIT;
            if (init > s->window_size)
                init = s->window_size;
            init -= s->high_water;
            memset(s->window + s->high_water, 0, init);
            s->high_water += init;
        }
    }

    Assert((unsigned long)s->strstart <= s->window_size - MIN_LOOKAHEAD, "not enough room for search");
}
