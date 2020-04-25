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
#include "../../memcopy.h"
#include "deflate_quick.h"

#ifdef ZLIB_DEBUG
#  include <ctype.h>
#endif

void fill_window_sse(deflate_state *s);
void flush_pending(PREFIX3(stream) *strm);

static inline long compare258(const unsigned char *const src0, const unsigned char *const src1) {
#ifdef _MSC_VER
    long cnt;

    cnt = 0;

#define mode  _SIDD_UBYTE_OPS | _SIDD_CMP_EQUAL_EACH | _SIDD_NEGATIVE_POLARITY
    do {
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
#undef mode

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

static inline void static_emit_ptr(deflate_state *const s, const int lc, const unsigned dist) {
    unsigned code1 = quick_len_codes[lc] >> 8;
    unsigned len1 =  quick_len_codes[lc] & 0xFF;
    unsigned code2 = quick_dist_codes[dist-1] >> 8;
    unsigned len2  = quick_dist_codes[dist-1] & 0xFF;

    uint32_t bi_valid = s->bi_valid;
    uint32_t bi_buf = s->bi_buf;

    send_bits(s, code1, len1, bi_buf, bi_valid);
    send_bits(s, code2, len2, bi_buf, bi_valid);
#ifdef ZLIB_DEBUG
    s->compressed_len += len1 + len2;
#endif

    s->bi_valid = bi_valid;
    s->bi_buf = bi_buf;
}

extern ZLIB_INTERNAL const ct_data static_ltree[L_CODES+2];

static inline void static_emit_lit(deflate_state *const s, const int lit) {
    uint32_t bi_valid = s->bi_valid;
    uint32_t bi_buf = s->bi_buf;

    send_bits(s, static_ltree[lit].Code, static_ltree[lit].Len, bi_buf, bi_valid);
#ifdef ZLIB_DEBUG
    s->compressed_len += static_ltree[lit].Len;
#endif

    s->bi_valid = bi_valid;
    s->bi_buf = bi_buf;

    Tracecv(isgraph(lit), (stderr, " '%c' ", lit));
}

static void static_emit_tree(deflate_state *const s, const int flush) {
    unsigned last;

    last = flush == Z_FINISH ? 1 : 0;
    Tracev((stderr, "\n--- Emit Tree: Last: %u\n", last));
    send_bits(s, (STATIC_TREES << 1)+ last, 3, s->bi_buf, s->bi_valid);
#ifdef ZLIB_DEBUG
    s->compressed_len += 3;
#endif
}

static void static_emit_end_block(deflate_state *const s, int last) {
    send_code(s, END_BLOCK, static_ltree, s->bi_buf, s->bi_valid);
#ifdef ZLIB_DEBUG
    s->compressed_len += 7;  /* 7 bits for EOB */
#endif
    Tracev((stderr, "\n+++ Emit End Block: Last: %u Pending: %u Total Out: %zu\n", last, s->pending, s->strm->total_out));

    if (last)
        bi_windup(s);

    s->block_start = s->strstart;
    flush_pending(s->strm);
    s->block_open = 0;
}

static inline Pos quick_insert_string(deflate_state *const s, const Pos str) {
    Pos ret;
    unsigned h = 0;

#ifdef _MSC_VER
    h = _mm_crc32_u32(h, *(unsigned *)(s->window + str));
#else
    __asm__ __volatile__ (
        "crc32l (%[window], %[str], 1), %0\n\t"
    : "+r" (h)
    : [window] "r" (s->window),
      [str] "r" ((uintptr_t)str)
    );
#endif

    ret = s->head[h & s->hash_mask];
    s->head[h & s->hash_mask] = str;
    return ret;
}

ZLIB_INTERNAL block_state deflate_quick(deflate_state *s, int flush) {
    IPos hash_head;
    unsigned dist, match_len;
    unsigned int wsize = s->w_size;

    if (wsize > 8192)
        wsize = 8192;

    if (s->block_open == 0) {
        static_emit_tree(s, flush);
        s->block_open = 1;
    }

    do {
        if (s->pending + 4 >= s->pending_buf_size) {
            flush_pending(s->strm);
            if (flush != Z_FINISH) {
                return need_more;
            }
        }

        if (s->lookahead < MIN_LOOKAHEAD) {
            fill_window_sse(s);
            if (s->lookahead < MIN_LOOKAHEAD && flush == Z_NO_FLUSH) {
                static_emit_end_block(s, 0);
                return need_more;
            }
            if (s->lookahead == 0)
                break;
        }

        if (s->lookahead >= MIN_MATCH) {
            hash_head = quick_insert_string(s, s->strstart);
            dist = s->strstart - hash_head;

            if (dist > 0 && (dist-1) < (wsize - 1)) {
                match_len = compare258(s->window + s->strstart, s->window + hash_head);

                if (match_len >= MIN_MATCH) {
                    if (match_len > s->lookahead)
                        match_len = s->lookahead;

                    if (match_len > MAX_MATCH)
                        match_len = MAX_MATCH;

                    static_emit_ptr(s, match_len - MIN_MATCH, dist);
                    s->lookahead -= match_len;
                    s->strstart += match_len;
                    continue;
                }
            }
        }

        static_emit_lit(s, s->window[s->strstart]);
        s->strstart++;
        s->lookahead--;
    } while (s->strm->avail_out != 0);

    if (s->strm->avail_out == 0 && flush != Z_FINISH)
        return need_more;

    s->insert = s->strstart < MIN_MATCH - 1 ? s->strstart : MIN_MATCH-1;
    if (flush == Z_FINISH) {
        static_emit_end_block(s, 1);
        if (s->strm->avail_out == 0)
            return s->strm->avail_in == 0 ? finish_started : need_more;
        else
            return finish_done;
    }

    static_emit_end_block(s, 0);
    return block_done;
}
