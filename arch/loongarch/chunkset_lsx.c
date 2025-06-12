/* chunkset_lsx.c -- LSX inline functions to copy small data chunks, based on Intel SSSE3 implementation
 * Copyright (C) 2025 Vladislav Shchapov <vladislav@shchapov.ru>
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#include "zbuild.h"
#include "zmemory.h"

#if defined(LOONGARCH_LSX)
#include <lsxintrin.h>
#include "lsxintrin_ext.h"
#include "arch/generic/chunk_128bit_perm_idx_lut.h"

typedef __m128i chunk_t;

#define HAVE_CHUNKMEMSET_2
#define HAVE_CHUNKMEMSET_4
#define HAVE_CHUNKMEMSET_8
#define HAVE_CHUNK_MAG


static inline void chunkmemset_2(uint8_t *from, chunk_t *chunk) {
    *chunk = __lsx_vreplgr2vr_h(zng_memread_2(from));
}

static inline void chunkmemset_4(uint8_t *from, chunk_t *chunk) {
    *chunk = __lsx_vreplgr2vr_w(zng_memread_4(from));
}

static inline void chunkmemset_8(uint8_t *from, chunk_t *chunk) {
    *chunk = __lsx_vreplgr2vr_d(zng_memread_8(from));
}

static inline void loadchunk(uint8_t const *s, chunk_t *chunk) {
    *chunk = __lsx_vld(s, 0);
}

static inline void storechunk(uint8_t *out, chunk_t *chunk) {
    __lsx_vst(*chunk, out, 0);
}

static inline chunk_t GET_CHUNK_MAG(uint8_t *buf, uint32_t *chunk_rem, uint32_t dist) {
    lut_rem_pair lut_rem = perm_idx_lut[dist - 3];
    __m128i perm_vec, ret_vec;
    /* Important to note:
     * This is _not_ to subvert the memory sanitizer but to instead unpoison some
     * bytes we willingly and purposefully load uninitialized that we swizzle over
     * in a vector register, anyway.  If what we assume is wrong about what is used,
     * the memory sanitizer will still usefully flag it */
    __msan_unpoison(buf + dist, 16 - dist);
    ret_vec = __lsx_vld(buf, 0);
    *chunk_rem = lut_rem.remval;

    perm_vec = __lsx_vld(permute_table + lut_rem.idx, 0);
    ret_vec = lsx_shuffle_b(ret_vec, perm_vec);

    return ret_vec;
}

#define CHUNKSIZE        chunksize_lsx
#define CHUNKMEMSET      chunkmemset_lsx
#define CHUNKMEMSET_SAFE chunkmemset_safe_lsx
#define CHUNKCOPY        chunkcopy_lsx
#define CHUNKUNROLL      chunkunroll_lsx

#include "chunkset_tpl.h"

#define INFLATE_FAST     inflate_fast_lsx

#include "inffast_tpl.h"

#endif
