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

/* Returns non-zero when the output buffer is full after closing the block. */
Z_FORCEINLINE static int quick_end_block(deflate_state *s, uint32_t strstart, int last,
                                         const int static_emit) {
    if (!static_emit) {
        /* Tallied symbols are coded by the flush, with trees built for the block */
        if (last || s->sym_next) {
            FLUSH_BLOCK_ONLY(s, s->window, last);
            return (s->strm->avail_out == 0);
        }
        return 0;
    }
    if (s->block_open) {
        zng_tr_emit_end_block(s, static_ltree, last);
        s->block_open = 0;
        s->block_start = (int)strstart;
        PREFIX(flush_pending)(s->strm);
        return (s->strm->avail_out == 0);
    }
    return 0;
}

/* Returns non-zero when the output buffer is full before the block could start. */
Z_FORCEINLINE static int quick_start_block(deflate_state *s, uint32_t strstart, uint32_t lookahead, int last,
                                           const int static_emit) {
    /* Tallied blocks get their header from the flush */
    if (!static_emit)
        return 0;
    if (UNLIKELY(last && s->block_open == 1)) {
        /* Emit end of previous block so the last block can carry the final bit */
        if (quick_end_block(s, strstart, 0, static_emit))
            return 1;
    }
    /* Start new block only when we have lookahead data or it is the last block,
       so that if no input data is given an empty block will not be written */
    if (UNLIKELY(s->block_open == 0 && (last || lookahead > 0))) {
        zng_tr_emit_tree(s, STATIC_TREES, last);
        s->block_open = 1 + last;
        s->block_start = (int)strstart;
    }
    return 0;
}

/* Direct emission writes into the pending buffer, so room is made before each
   symbol. Returns non-zero when the output buffer is full. */
Z_FORCEINLINE static int quick_flush_pending(deflate_state *s, const int static_emit) {
    /* Tallied symbols reach the pending buffer only at the block flush */
    if (!static_emit)
        return 0;
    if (UNLIKELY(s->pending + ((BIT_BUF_SIZE + 7) >> 3) >= s->pending_buf_size)) {
        PREFIX(flush_pending)(s->strm);
        return (s->strm->avail_out == 0);
    }
    return 0;
}

/* Returns non-zero when the output buffer is full after a block flush. */
Z_FORCEINLINE static int quick_emit_lit(deflate_state *s, uint8_t lit, uint32_t *strstart,
                                        uint32_t *lookahead, const int static_emit) {
    if (static_emit) {
        zng_tr_emit_lit(s, static_ltree, lit);
        (*strstart)++;
        (*lookahead)--;
        return 0;
    }
    int bflush = zng_tr_tally_lit(s, lit);
    (*strstart)++;
    (*lookahead)--;
    if (UNLIKELY(bflush)) {
        s->strstart = *strstart;
        s->lookahead = *lookahead;
        return quick_end_block(s, *strstart, 0, static_emit);
    }
    return 0;
}

/* Returns non-zero when the output buffer is full after a block flush. */
Z_FORCEINLINE static int quick_emit_dist(deflate_state *s, uint32_t match_len, uint32_t dist,
                                         uint32_t *strstart, uint32_t *lookahead, const int static_emit) {
    if (static_emit) {
        zng_tr_emit_dist(s, static_ltree, static_dtree, match_len - STD_MIN_MATCH, dist);
        *lookahead -= match_len;
        *strstart += match_len;
        return 0;
    }
    int bflush = zng_tr_tally_dist(s, dist, match_len - STD_MIN_MATCH);
    *lookahead -= match_len;
    *strstart += match_len;
    if (UNLIKELY(bflush)) {
        s->strstart = *strstart;
        s->lookahead = *lookahead;
        return quick_end_block(s, *strstart, 0, static_emit);
    }
    return 0;
}

/* static_emit is a compile-time constant, so each instantiation keeps only its own mode. */
Z_FORCEINLINE static block_state deflate_quick_impl(deflate_state *s, int flush, const int static_emit) {
    unsigned char *window;
    /* Carrying the scan state in locals keeps it in callee-saved registers
       across the compare256 and flush calls, instead of a load and store
       pair through the state on every symbol. */
    uint32_t strstart = s->strstart;
    uint32_t lookahead = s->lookahead;
    unsigned last = (flush == Z_FINISH) ? 1 : 0;

    if (quick_start_block(s, strstart, lookahead, last, static_emit))
        return need_more;

    window = s->window;

    for (;;) {
        if (quick_flush_pending(s, static_emit)) {
            s->lookahead = lookahead;
            s->strstart = strstart;
            return (last && s->strm->avail_in == 0 && s->bi_valid == 0 && s->block_open == 0) ? finish_started : need_more;
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
                quick_start_block(s, strstart, lookahead, last, static_emit);
            }
        }

        uint32_t str_val = Z_U32_FROM_LE(zng_memread_4(window + strstart));

        if (LIKELY(lookahead >= WANT_MIN_MATCH)) {
            uint32_t hash_head = insert_knuth_val_head(s, strstart, str_val);
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

                        if (quick_emit_dist(s, match_len, (uint32_t)dist, &strstart, &lookahead, static_emit))
                            return need_more;
                        continue;
                    }
                }
            }
        }

        if (quick_emit_lit(s, (uint8_t)str_val, &strstart, &lookahead, static_emit))
            return need_more;
    }

    s->lookahead = lookahead;
    s->strstart = strstart;
    s->insert = strstart < (STD_MIN_MATCH - 1) ? strstart : (STD_MIN_MATCH - 1);
    if (UNLIKELY(last)) {
        if (quick_end_block(s, strstart, 1, static_emit))
            return finish_started;
        return finish_done;
    }

    if (quick_end_block(s, strstart, 0, static_emit))
        return need_more;
    return block_done;
}

/* Z_FIXED path: every symbol goes straight through the static tables. */
static block_state deflate_quick_static(deflate_state *s, int flush) {
    return deflate_quick_impl(s, flush, 1);
}

/* Default path: symbols are tallied so the flush builds per-block trees. */
static block_state deflate_quick_dynamic(deflate_state *s, int flush) {
    return deflate_quick_impl(s, flush, 0);
}

Z_INTERNAL block_state deflate_quick(deflate_state *s, int flush) {
    if (UNLIKELY(s->strategy == Z_FIXED))
        return deflate_quick_static(s, flush);
    return deflate_quick_dynamic(s, flush);
}
