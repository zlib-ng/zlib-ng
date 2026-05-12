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
#include "zmemory.h"
#include "deflate.h"
#include "deflate_p.h"
#include "functable.h"
#include "trees_emit.h"
#include "insert_string_p.h"

extern const ct_data static_ltree[L_CODES+2];
extern const ct_data static_dtree[D_CODES];

Z_FORCEINLINE static void quick_start_block(deflate_state *s, uint32_t strstart, int last) {
    zng_tr_emit_tree(s, STATIC_TREES, last);
    s->block_open = 1 + last;
    s->block_start = (int)strstart;
}

Z_FORCEINLINE static int quick_end_block(deflate_state *s, uint32_t strstart, int last) {
    if (s->block_open) {
        zng_tr_emit_end_block(s, static_ltree, last);
        s->block_open = 0;
        s->block_start = (int)strstart;
        PREFIX(flush_pending)(s->strm);
        return (s->strm->avail_out == 0);
    }
    return 0;
}

Z_FORCEINLINE static block_state deflate_quick_impl(deflate_state *s, int flush,
                                                   uint32_t strstart, uint32_t lookahead) {
    unsigned char *window;
    unsigned last = (flush == Z_FINISH) ? 1 : 0;

    if (UNLIKELY(last && s->block_open != 2)) {
        /* Emit end of previous block */
        if (quick_end_block(s, strstart, 0))
            return need_more;
        /* Emit start of last block */
        quick_start_block(s, strstart, last);
    } else if (UNLIKELY(s->block_open == 0 && lookahead > 0)) {
        /* Start new block only when we have lookahead data, so that if no
           input data is given an empty block will not be written */
        quick_start_block(s, strstart, last);
    }

    window = s->window;

    for (;;) {
        if (UNLIKELY(s->pending + ((BIT_BUF_SIZE + 7) >> 3) >= s->pending_buf_size)) {
            PREFIX(flush_pending)(s->strm);
            if (s->strm->avail_out == 0) {
                s->lookahead = lookahead;
                s->strstart = strstart;
                return (last && s->strm->avail_in == 0 && s->bi_valid == 0 && s->block_open == 0) ? finish_started : need_more;
            }
        }

        if (UNLIKELY(lookahead < MIN_LOOKAHEAD)) {
            s->lookahead = lookahead;
            s->strstart = strstart;
            PREFIX(fill_window)(s);
            lookahead = s->lookahead;
            strstart = s->strstart;
            if (UNLIKELY(lookahead < MIN_LOOKAHEAD && flush == Z_NO_FLUSH))
                return need_more;
            if (UNLIKELY(lookahead == 0))
                break;

            if (UNLIKELY(s->block_open == 0)) {
                /* Start new block when we have lookahead data, so that if no
                   input data is given an empty block will not be written */
                quick_start_block(s, strstart, last);
            }
        }

        uint32_t str_val = Z_U32_FROM_LE(zng_memread_4(window + strstart));

        if (LIKELY(lookahead >= WANT_MIN_MATCH)) {
            uint32_t hash_head = quick_insert_value(s, strstart, str_val);
            int64_t dist = (int64_t)strstart - hash_head;

            if (dist <= MAX_DIST(s) && dist > 0) {
                const uint8_t *match_start = window + hash_head;
                uint32_t match_val = Z_U32_FROM_LE(zng_memread_4(match_start));

                if (str_val == match_val) {
                    const uint8_t *scan_start = window + strstart;
                    uint32_t match_len = FUNCTABLE_CALL(compare256)(scan_start+2, match_start+2) + 2;

                    if (match_len >= WANT_MIN_MATCH) {
                        if (UNLIKELY(match_len > lookahead))
                            match_len = lookahead;

                        Assert(match_len <= STD_MAX_MATCH, "match too long");
                        Assert(strstart <= UINT16_MAX, "strstart should fit in uint16_t");
                        check_match(s, strstart, hash_head, match_len);

                        zng_tr_emit_dist(s, static_ltree, static_dtree, match_len - STD_MIN_MATCH, (uint32_t)dist);
                        lookahead -= match_len;
                        strstart += match_len;
                        continue;
                    }
                }
            }
        }

        zng_tr_emit_lit(s, static_ltree, (uint8_t)str_val);
        strstart++;
        lookahead--;
    }

    s->lookahead = lookahead;
    s->strstart = strstart;
    s->insert = strstart < (STD_MIN_MATCH - 1) ? strstart : (STD_MIN_MATCH - 1);
    if (UNLIKELY(last)) {
        if (quick_end_block(s, strstart, 1))
            return finish_started;
        return finish_done;
    }

    if (quick_end_block(s, strstart, 0))
        return need_more;
    return block_done;
}

Z_INTERNAL block_state deflate_quick(deflate_state *s, int flush) {
    return deflate_quick_impl(s, flush, s->strstart, s->lookahead);
}
