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

#define QUICK_START_BLOCK(s, last) { \
    zng_tr_emit_tree(s, STATIC_TREES, last); \
    s->block_open = 1 + last; \
    s->block_start = (int)s->strstart; \
}

#define QUICK_END_BLOCK(s, last) { \
    if (s->block_open) { \
        zng_tr_emit_end_block(s, static_ltree, last); \
        s->block_open = 0; \
        s->block_start = (int)s->strstart; \
        PREFIX(flush_pending)(s->strm); \
        if (s->strm->avail_out == 0) \
            return (last) ? finish_started : need_more; \
    } \
}

Z_INTERNAL block_state deflate_quick(deflate_state *s, int flush) {
    unsigned char *window;
    unsigned last = (flush == Z_FINISH) ? 1 : 0;

    if (UNLIKELY(last && s->block_open != 2)) {
        /* Emit end of previous block */
        QUICK_END_BLOCK(s, 0);
        /* Emit start of last block */
        QUICK_START_BLOCK(s, last);
    } else if (UNLIKELY(s->block_open == 0 && s->lookahead > 0)) {
        /* Start new block only when we have lookahead data, so that if no
           input data is given an empty block will not be written */
        QUICK_START_BLOCK(s, last);
    }

    window = s->window;

    for (;;) {
        uint8_t lc;

        if (UNLIKELY(s->pending + ((BIT_BUF_SIZE + 7) >> 3) >= s->pending_buf_size)) {
            PREFIX(flush_pending)(s->strm);
            if (s->strm->avail_out == 0) {
                return (last && s->strm->avail_in == 0 && s->bi_valid == 0 && s->block_open == 0) ? finish_started : need_more;
            }
        }

        if (UNLIKELY(s->lookahead < MIN_LOOKAHEAD)) {
            PREFIX(fill_window)(s);
            if (UNLIKELY(s->lookahead < MIN_LOOKAHEAD && flush == Z_NO_FLUSH)) {
                return need_more;
            }
            if (UNLIKELY(s->lookahead == 0))
                break;

            if (UNLIKELY(s->block_open == 0)) {
                /* Start new block when we have lookahead data, so that if no
                   input data is given an empty block will not be written */
                QUICK_START_BLOCK(s, last);
            }
        }

        if (LIKELY(s->lookahead >= WANT_MIN_MATCH)) {
#if BYTE_ORDER == LITTLE_ENDIAN
            uint32_t str_val = zng_memread_4(window + s->strstart);
#else
            uint32_t str_val = ZSWAP32(zng_memread_4(window + s->strstart));
#endif
            uint32_t hash_head = quick_insert_value(s, s->strstart, str_val);
            int64_t dist = (int64_t)s->strstart - hash_head;
            lc = (uint8_t)str_val;

            if (dist <= MAX_DIST(s) && dist > 0) {
                const uint8_t *match_start = window + hash_head;
#if BYTE_ORDER == LITTLE_ENDIAN
                uint32_t match_val = zng_memread_4(match_start);
#else
                uint32_t match_val = ZSWAP32(zng_memread_4(match_start));
#endif

                if (str_val == match_val) {
                    const uint8_t *str_start = window + s->strstart;
                    uint32_t match_len = FUNCTABLE_CALL(compare256)(str_start+2, match_start+2) + 2;

                    if (match_len >= WANT_MIN_MATCH) {
                        if (UNLIKELY(match_len > s->lookahead))
                            match_len = s->lookahead;

                        Assert(match_len <= STD_MAX_MATCH, "match too long");
                        Assert(s->strstart <= UINT16_MAX, "strstart should fit in uint16_t");
                        check_match(s, s->strstart, hash_head, match_len);

                        zng_tr_emit_dist(s, static_ltree, static_dtree, match_len - STD_MIN_MATCH, (uint32_t)dist);
                        s->lookahead -= match_len;
                        s->strstart += match_len;
                        continue;
                    }
                }
            }
        } else {
            lc = window[s->strstart];
        }
        zng_tr_emit_lit(s, static_ltree, lc);
        s->strstart++;
        s->lookahead--;
    }

    s->insert = s->strstart < (STD_MIN_MATCH - 1) ? s->strstart : (STD_MIN_MATCH - 1);
    if (UNLIKELY(last)) {
        QUICK_END_BLOCK(s, 1);
        return finish_done;
    }

    QUICK_END_BLOCK(s, 0);
    return block_done;
}
