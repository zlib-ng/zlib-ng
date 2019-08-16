/* insert_string_acle.c -- insert_string variant using ACLE's CRC instructions
 *
 * Copyright (C) 1995-2013 Jean-loup Gailly and Mark Adler
 * For conditions of distribution and use, see copyright notice in zlib.h
 *
 */

#if defined(__ARM_FEATURE_CRC32) && defined(ARM_ACLE_CRC_HASH)
#include <arm_acle.h>
#include "../../zbuild.h"
#include "../../deflate.h"

/* ===========================================================================
 * Insert string str in the dictionary and set match_head to the previous head
 * of the hash chain (the most recent string with same hash key). Return
 * the previous length of the hash chain.
 * IN  assertion: all calls to to INSERT_STRING are made with consecutive
 *    input characters and the first MIN_MATCH bytes of str are valid
 *    (except for the last MIN_MATCH-1 bytes of the input file).
 */
Pos insert_string_acle(deflate_state *const s, const Pos str, unsigned int count) {
    Pos p, lp, ret;

    if (unlikely(count == 0)) {
        return s->prev[str & s->w_mask];
    }

    ret = 0;
    lp = str + count - 1; /* last position */

    for (p = str; p <= lp; p++) {
        uint32_t val, h, hm;
        memcpy(&val, &s->window[p], sizeof(val));

        if (s->level >= TRIGGER_LEVEL)
            val &= 0xFFFFFF;

        h = __crc32w(0, val);
        hm = h & s->hash_mask;

        Pos head = s->head[hm];
        if (head != p) {
            s->prev[p & s->w_mask] = head;
            s->head[hm] = p;
            if (p == lp)
              ret = head;
        } else if (p == lp) {
          ret = p;
        }
    }
    return ret;
}
#endif
