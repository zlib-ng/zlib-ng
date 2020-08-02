/* chunkset_neon.c -- NEON inline functions to copy small data chunks.
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#ifdef ARM_NEON_CHUNKSET
#ifdef _M_ARM64
#  include <arm64_neon.h>
#else
#  include <arm_neon.h>
#endif
#include "../../zbuild.h"
#include "../../zutil.h"

typedef uint8x16_t chunk_t;

#define HAVE_CHUNKMEMSET_1
#define HAVE_CHUNKMEMSET_2
#define HAVE_CHUNKMEMSET_3
#define HAVE_CHUNKMEMSET_4
#define HAVE_CHUNKMEMSET_8

static inline void chunkmemset_1(uint8_t *from, chunk_t *chunk) {
    *chunk = vld1q_dup_u8(from);
}

static inline void chunkmemset_2(uint8_t *from, chunk_t *chunk) {
    *chunk = vreinterpretq_u8_s16(vdupq_n_s16(*(int16_t *)from));
}

static inline void chunkmemset_4(uint8_t *from, chunk_t *chunk) {
    *chunk = vreinterpretq_u8_s32(vdupq_n_s32(*(int32_t *)from));
}

static inline void chunkmemset_8(uint8_t *from, chunk_t *chunk) {
    *chunk = vcombine_u8(vld1_u8(from), vld1_u8(from));
}

#define CHUNKSIZE        chunksize_neon
#define CHUNKCOPY        chunkcopy_neon
#define CHUNKCOPY_SAFE   chunkcopy_safe_neon
#define CHUNKUNROLL      chunkunroll_neon
#define CHUNKMEMSET      chunkmemset_neon
#define CHUNKMEMSET_SAFE chunkmemset_safe_neon

uint8_t* CHUNKCOPY(uint8_t *out, uint8_t const *from, unsigned len);
uint8_t* CHUNKUNROLL(uint8_t *out, unsigned *dist, unsigned *len);

static inline uint8_t *chunkmemset_3(uint8_t *out, uint8_t *from, unsigned dist, unsigned len) {
    uint8x8x3_t chunks;
    unsigned sz = sizeof(chunks);
    if (len < sz) {
        out = CHUNKUNROLL(out, &dist, &len);
        return CHUNKCOPY(out, out - dist, len);
    }

    /* Load 3 bytes 'a,b,c' from FROM and duplicate across all lanes:
       chunks[0] = {a,a,a,a,a,a,a,a}
       chunks[1] = {b,b,b,b,b,b,b,b}
       chunks[2] = {c,c,c,c,c,c,c,c}. */
    chunks = vld3_dup_u8(from);

    unsigned rem = len % sz;
    len -= rem;
    while (len) {
        /* Store "a,b,c, ..., a,b,c". */
        vst3_u8(out, chunks);
        out += sz;
        len -= sz;
    }

    if (!rem)
        return out;

    /* Last, deal with the case when LEN is not a multiple of SZ. */
    out = CHUNKUNROLL(out, &dist, &rem);
    return CHUNKCOPY(out, out - dist, rem);
}

#if defined(__aarch64__) || defined(_M_ARM64)

#define HAVE_CHUNKMEMSET_6

static inline uint8_t *chunkmemset_6(uint8_t *out, uint8_t *from, unsigned dist, unsigned len) {
    uint16x8x3_t chunks;
    unsigned sz = sizeof(chunks);
    if (len < sz) {
        out = CHUNKUNROLL(out, &dist, &len);
        return CHUNKCOPY(out, out - dist, len);
    }

    /* Load 6 bytes 'ab,cd,ef' from FROM and duplicate across all lanes:
       chunks[0] = {ab,ab,ab,ab,ab,ab,ab,ab}
       chunks[1] = {cd,cd,cd,cd,cd,cd,cd,cd}
       chunks[2] = {ef,ef,ef,ef,ef,ef,ef,ef}. */
    chunks = vld3q_dup_u16((unsigned short *)from);

    unsigned rem = len % sz;
    len -= rem;
    while (len) {
        /* Store "ab,cd,ef, ..., ab,cd,ef". */
        vst3q_u16((unsigned short *)out, chunks);
        out += sz;
        len -= sz;
    }

    if (!rem)
        return out;

    /* Last, deal with the case when LEN is not a multiple of SZ. */
    out = CHUNKUNROLL(out, &dist, &rem);
    return CHUNKCOPY(out, out - dist, rem);
}

#endif

static inline void loadchunk(uint8_t const *s, chunk_t *chunk) {
    *chunk = vld1q_u8(s);
}

static inline void storechunk(uint8_t *out, chunk_t *chunk) {
    vst1q_u8(out, *chunk);
}

#include "chunkset_tpl.h"

#endif
