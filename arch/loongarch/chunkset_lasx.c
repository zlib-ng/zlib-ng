/* chunkset_lasx.c -- LASX inline functions to copy small data chunks, based on Intel AVX2 implementation
 * Copyright (C) 2025 Vladislav Shchapov <vladislav@shchapov.ru>
 * For conditions of distribution and use, see copyright notice in zlib.h
 */
#include "zbuild.h"
#include "zmemory.h"

#ifdef LOONGARCH_LASX

#include <lasxintrin.h>
#include "lasxintrin_ext.h"
#include "lsxintrin_ext.h"

#include "arch/generic/chunk_256bit_perm_idx_lut.h"

typedef __m256i chunk_t;
typedef __m128i halfchunk_t;

#define HAVE_CHUNKMEMSET_2
#define HAVE_CHUNKMEMSET_4
#define HAVE_CHUNKMEMSET_8
#define HAVE_CHUNKMEMSET_16
#define HAVE_CHUNK_MAG
#define HAVE_HALF_CHUNK

static inline void chunkmemset_2(uint8_t *from, chunk_t *chunk) {
    *chunk = __lasx_xvreplgr2vr_h(zng_memread_2(from));
}

static inline void chunkmemset_4(uint8_t *from, chunk_t *chunk) {
    *chunk = __lasx_xvreplgr2vr_w(zng_memread_4(from));
}

static inline void chunkmemset_8(uint8_t *from, chunk_t *chunk) {
    *chunk = __lasx_xvreplgr2vr_d(zng_memread_8(from));
}

static inline void chunkmemset_16(uint8_t *from, chunk_t *chunk) {
    *chunk = lasx_broadcastsi128_si256(__lsx_vld(from, 0));
}

static inline void loadchunk(uint8_t const *s, chunk_t *chunk) {
    *chunk = __lasx_xvld(s, 0);
}

static inline void storechunk(uint8_t *out, chunk_t *chunk) {
    __lasx_xvst(*chunk, out, 0);
}

static inline chunk_t GET_CHUNK_MAG(uint8_t *buf, uint32_t *chunk_rem, uint32_t dist) {
    lut_rem_pair lut_rem = perm_idx_lut[dist - 3];
    __m256i ret_vec;
    /* While technically we only need to read 4 or 8 bytes into this vector register for a lot of cases, GCC is
     * compiling this to a shared load for all branches, preferring the simpler code.  Given that the buf value isn't in
     * GPRs to begin with the 256 bit load is _probably_ just as inexpensive */
    *chunk_rem = lut_rem.remval;

    /* See note in chunkset_ssse3.c for why this is ok */
    __msan_unpoison(buf + dist, 32 - dist);

    if (dist < 16) {
        /* This simpler case still requires us to shuffle in 128 bit lanes, so we must apply a static offset after
         * broadcasting the first vector register to both halves. This is _marginally_ faster than doing two separate
         * shuffles and combining the halves later */
        const __m256i permute_xform = lasx_set_si128(__lsx_vreplgr2vr_b(16), __lsx_vreplgr2vr_b(0));
        __m256i perm_vec = __lasx_xvld(permute_table+lut_rem.idx, 0);
        __m128i ret_vec0 = __lsx_vld(buf, 0);
        perm_vec = __lasx_xvadd_b(perm_vec, permute_xform);
        ret_vec = lasx_set_si128(ret_vec0, ret_vec0);
        ret_vec = lasx_shuffle_b(ret_vec, perm_vec);
    }  else {
        __m128i ret_vec0 = __lsx_vld(buf, 0);
        __m128i ret_vec1 = __lsx_vld(buf, 16);
        /* Take advantage of the fact that only the latter half of the 256 bit vector will actually differ */
        __m128i perm_vec1 = __lsx_vld(permute_table + lut_rem.idx, 0);
        __m128i xlane_permutes = __lsx_vslt_b(perm_vec1, __lsx_vreplgr2vr_b(16));
        __m128i xlane_res  = lsx_shuffle_b(ret_vec0, perm_vec1);
        /* Since we can't wrap twice, we can simply keep the later half exactly how it is instead of having to _also_
         * shuffle those values */
        __m128i latter_half = __lsx_vbitsel_v(ret_vec1, xlane_res, xlane_permutes);
        ret_vec = lasx_set_si128(latter_half, ret_vec0);
    }

    return ret_vec;
}

static inline void loadhalfchunk(uint8_t const *s, halfchunk_t *chunk) {
    *chunk = __lsx_vld(s, 0);
}

static inline void storehalfchunk(uint8_t *out, halfchunk_t *chunk) {
    __lsx_vst(*chunk, out, 0);
}

static inline chunk_t halfchunk2whole(halfchunk_t *chunk) {
    /* We zero extend mostly to appease some memory sanitizers. These bytes are ultimately
     * unlikely to be actually written or read from */
    return lasx_zextsi128_si256(*chunk);
}

static inline halfchunk_t GET_HALFCHUNK_MAG(uint8_t *buf, uint32_t *chunk_rem, uint32_t dist) {
    lut_rem_pair lut_rem = perm_idx_lut[dist - 3];
    __m128i perm_vec, ret_vec;
    __msan_unpoison(buf + dist, 16 - dist);
    ret_vec = __lsx_vld(buf, 0);
    *chunk_rem = half_rem_vals[dist - 3];

    perm_vec = __lsx_vld(permute_table + lut_rem.idx, 0);
    ret_vec = lsx_shuffle_b(ret_vec, perm_vec);

    return ret_vec;
}

#define CHUNKSIZE        chunksize_lasx
#define CHUNKCOPY        chunkcopy_lasx
#define CHUNKUNROLL      chunkunroll_lasx
#define CHUNKMEMSET      chunkmemset_lasx
#define CHUNKMEMSET_SAFE chunkmemset_safe_lasx

#include "chunkset_tpl.h"

#define INFLATE_FAST     inflate_fast_lasx

#include "inffast_tpl.h"

#endif
