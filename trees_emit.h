#ifndef TREES_EMIT_H_
#define TREES_EMIT_H_

#include "zbuild.h"
#include "trees.h"

#ifdef ZLIB_DEBUG
#  include <ctype.h>
#  include <inttypes.h>
#endif


/* trees.h */
extern Z_INTERNAL const ct_data static_ltree[L_CODES+2];
extern Z_INTERNAL const ct_data static_dtree[D_CODES];

extern Z_INTERNAL const unsigned char zng_dist_code[DIST_CODE_LEN];
extern Z_INTERNAL const unsigned char zng_length_code[STD_MAX_MATCH-STD_MIN_MATCH+1];

/* Combined mask + extra_bits tables for single-lookup optimization */
extern Z_INTERNAL const uint16_t lmask_extra[LENGTH_CODES];
extern Z_INTERNAL const uint32_t dmask_extra[D_CODES];

/* Bit buffer and deflate code stderr tracing */
#ifdef ZLIB_DEBUG
#  define trace_bits(s, value, length) { \
        Tracevv((stderr, " l %2d v %4llx ", (int)(length), (long long)(value))); \
        Assert(length > 0 && length <= BIT_BUF_SIZE, "invalid length"); \
    }
#  define trace_code(s, c) \
    if (z_verbose > 2) { \
        fprintf(stderr, "\ncd %3d ", (c)); \
    }
#else
#  define trace_bits(s, value, length)
#  define trace_code(s, c)
#endif

/* If not enough room in bi_buf, use (valid) bits from bi_buf and
 * (64 - bi_valid) bits from value, leaving (width - (64-bi_valid))
 * unused bits in value.
 */
#define send_bits(s, t_val, t_len, bi_buf, bi_valid) do {\
    Assert(bi_valid <= 64, "Too many bits in bi_valid");\
    uint64_t val = (uint64_t)t_val;\
    uint32_t len = (uint32_t)t_len;\
    uint32_t total_bits = bi_valid + len;\
    trace_bits(s, val, len);\
    sent_bits_add(s, len);\
    \
    /* Unconditionally shift and merge values into the buffer */\
    bi_buf |= val << bi_valid;\
    \
    /* Check if the 64-bit boundary was crossed */\
    if (total_bits >= 64) {\
        total_bits -= 64;\
        put_uint64(s, bi_buf);\
        \
        /* Secure shift: prevent Undefined Behavior when bi_valid is 0 */\
        /* If bi_valid is 0, we shift by 0 (via the mask) and overwrite bi_buf completely */\
        bi_buf = (val >> 1) >> (~bi_valid & 63);\
    }\
    bi_valid = total_bits;\
} while (0)

/* Send a code of the given tree. c and tree must not have side effects */
#ifdef ZLIB_DEBUG
#  define send_code(s, c, tree, bi_buf, bi_valid) { \
    trace_code(s, c); \
    send_bits(s, tree[c].Code, tree[c].Len, bi_buf, bi_valid); \
}
#else
#  define send_code(s, c, tree, bi_buf, bi_valid) \
    send_bits(s, tree[c].Code, tree[c].Len, bi_buf, bi_valid)
#endif

/* ===========================================================================
 * Flush the bit buffer and align the output on a byte boundary
 */
static inline void bi_windup(deflate_state *s) {
    if (s->bi_valid > 56) {
        put_uint64(s, s->bi_buf);
    } else {
        if (s->bi_valid > 24) {
            put_uint32(s, (uint32_t)s->bi_buf);
            s->bi_buf >>= 32;
            s->bi_valid -= 32;
        }
        if (s->bi_valid > 8) {
            put_short(s, (uint16_t)s->bi_buf);
            s->bi_buf >>= 16;
            s->bi_valid -= 16;
        }
        if (s->bi_valid > 0) {
            put_byte(s, s->bi_buf);
        }
    }
    s->bi_used = ((s->bi_valid - 1) & 7) + 1;
    s->bi_buf = 0;
    s->bi_valid = 0;
}

/* ===========================================================================
 * Emit literal code
 */
Z_FORCEINLINE static void zng_emit_lit(deflate_state *s, const ct_data *ltree, unsigned c,
                                uint64_t *bi_buf, uint32_t *bi_valid) {
    send_code(s, c, ltree, *bi_buf, *bi_valid);
    Tracecv(isgraph(c & 0xff), (stderr, " '%c' ", c));
}

/* ===========================================================================
 * Emit match distance/length code
 */
