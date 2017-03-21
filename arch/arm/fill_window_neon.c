/*
 * Fill Window with NEON-optimized hash shifting
 *
 * Copyright (C) 2017 ARM Corporation
 * Author:
 *  Jun He    <jun.he@arm.com>
 *
 * For conditions of distribution and use, see copyright notice in zlib.h
 */
#ifdef ARM_NEON_FILL_WINDOW

#include <arm_neon.h>
#include "deflate.h"

/* SIMD version of hash_chain rebase */
static inline void slide_hash_neon(Pos *table, int entries, uint16_t window_size)
{
    register uint16x8_t v, *p;
    register size_t n;

    size_t size = entries*sizeof(table[0]);
    if ((size % sizeof(int16x8_t) * 8 != 0))
        Assert(1 > 0, "hash table size err");

    Assert(sizeof(Pos) == 2, "Wrong Posf size");
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

extern int read_buf(z_stream *strm, unsigned char *buf, unsigned size);

ZLIB_INTERNAL void fill_window_neon(deflate_state *s)
{
    register unsigned n;
    unsigned more;    /* Amount of free space at the end of the window. */
    unsigned int wsize = s->w_size;

    Assert(s->lookahead < MIN_LOOKAHEAD, "already enough lookahead");

    do {
        more = (unsigned)(s->window_size -(unsigned long)s->lookahead -(unsigned long)s->strstart);

        /* If the window is almost full and there is insufficient lookahead,
         * move the upper half to the lower one to make room in the upper half.
         */
        if (s->strstart >= wsize+MAX_DIST(s)) {
            memcpy(s->window, s->window+wsize, (unsigned)wsize - more);
            s->match_start -= wsize;
            s->strstart    -= wsize; /* we now have strstart >= MAX_DIST */
            s->block_start -= (long) wsize;

            n = s->hash_size;
            slide_hash_neon(s->head, n, wsize);
            n = wsize;
            slide_hash_neon(s->prev, n, wsize);
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
            s->ins_h = s->window[str];
            if (str >= 1)
                UPDATE_HASH(s, s->ins_h, str + 1 - (MIN_MATCH-1));
#if MIN_MATCH != 3
#warning    Call UPDATE_HASH MIN_MATCH-3 more times
#endif
            while (s->insert) {
                UPDATE_HASH(s, s->ins_h, str);
                s->prev[str & s->w_mask] = s->head[s->ins_h];
                s->head[s->ins_h] = (Pos)str;
                str++;
                s->insert--;
                if (s->lookahead + s->insert < MIN_MATCH)
                    break;
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
        unsigned long curr = s->strstart + (unsigned long)(s->lookahead);
        unsigned long init;

        if (s->high_water < curr) {
            /* Previous high water mark below current data -- zero WIN_INIT
             * bytes or up to end of window, whichever is less.
             */
            init = s->window_size - curr;
            if (init > WIN_INIT)
                init = WIN_INIT;
            memset(s->window + curr, 0, (unsigned)init);
            s->high_water = curr + init;
        } else if (s->high_water < (unsigned long)curr + WIN_INIT) {
            /* High water mark at or above current data, but below current data
             * plus WIN_INIT -- zero out to current data plus WIN_INIT, or up
             * to end of window, whichever is less.
             */
            init = (unsigned long)curr + WIN_INIT - s->high_water;
            if (init > s->window_size - s->high_water)
                init = s->window_size - s->high_water;
            memset(s->window + s->high_water, 0, (unsigned)init);
            s->high_water += init;
        }
    }

    Assert((unsigned long)s->strstart <= s->window_size - MIN_LOOKAHEAD,
           "not enough room for search");
}
#endif
