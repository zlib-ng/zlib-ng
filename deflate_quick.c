/*
 * The deflate_quick deflate strategy, designed to be used when cycles are
 * at a premium.
 *
 * Copyright (C) 2013 Intel Corporation. All rights reserved.
 * Authors:
 *  Wajdi Feghali   <wajdi.k.feghali@intel.com>
 *  Jim Guilford    <james.guilford@intel.com>
 *  Vinodh Gopal    <vinodh.gopal@intel.com>
 *     Erdinc Ozturk   <erdinc.ozturk@intel.com>
 *  Jim Kukunas     <james.t.kukunas@linux.intel.com>
 *
 * Portions are Copyright (C) 2016 12Sided Technology, LLC.
 * Author:
 *  Phil Vachon     <pvachon@12sidedtech.com>
 *
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#include "zbuild.h"
#include "deflate.h"
#include "deflate_p.h"
#include "functable.h"
#include "trees_emit.h"

extern const ct_data static_ltree[L_CODES+2];
extern const ct_data static_dtree[D_CODES];

#define QUICK_START_BLOCK(s, last) { \
    zng_tr_emit_tree(s, STATIC_TREES, last); \
    s->block_open = 1; \
    s->block_start = s->strstart; \
}

#define QUICK_END_BLOCK(s, last) { \
    if (s->block_open) { \
        zng_tr_emit_end_block(s, static_ltree, last); \
        s->block_open = 0; \
        s->block_start = s->strstart; \
    } \
} 

ZLIB_INTERNAL block_state deflate_quick(deflate_state *s, int flush) {
    Pos hash_head;
    unsigned dist, match_len, last;

    do {
        if (s->lookahead < MIN_LOOKAHEAD) {
            fill_window(s);
            if (s->lookahead < MIN_LOOKAHEAD && flush == Z_NO_FLUSH) {
                /* Always emit end block, in case next call is with Z_FINISH,
                   and we need to emit start of last block */
                QUICK_END_BLOCK(s, 0);
                return need_more;
            }
            if (s->lookahead == 0)
                break;
        }

        if (s->pending + ((BIT_BUF_SIZE + 7) >> 3) >= s->pending_buf_size) {
            flush_pending(s->strm);
            if (s->strm->avail_out == 0 && flush != Z_FINISH) {
                /* Break to emit end block and return need_more */
                break;
            }
        }

        if (s->block_open == 0) {
            /* Start new block when we have lookahead data, so that if no
               input data is given an empty block will not be written */
            last = (flush == Z_FINISH) ? 1 : 0;
            QUICK_START_BLOCK(s, last);
        }

        if (s->lookahead >= MIN_MATCH) {
            hash_head = functable.quick_insert_string(s, s->strstart);
            dist = s->strstart - hash_head;

            if (dist > 0 && dist < MAX_DIST(s)) {
                match_len = functable.compare258(s->window + s->strstart, s->window + hash_head);

                if (match_len >= MIN_MATCH) {
                    if (match_len > s->lookahead)
                        match_len = s->lookahead;

                    check_match(s, s->strstart, hash_head, match_len);

                    zng_tr_emit_dist(s, static_ltree, static_dtree, match_len - MIN_MATCH, dist);
                    s->lookahead -= match_len;
                    s->strstart += match_len;
                    continue;
                }
            }
        }

        zng_tr_emit_lit(s, static_ltree, s->window[s->strstart]);
        s->strstart++;
        s->lookahead--;
    } while (s->strm->avail_out != 0);

    s->insert = s->strstart < MIN_MATCH - 1 ? s->strstart : MIN_MATCH-1;

    last = (flush == Z_FINISH) ? 1 : 0;
    QUICK_END_BLOCK(s, last);
    flush_pending(s->strm);

    if (last) {
        if (s->strm->avail_out == 0)
            return s->strm->avail_in == 0 ? finish_started : need_more;
        else
            return finish_done;
    }

    return block_done;
}
