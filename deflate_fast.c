/* deflate_fast.c -- compress data using the fast strategy of deflation algorithm
 *
 * Copyright (C) 1995-2013 Jean-loup Gailly and Mark Adler
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#include "zbuild.h"
#include "deflate.h"
#include "deflate_p.h"
#include "match_p.h"
#include "functable.h"

/* ===========================================================================
 * Compress as much as possible from the input stream, return the current
 * block state.
 * This function does not perform lazy evaluation of matches and inserts
 * new strings in the dictionary only for unmatched strings or for short
 * matches. It is used only for the fast compression options.
 */
ZLIB_INTERNAL block_state deflate_fast(deflate_state *s, int flush) {
    IPos hash_head;       /* head of the hash chain */
    int bflush;           /* set if current block must be flushed */

    for (;;) {
        /* Make sure that we always have enough lookahead, except
         * at the end of the input file. We need MAX_MATCH bytes
         * for the next match, plus MIN_MATCH bytes to insert the
         * string following the next match.
         */
        if (s->lookahead < MIN_LOOKAHEAD) {
            functable.fill_window(s);
            if (s->lookahead < MIN_LOOKAHEAD && flush == Z_NO_FLUSH) {
                return need_more;
            }
            if (s->lookahead == 0)
                break; /* flush the current block */
        }

        /* Insert the string window[strstart .. strstart+2] in the
         * dictionary, and set hash_head to the head of the hash chain:
         */
        hash_head = NIL;
        if (s->lookahead >= MIN_MATCH) {
            hash_head = functable.insert_string(s, s->strstart, 1);
        }

        /* Find the longest match, discarding those <= prev_length.
         * At this point we have always match_length < MIN_MATCH
         */
        if (hash_head != NIL && s->strstart - hash_head <= MAX_DIST(s)) {
            /* To simplify the code, we prevent matches with the string
             * of window index 0 (in particular we have to avoid a match
             * of the string with itself at the start of the input file).
             */
            s->match_length = longest_match(s, hash_head);
            /* longest_match() sets match_start */
        }
        if (s->match_length >= MIN_MATCH) {
            check_match(s, s->strstart, s->match_start, s->match_length);

            zng_tr_tally_dist(s, s->strstart - s->match_start, s->match_length - MIN_MATCH, bflush);

            s->lookahead -= s->match_length;

            /* Insert new strings in the hash table only if the match length
             * is not too large. This saves time but degrades compression.
             */
            if (s->match_length <= s->max_insert_length && s->lookahead >= MIN_MATCH) {
                s->match_length--; /* string at strstart already in table */
                s->strstart++;
#ifdef NOT_TWEAK_COMPILER
                do {
                    functable.insert_string(s, s->strstart, 1);
                    s->strstart++;
                    /* strstart never exceeds WSIZE-MAX_MATCH, so there are
                     * always MIN_MATCH bytes ahead.
                     */
                } while (--s->match_length != 0);
#else
                {
                    functable.insert_string(s, s->strstart, s->match_length);
                    s->strstart += s->match_length;
                    s->match_length = 0;
                }
#endif
            } else {
                s->strstart += s->match_length;
                s->match_length = 0;
                s->ins_h = s->window[s->strstart];
#ifndef NOT_TWEAK_COMPILER
                functable.insert_string(s, s->strstart + 2 - MIN_MATCH, MIN_MATCH - 2);
#else
                functable.insert_string(s, s->strstart + 2 - MIN_MATCH, 1);
#if MIN_MATCH != 3
#warning        Call insert_string() MIN_MATCH-3 more times
#endif
#endif
                /* If lookahead < MIN_MATCH, ins_h is garbage, but it does not
                 * matter since it will be recomputed at next deflate call.
                 */
            }
        } else {
            /* No match, output a literal byte */
            Tracevv((stderr, "%c", s->window[s->strstart]));
            zng_tr_tally_lit(s, s->window[s->strstart], bflush);
            s->lookahead--;
            s->strstart++;
        }
        if (bflush)
            FLUSH_BLOCK(s, 0);
    }
    s->insert = s->strstart < MIN_MATCH-1 ? s->strstart : MIN_MATCH-1;
    if (flush == Z_FINISH) {
        FLUSH_BLOCK(s, 1);
        return finish_done;
    }
    if (s->sym_next)
        FLUSH_BLOCK(s, 0);
    return block_done;
}
