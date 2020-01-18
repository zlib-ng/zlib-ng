/* deflate_p.h -- Private inline functions and macros shared with more than
 *                one deflate method
 *
 * Copyright (C) 1995-2013 Jean-loup Gailly and Mark Adler
 * For conditions of distribution and use, see copyright notice in zlib.h
 *
 */

#ifndef DEFLATE_P_H
#define DEFLATE_P_H

/* Forward declare common non-inlined functions declared in deflate.c */

#include "functable.h"

#ifdef ZLIB_DEBUG
void check_match(deflate_state *s, IPos start, IPos match, int length);
#else
#define check_match(s, start, match, length)
#endif
void flush_pending(PREFIX3(stream) *strm);

/* ===========================================================================
 * Insert string str in the dictionary and set match_head to the previous head
 * of the hash chain (the most recent string with same hash key). Return
 * the previous length of the hash chain.
 * IN  assertion: all calls to to INSERT_STRING are made with consecutive
 *    input characters and the first MIN_MATCH bytes of str are valid
 *    (except for the last MIN_MATCH-1 bytes of the input file).
 */

static inline Pos insert_string(deflate_state *const s, const Pos str, unsigned int count) {
    Pos str_idx, str_end, ret;
    update_hash_cb update_hash;

    if (UNLIKELY(count == 0)) {
        return s->prev[str & s->w_mask];
    }

    ret = 0;
    str_end = str + count - 1; /* last position */
    update_hash = functable.update_hash;

    for (str_idx = str; str_idx <= str_end; str_idx++) {
        uint32_t val, hm;
#if defined(UNALIGNED_OK)
        val = *(unsigned int *)(&s->window[str_idx]);
#else
        memcpy(&val, &s->window[str_idx], sizeof(val));
#endif

        if (s->level >= TRIGGER_LEVEL)
            val &= MIN_MATCH_MASK;

        s->ins_h = update_hash(s, s->ins_h, val);
        hm = s->ins_h & s->hash_mask;

        Pos head = s->head[hm];
        if (head != str_idx) {
            s->prev[str_idx & s->w_mask] = head;
            s->head[hm] = str_idx;
            if (str_idx == str_end)
                ret = head;
        } else if (str_idx == str_end) {
            ret = str_idx;
        }
    }
    return ret;
}

/* ===========================================================================
 * Flush the current block, with given end-of-file flag.
 * IN assertion: strstart is set to the end of the current match.
 */
#define FLUSH_BLOCK_ONLY(s, last) { \
    zng_tr_flush_block(s, (s->block_start >= 0L ? \
                   (char *)&s->window[(unsigned)s->block_start] : \
                   NULL), \
                   (unsigned long)((long)s->strstart - s->block_start), \
                   (last)); \
    s->block_start = s->strstart; \
    flush_pending(s->strm); \
    Tracev((stderr, "[FLUSH]")); \
}

/* Same but force premature exit if necessary. */
#define FLUSH_BLOCK(s, last) { \
    FLUSH_BLOCK_ONLY(s, last); \
    if (s->strm->avail_out == 0) return (last) ? finish_started : need_more; \
}

/* Maximum stored block length in deflate format (not including header). */
#define MAX_STORED 65535

/* Minimum of a and b. */
#define MIN(a, b) ((a) > (b) ? (b) : (a))

#endif
