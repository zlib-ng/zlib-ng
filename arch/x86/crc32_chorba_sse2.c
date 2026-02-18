#include "zbuild.h"
#include "arch_functions.h"

#if defined(X86_SSE2) && defined(CRC32_CHORBA_SSE_FALLBACK)

#include "crc32_chorba_p.h"
#include "crc32_braid_p.h"
#include "crc32_braid_tbl.h"
#include <emmintrin.h>
#include "arch/x86/x86_intrins.h"

#define LSHIFT_QWORD(x)     _mm_unpacklo_epi64(_mm_setzero_si128(), (x))
#define RSHIFT_QWORD(x)     _mm_unpackhi_epi64((x), _mm_setzero_si128())
#define ALIGNR_QWORD(a, b)  _mm_castpd_si128(_mm_shuffle_pd(_mm_castsi128_pd(a), _mm_castsi128_pd(b), 1))

#define READ_NEXT(in, off, a, b) \
    do { \
        a = _mm_load_si128((__m128i*)(in + off / sizeof(uint64_t))); \
        b = _mm_load_si128((__m128i*)(in + off / sizeof(uint64_t) + 2)); \
    } while (0)

#define NEXT_ROUND(invec, a, b, c, d) \
    do { \
        a = _mm_xor_si128(_mm_slli_epi64(invec, 17), _mm_slli_epi64(invec, 55)); \
        b = _mm_xor_si128(_mm_xor_si128(_mm_srli_epi64(invec, 47), _mm_srli_epi64(invec, 9)), _mm_slli_epi64(invec, 19)); \
        c = _mm_xor_si128(_mm_srli_epi64(invec, 45), _mm_slli_epi64(invec, 44)); \
        d = _mm_srli_epi64(invec, 20); \
    } while (0)

#define ACCUM_ROUND() \
    do { \
        __m128i a4_ = _mm_unpacklo_epi64(next56, ab4); \
        __m128i b2c2 = ALIGNR_QWORD(ab2, cd2); \
        __m128i b4c4 = ALIGNR_QWORD(ab4, cd4); \
        __m128i d2_ = RSHIFT_QWORD(cd2); \
        next12 = _mm_xor_si128(_mm_xor_si128(a4_, ab3), cd1); \
        next12 = _mm_xor_si128(next12, b2c2); \
        next34 = _mm_xor_si128(b4c4, cd3); \
        next34 = _mm_xor_si128(next34, d2_); \
        next56 = RSHIFT_QWORD(cd4); \
    } while (0)