static inline uint32_t zng_emit_dist(deflate_state *s, const ct_data *ltree, const ct_data *dtree,
                                     uint32_t lc, uint32_t dist, uint64_t *bi_buf, uint32_t *bi_valid) {
    uint64_t match_bits;
    uint32_t match_bits_len;
    uint32_t mask_ext;  // Contains both mask and extra, can safely be used directly as mask
                        // due to extra bits being outside the range of lc and dist data.
    uint32_t c, extra;
    uint8_t code;

    /* 1. Process Length Code */
    code = zng_length_code[lc];
    c = code + LITERALS + 1;
    Assert(c < L_CODES, "bad l_code");
    trace_code(s, c);

    /*    Send length code, len is the match length - STD_MIN_MATCH */
    match_bits = ltree[c].Code;
    match_bits_len = ltree[c].Len;

    /* 2. Get extra bits count and mask */
    mask_ext = lmask_extra[code];
    extra = mask_ext >> 8;

    /*    Send length extra bits */
    match_bits |= (uint64_t)(lc & mask_ext) << match_bits_len;
    match_bits_len += extra;

    /* 3. Process Distance Code */
    dist--; /* dist is now the match distance - 1 */
    code = d_code(dist);
    Assert(code < D_CODES, "bad d_code");
    trace_code(s, code);

    /*    Send distance code */
    match_bits |= ((uint64_t)dtree[code].Code << match_bits_len);
    match_bits_len += dtree[code].Len;

    /* 4. Get extra bits count and mask */
    mask_ext = dmask_extra[code];
    extra = mask_ext >> 16;

    /*    Send dist extra bits */
    match_bits |= ((uint64_t)(dist & mask_ext) << match_bits_len);
    match_bits_len += extra;

    send_bits(s, match_bits, match_bits_len, *bi_buf, *bi_valid);

    return match_bits_len;
}

/* ===========================================================================
 * Emit end block
 */
static inline void zng_emit_end_block(deflate_state *s, const ct_data *ltree, const int last,
                                      uint64_t *bi_buf, uint32_t *bi_valid) {
    send_code(s, END_BLOCK, ltree, *bi_buf, *bi_valid);
    Tracev((stderr, "\n+++ Emit End Block: Last: %u Pending: %u Total Out: %" PRIu64 "\n",
        last, s->pending, (uint64_t)s->strm->total_out));
    Z_UNUSED(last);
}

/* ===========================================================================
 * Emit literal and count bits
 */
static inline void zng_tr_emit_lit(deflate_state *s, const ct_data *ltree, unsigned c) {
    uint64_t bi_buf = s->bi_buf;
    uint32_t bi_valid = s->bi_valid;
    zng_emit_lit(s, ltree, c, &bi_buf, &bi_valid);
    s->bi_buf = bi_buf;
    s->bi_valid = bi_valid;
    cmpr_bits_add(s, ltree[c].Len);
}

/* ===========================================================================
 * Emit match and count bits
 */
static inline void zng_tr_emit_dist(deflate_state *s, const ct_data *ltree, const ct_data *dtree,
    uint32_t lc, uint32_t dist) {
    uint64_t bi_buf = s->bi_buf;
    uint32_t bi_valid = s->bi_valid;
    uint32_t bits = zng_emit_dist(s, ltree, dtree, lc, dist, &bi_buf, &bi_valid);
    s->bi_buf = bi_buf;
    s->bi_valid = bi_valid;
    cmpr_bits_add(s, bits);
}

/* ===========================================================================
 * Emit start of block
 */
static inline void zng_tr_emit_tree(deflate_state *s, int type, const int last) {
    uint32_t bi_valid = s->bi_valid;
    uint64_t bi_buf = s->bi_buf;
    uint32_t header_bits = (type << 1) + last;
    send_bits(s, header_bits, 3, bi_buf, bi_valid);
    cmpr_bits_add(s, 3);
    s->bi_valid = bi_valid;
    s->bi_buf = bi_buf;
    Tracev((stderr, "\n--- Emit Tree: Last: %u\n", last));
}

/* ===========================================================================
 * Align bit buffer on a byte boundary and count bits
 */
static inline void zng_tr_emit_align(deflate_state *s) {
    bi_windup(s); /* align on byte boundary */
    sent_bits_align(s);
}

/* ===========================================================================
 * Emit an end block and align bit buffer if last block
 */
static inline void zng_tr_emit_end_block(deflate_state *s, const ct_data *ltree, const int last) {
    uint64_t bi_buf = s->bi_buf;
    uint32_t bi_valid = s->bi_valid;
    zng_emit_end_block(s, ltree, last, &bi_buf, &bi_valid);
    s->bi_buf = bi_buf;
    s->bi_valid = bi_valid;
    cmpr_bits_add(s, 7);
    if (last)
        zng_tr_emit_align(s);
}

#endif
