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
#define HAVE_CHUNKMEMSET_4
#define HAVE_CHUNKMEMSET_8

static inline void chunkmemset_1(uint8_t *from, chunk_t *chunk) {
    *chunk = vld1q_dup_u8(from);
}

static inline void chunkmemset_2(uint8_t *from, chunk_t *chunk) {
    int16_t tmp;
    memcpy(&tmp, from, 2);
    *chunk = vreinterpretq_u8_s16(vdupq_n_s16(tmp));
}

static inline void chunkmemset_4(uint8_t *from, chunk_t *chunk) {
    int32_t tmp;
    memcpy(&tmp, from, 4);
    *chunk = vreinterpretq_u8_s32(vdupq_n_s32(tmp));
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

static inline void loadchunk(uint8_t const *s, chunk_t *chunk) {
    *chunk = vld1q_u8(s);
}

static inline void storechunk(uint8_t *out, chunk_t *chunk) {
    vst1q_u8(out, *chunk);
}

#include "chunkset_tpl.h"

#endif