Z_INTERNAL uint32_t chorba_small_nondestructive_sse2(uint32_t crc, const uint8_t *buf, size_t len) {
    /* The calling function ensured that this is aligned correctly */
    const uint64_t* input = (const uint64_t*)buf;
    ALIGNED_(16) uint64_t final[9] = {0};
    uint64_t next1 = ~crc;
    crc = 0;
    uint64_t next2, next3, next4, next5;

    __m128i next12 = _mm_cvtsi64_si128(next1);
    __m128i next34 = _mm_setzero_si128();
    __m128i next56 = _mm_setzero_si128();
    __m128i ab1, ab2, ab3, ab4, cd1, cd2, cd3, cd4;

    size_t i = 0;

    /* This is weird, doing for vs while drops 10% off the exec time */
    for (; (i + 256 + 40 + 32 + 32) < len; i += 32) {
        __m128i in1in2, in3in4;

        const uint64_t *input_ptr = input + (i / sizeof(uint64_t));
        const __m128i *input_ptr_128 = (__m128i*)input_ptr;
        __m128i chorba12 = _mm_load_si128(input_ptr_128++);
        __m128i chorba34 = _mm_load_si128(input_ptr_128++);
        __m128i chorba56 = _mm_load_si128(input_ptr_128++);
        __m128i chorba78 = _mm_load_si128(input_ptr_128++);

        chorba12 = _mm_xor_si128(chorba12, next12);
        chorba34 = _mm_xor_si128(chorba34, next34);
        chorba56 = _mm_xor_si128(chorba56, next56);
        chorba78 = _mm_xor_si128(chorba78, chorba12);
        __m128i chorba45 = ALIGNR_QWORD(chorba34, chorba56);
        __m128i chorba23 = ALIGNR_QWORD(chorba12, chorba34);

        i += 8 * 8;

        /* 0-3 */
        READ_NEXT(input, i, in1in2, in3in4);

        __m128i chorba34xor = _mm_xor_si128(chorba34, LSHIFT_QWORD(chorba12));
        in1in2 = _mm_xor_si128(in1in2, chorba34xor);

        NEXT_ROUND(in1in2, ab1, ab2, ab3, ab4);

        in3in4 = _mm_xor_si128(in3in4, ab1);
        /* _hopefully_ we don't get a huge domain switching penalty for this. This seems to be the best sequence */
        __m128i chorba56xor = _mm_xor_si128(chorba56, LSHIFT_QWORD(ab2));

        in3in4 = _mm_xor_si128(in3in4, _mm_xor_si128(chorba56xor, chorba23));
        in3in4 = _mm_xor_si128(in3in4, chorba12);

        NEXT_ROUND(in3in4, cd1, cd2, cd3, cd4);

        /* chorba56 already consumed next56, clear it so ACCUM_ROUND
           does not xor the stale value into next12 */
        next56 = _mm_setzero_si128();
        ACCUM_ROUND();

        i += 32;

        /* 4-7 */
        READ_NEXT(input, i, in1in2, in3in4);

        in1in2 = _mm_xor_si128(in1in2, next12);
        in1in2 = _mm_xor_si128(in1in2, chorba78);
        in1in2 = _mm_xor_si128(in1in2, chorba45);
        in1in2 = _mm_xor_si128(in1in2, chorba34);

        NEXT_ROUND(in1in2, ab1, ab2, ab3, ab4);

        in3in4 = _mm_xor_si128(in3in4, next34);
        in3in4 = _mm_xor_si128(in3in4, ab1);
        in3in4 = _mm_xor_si128(in3in4, chorba56);
        __m128i chorba67 = ALIGNR_QWORD(chorba56, chorba78);
        in3in4 = _mm_xor_si128(in3in4, _mm_xor_si128(chorba67, LSHIFT_QWORD(ab2)));

        NEXT_ROUND(in3in4, cd1, cd2, cd3, cd4);

        ACCUM_ROUND();

        i += 32;

        /* 8-11 */
        READ_NEXT(input, i, in1in2, in3in4);

        __m128i chorba80 = RSHIFT_QWORD(chorba78);
        __m128i next12_chorba12 = _mm_xor_si128(next12, chorba12);
        in1in2 = _mm_xor_si128(in1in2, chorba80);
        in1in2 = _mm_xor_si128(in1in2, chorba78);
        in1in2 = _mm_xor_si128(in1in2, next12_chorba12);

        NEXT_ROUND(in1in2, ab1, ab2, ab3, ab4);

        in3in4 = _mm_xor_si128(next34, in3in4);
        in3in4 = _mm_xor_si128(in3in4, ab1);
        __m128i a2_ = LSHIFT_QWORD(ab2);
        in3in4 = _mm_xor_si128(in3in4, chorba34);
        in3in4 = _mm_xor_si128(in3in4, a2_);

        NEXT_ROUND(in3in4, cd1, cd2, cd3, cd4);

        ACCUM_ROUND();

        i += 32;

        /* 12-15 */
        READ_NEXT(input, i, in1in2, in3in4);

        in1in2 = _mm_xor_si128(in1in2, next12);
        __m128i chorb56xorchorb12 = _mm_xor_si128(chorba56, chorba12);
        in1in2 = _mm_xor_si128(in1in2, chorb56xorchorb12);
        __m128i chorb1_ = LSHIFT_QWORD(chorba12);
        in1in2 = _mm_xor_si128(in1in2, chorb1_);

        NEXT_ROUND(in1in2, ab1, ab2, ab3, ab4);

        in3in4 = _mm_xor_si128(next34, in3in4);
        in3in4 = _mm_xor_si128(in3in4, _mm_xor_si128(ab1, chorba78));
        in3in4 = _mm_xor_si128(in3in4, _mm_xor_si128(chorba34, chorba12));
        in3in4 = _mm_xor_si128(in3in4, _mm_xor_si128(chorba23, LSHIFT_QWORD(ab2)));

        NEXT_ROUND(in3in4, cd1, cd2, cd3, cd4);

        ACCUM_ROUND();

        i += 32;

        /* 16-19 */
        READ_NEXT(input, i, in1in2, in3in4);

        __m128i chorba1_ = LSHIFT_QWORD(chorba12);
        in1in2 = _mm_xor_si128(_mm_xor_si128(next12, in1in2), _mm_xor_si128(chorba56, chorba45));
        in1in2 = _mm_xor_si128(in1in2, _mm_xor_si128(chorba12, chorba34));
        in1in2 = _mm_xor_si128(chorba1_, in1in2);

        NEXT_ROUND(in1in2, ab1, ab2, ab3, ab4);

        a2_ = LSHIFT_QWORD(ab2);
        in3in4 = _mm_xor_si128(in3in4, _mm_xor_si128(ab1, chorba78));
        in3in4 = _mm_xor_si128(in3in4, _mm_xor_si128(chorba56, chorba34));
        in3in4 = _mm_xor_si128(in3in4, _mm_xor_si128(chorba23, chorba67));
        in3in4 = _mm_xor_si128(in3in4, _mm_xor_si128(chorba1_, a2_));
        in3in4 = _mm_xor_si128(in3in4, next34);

        NEXT_ROUND(in3in4, cd1, cd2, cd3, cd4);

        ACCUM_ROUND();

        i += 32;

        /* 20-23 */
        READ_NEXT(input, i, in1in2, in3in4);

        in1in2 = _mm_xor_si128(in1in2, _mm_xor_si128(next12, chorba78));
        in1in2 = _mm_xor_si128(in1in2, _mm_xor_si128(chorba45, chorba56));
        in1in2 = _mm_xor_si128(in1in2, _mm_xor_si128(chorba23, chorba12));
        in1in2 = _mm_xor_si128(in1in2, chorba80);

        NEXT_ROUND(in1in2, ab1, ab2, ab3, ab4);

        a2_ = LSHIFT_QWORD(ab2);
        in3in4 = _mm_xor_si128(in3in4, _mm_xor_si128(next34, ab1));
        in3in4 = _mm_xor_si128(in3in4, _mm_xor_si128(chorba78, chorba67));
        in3in4 = _mm_xor_si128(in3in4, _mm_xor_si128(chorba45, chorba34));
        in3in4 = _mm_xor_si128(in3in4, _mm_xor_si128(chorba1_, a2_));
        in3in4 = _mm_xor_si128(in3in4, chorba12);

        NEXT_ROUND(in3in4, cd1, cd2, cd3, cd4);

        ACCUM_ROUND();

        i += 32;

        /* 24-27 */
        READ_NEXT(input, i, in1in2, in3in4);

        in1in2 = _mm_xor_si128(in1in2, _mm_xor_si128(next12, chorba67));
        in1in2 = _mm_xor_si128(in1in2, _mm_xor_si128(chorba56, chorba34));
        in1in2 = _mm_xor_si128(in1in2, _mm_xor_si128(chorba23, chorba12));
        in1in2 = _mm_xor_si128(in1in2, chorba80);

        NEXT_ROUND(in1in2, ab1, ab2, ab3, ab4);

        a2_ = LSHIFT_QWORD(ab2);
        in3in4 = _mm_xor_si128(in3in4, _mm_xor_si128(next34, ab1));
        in3in4 = _mm_xor_si128(in3in4, _mm_xor_si128(chorba78, chorba56));
        in3in4 = _mm_xor_si128(in3in4, _mm_xor_si128(chorba45, chorba34));
        in3in4 = _mm_xor_si128(in3in4, _mm_xor_si128(chorba80, a2_));

        NEXT_ROUND(in3in4, cd1, cd2, cd3, cd4);

        ACCUM_ROUND();

        i += 32;

        /* 28-31 */
        READ_NEXT(input, i, in1in2, in3in4);

        in1in2 = _mm_xor_si128(in1in2, _mm_xor_si128(next12, chorba78));
        in1in2 = _mm_xor_si128(in1in2, _mm_xor_si128(chorba67, chorba56));

        NEXT_ROUND(in1in2, ab1, ab2, ab3, ab4);

        a2_ = LSHIFT_QWORD(ab2);
        in3in4 = _mm_xor_si128(in3in4, _mm_xor_si128(next34, ab1));
        in3in4 = _mm_xor_si128(in3in4, _mm_xor_si128(chorba78, chorba80));
        in3in4 = _mm_xor_si128(a2_, in3in4);

        NEXT_ROUND(in3in4, cd1, cd2, cd3, cd4);

        ACCUM_ROUND();
    }

    for (; (i + 40 + 32) < len; i += 32) {
        __m128i in1in2, in3in4;

        READ_NEXT(input, i, in1in2, in3in4);

        in1in2 = _mm_xor_si128(in1in2, next12);

        NEXT_ROUND(in1in2, ab1, ab2, ab3, ab4);

        __m128i a2_ = LSHIFT_QWORD(ab2);
        __m128i ab1_next34 = _mm_xor_si128(next34, ab1);
        in3in4 = _mm_xor_si128(in3in4, ab1_next34);
        in3in4 = _mm_xor_si128(a2_, in3in4);

        NEXT_ROUND(in3in4, cd1, cd2, cd3, cd4);

        ACCUM_ROUND();
    }

    next1 = _mm_cvtsi128_si64(next12);
    next2 = _mm_cvtsi128_si64(_mm_unpackhi_epi64(next12, next12));
    next3 = _mm_cvtsi128_si64(next34);
    next4 = _mm_cvtsi128_si64(_mm_unpackhi_epi64(next34, next34));
    next5 = _mm_cvtsi128_si64(next56);

    /* Skip the call to memcpy */
    size_t copy_len = len - i;
    __m128i *final128 = (__m128i*)final;
    __m128i *input128 = (__m128i*)(input + (i / sizeof(uint64_t)));
    while (copy_len >= 64) {
        _mm_store_si128(final128++, _mm_load_si128(input128++));
        _mm_store_si128(final128++, _mm_load_si128(input128++));
        _mm_store_si128(final128++, _mm_load_si128(input128++));
        _mm_store_si128(final128++, _mm_load_si128(input128++));
        copy_len -= 64;
    }

    while (copy_len >= 16) {
        _mm_store_si128(final128++, _mm_load_si128(input128++));
        copy_len -= 16;
    }

    uint8_t *src_bytes = (uint8_t*)input128;
    uint8_t *dst_bytes = (uint8_t*)final128;
    while (copy_len--) {
       *dst_bytes++ = *src_bytes++;
    }

    final[0] ^= next1;
    final[1] ^= next2;
    final[2] ^= next3;
    final[3] ^= next4;
    final[4] ^= next5;

    /* We perform the same loop that braid_internal is doing but we'll skip
     * the function call for this tiny tail */
    uint8_t *final_bytes = (uint8_t*)final;
    size_t rem = len - i;

    while (rem--) {
        crc = crc_table[(crc ^ *final_bytes++) & 0xff] ^ (crc >> 8);
    }

    return ~crc;
}

Z_INTERNAL uint32_t crc32_chorba_sse2(uint32_t crc, const uint8_t *buf, size_t len) {
    uintptr_t align_diff = ALIGN_DIFF(buf, 16);
    if (len <= align_diff + CHORBA_SMALL_THRESHOLD_64BIT)
        return crc32_braid(crc, buf, len);

    if (align_diff) {
        crc = crc32_braid(crc, buf, align_diff);
        len -= align_diff;
        buf += align_diff;
    }
#ifdef CRC32_CHORBA_FALLBACK
    if (len > CHORBA_LARGE_THRESHOLD)
        return crc32_chorba_118960_nondestructive(crc, buf, len);
#endif
    return chorba_small_nondestructive_sse2(crc, buf, len);
}

Z_INTERNAL uint32_t crc32_copy_chorba_sse2(uint32_t crc, uint8_t *dst, const uint8_t *src, size_t len) {
    crc = crc32_chorba_sse2(crc, src, len);
    memcpy(dst, src, len);
    return crc;
}
#endif
