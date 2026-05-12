/* deflate_rle.c -- compress data using RLE strategy of deflation algorithm
 *
 * Copyright (C) 1995-2024 Jean-loup Gailly and Mark Adler
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#include "zbuild.h"
#include "deflate.h"
#include "deflate_p.h"
#include "functable.h"
#include "compare256_rle.h"

#if OPTIMAL_CMP == 8
#  define compare256_rle compare256_rle_8
#else
#  define compare256_rle compare256_rle_64
#endif

/* ===========================================================================
 * For Z_RLE, simply look for runs of bytes, generate matches only of distance
 * one.  Do not maintain a hash table.  (It will be regenerated if this run of
 * deflate switches away from Z_RLE.)
 */
Z_INTERNAL block_state deflate_rle(deflate_state *s, int flush) {
    unsigned char *window = s->window;
    unsigned char *scan;            /* scan goes up to strend for length of run */
    int bflush = 0;                 /* set if current block must be flushed */
    uint32_t match_len = 0;
    unsigned int lookahead = s->lookahead;
    unsigned int strstart = s->strstart;

    for (;;) {
        /* Make sure that we always have enough lookahead, except
         * at the end of the input file. We need STD_MAX_MATCH bytes
         * for the longest run, plus one for the unrolled loop.
         */
        if (UNLIKELY(lookahead <= STD_MAX_MATCH)) {
            s->lookahead = lookahead;
            s->strstart = strstart;
            PREFIX(fill_window)(s);
            lookahead = s->lookahead;
            strstart = s->strstart;
            if (UNLIKELY(lookahead <= STD_MAX_MATCH && flush == Z_NO_FLUSH))
                return need_more;
            if (UNLIKELY(lookahead == 0))
                break; /* flush the current block */
        }

        /* See how many times the previous byte repeats */
        if (LIKELY(lookahead >= STD_MIN_MATCH && strstart > 0)) {
            scan = window + strstart - 1;
            if (scan[0] == scan[1] && scan[1] == scan[2]) {
                match_len = compare256_rle(scan, scan+3)+2;
                match_len = MIN(match_len, lookahead);
            }
            Assert(scan+match_len <= window + s->window_size - 1, "wild scan");
        }

        /* Emit match if have run of STD_MIN_MATCH or longer, else emit literal */
        if (match_len >= STD_MIN_MATCH) {
            Assert(strstart <= UINT16_MAX, "strstart should fit in uint16_t");
            check_match(s, strstart, strstart - 1, match_len);

            bflush = zng_tr_tally_dist(s, 1, match_len - STD_MIN_MATCH);

            lookahead -= match_len;
            strstart += match_len;
            match_len = 0;
        } else {
            /* No match, output a literal byte */
            bflush = zng_tr_tally_lit(s, window[strstart]);
            lookahead--;
            strstart++;
        }
        if (bflush) {
            s->lookahead = lookahead;
            s->strstart = strstart;
            FLUSH_BLOCK(s, window, 0);
        }
    }
    s->lookahead = lookahead;
    s->strstart = strstart;
    s->insert = 0;
    if (flush == Z_FINISH) {
        FLUSH_BLOCK(s, window, 1);
        return finish_done;
    }
    if (s->sym_next)
        FLUSH_BLOCK(s, window, 0);
    return block_done;
}
