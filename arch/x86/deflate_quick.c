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

#include "../../zbuild.h"
#include <immintrin.h>
#ifdef _MSC_VER
#  include <nmmintrin.h>
#endif
#include "../../deflate.h"
#include "../../deflate_p.h"
#include "../../functable.h"
#include "../../memcopy.h"
#include "../../trees_emit.h"

extern const ct_data static_ltree[L_CODES+2];
extern const ct_data static_dtree[D_CODES];

static inline long compare258(const unsigned char *const src0, const unsigned char *const src1) {
#ifdef _MSC_VER
    long cnt;

    cnt = 0;
    do {
#define mode  _SIDD_UBYTE_OPS | _SIDD_CMP_EQUAL_EACH | _SIDD_NEGATIVE_POLARITY

        int ret;
        __m128i xmm_src0, xmm_src1;

        xmm_src0 = _mm_loadu_si128((__m128i *)(src0 + cnt));
        xmm_src1 = _mm_loadu_si128((__m128i *)(src1 + cnt));
        ret = _mm_cmpestri(xmm_src0, 16, xmm_src1, 16, mode);
        if (_mm_cmpestrc(xmm_src0, 16, xmm_src1, 16, mode)) {
            cnt += ret;
            break;
        }
        cnt += 16;

        xmm_src0 = _mm_loadu_si128((__m128i *)(src0 + cnt));
        xmm_src1 = _mm_loadu_si128((__m128i *)(src1 + cnt));
        ret = _mm_cmpestri(xmm_src0, 16, xmm_src1, 16, mode);
        if (_mm_cmpestrc(xmm_src0, 16, xmm_src1, 16, mode)) {
            cnt += ret;
            break;
        }
        cnt += 16;
    } while (cnt < 256);

    if (memcmp(src0 + cnt, src1 + cnt, sizeof(uint16_t)) == 0) {
        cnt += 2;
    } else if (*(src0 + cnt) == *(src1 + cnt)) {
        cnt++;
    }
    return cnt;
#else
    uintptr_t ax, dx, cx;
    __m128i xmm_src0;

    ax = 16;
    dx = 16;
    /* set cx to something, otherwise gcc thinks it's used
       uninitalised */
    cx = 0;

    __asm__ __volatile__ (
    "1:"
        "movdqu     -16(%[src0], %[ax]), %[xmm_src0]\n\t"
        "pcmpestri  $0x18, -16(%[src1], %[ax]), %[xmm_src0]\n\t"
        "jc         2f\n\t"
        "add        $16, %[ax]\n\t"

        "movdqu     -16(%[src0], %[ax]), %[xmm_src0]\n\t"
        "pcmpestri  $0x18, -16(%[src1], %[ax]), %[xmm_src0]\n\t"
        "jc         2f\n\t"
        "add        $16, %[ax]\n\t"

        "cmp        $256 + 16, %[ax]\n\t"
        "jb         1b\n\t"

#  if !defined(__x86_64__)
        "movzwl     -16(%[src0], %[ax]), %[dx]\n\t"
#  else
        "movzwq     -16(%[src0], %[ax]), %[dx]\n\t"
#  endif
        "xorw       -16(%[src1], %[ax]), %%dx\n\t"
        "jnz        3f\n\t"

        "add        $2, %[ax]\n\t"
        "jmp        4f\n\t"
    "3:\n\t"
        "rep; bsf   %[dx], %[cx]\n\t"
        "shr        $3, %[cx]\n\t"
    "2:"
        "add        %[cx], %[ax]\n\t"
    "4:"
    : [ax] "+a" (ax),
      [cx] "+c" (cx),
      [dx] "+d" (dx),
      [xmm_src0] "=x" (xmm_src0)
    : [src0] "r" (src0),
      [src1] "r" (src1)
    : "cc"
    );
    return ax - 16;
#endif
}

ZLIB_INTERNAL block_state deflate_quick(deflate_state *s, int flush) {
    IPos hash_head;
    unsigned dist, match_len, last;

    if (s->block_open == 0) {
        last = (flush == Z_FINISH) ? 1 : 0;
        zng_tr_emit_tree(s, STATIC_TREES, last);
    }

    do {
        if (s->pending + ((BIT_BUF_SIZE + 7) >> 3) >= s->pending_buf_size) {
            flush_pending(s->strm);
            if (s->strm->avail_in == 0 && flush != Z_FINISH) {
                return need_more;
            }
        }

        if (s->lookahead < MIN_LOOKAHEAD) {
            fill_window(s);
            if (s->lookahead < MIN_LOOKAHEAD && flush == Z_NO_FLUSH) {
                zng_tr_emit_end_block(s, static_ltree, 0);
                s->block_start = s->strstart;
                flush_pending(s->strm);
                return need_more;
            }
            if (s->lookahead == 0)
                break;
        }

        if (s->lookahead >= MIN_MATCH) {
            hash_head = functable.quick_insert_string(s, s->strstart);
            dist = s->strstart - hash_head;

            if (dist > 0 && (dist-1) < (s->w_size - 1)) {
                match_len = compare258(s->window + s->strstart, s->window + hash_head);

                if (match_len >= MIN_MATCH) {
                    if (match_len > s->lookahead)
                        match_len = s->lookahead;

                    if (match_len > MAX_MATCH)
                        match_len = MAX_MATCH;

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

    if (s->strm->avail_out == 0 && flush != Z_FINISH)
        return need_more;

    s->insert = s->strstart < MIN_MATCH - 1 ? s->strstart : MIN_MATCH-1;

    last = (flush == Z_FINISH) ? 1 : 0;
    zng_tr_emit_end_block(s, static_ltree, last);
    s->block_start = s->strstart;
    flush_pending(s->strm);

    if (last) {
        if (s->strm->avail_out == 0)
            return s->strm->avail_in == 0 ? finish_started : need_more;
        else
            return finish_done;
    }

    return block_done;
}
