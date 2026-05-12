/* deflate_huff.c -- compress data using huffman encoding only strategy
 *
 * Copyright (C) 1995-2024 Jean-loup Gailly and Mark Adler
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#include "zbuild.h"
#include "deflate.h"
#include "deflate_p.h"
#include "functable.h"

/* ===========================================================================
 * For Z_HUFFMAN_ONLY, do not look for matches.  Do not maintain a hash table.
 * (It will be regenerated if this run of deflate switches away from Huffman.)
 */
Z_INTERNAL block_state deflate_huff(deflate_state *s, int flush) {
    unsigned char *window = s->window;
    int bflush = 0;         /* set if current block must be flushed */
    unsigned int lookahead = s->lookahead;
    unsigned int strstart = s->strstart;

    for (;;) {
        /* Make sure that we have a literal to write. */
        if (UNLIKELY(lookahead == 0)) {
            s->lookahead = lookahead;
            s->strstart = strstart;
            PREFIX(fill_window)(s);
            lookahead = s->lookahead;
            strstart = s->strstart;
            if (UNLIKELY(lookahead == 0)) {
                if (flush == Z_NO_FLUSH)
                    return need_more;
                break;      /* flush the current block */
            }
        }

        /* Output a literal byte */
        bflush = zng_tr_tally_lit(s, window[strstart]);
        lookahead--;
        strstart++;
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
