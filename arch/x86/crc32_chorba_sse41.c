#include "zbuild.h"
#include "arch_functions.h"

#if defined(X86_SSE41) && defined(CRC32_CHORBA_SSE_FALLBACK)

#include "crc32_chorba_p.h"
#include "crc32_p.h"
#include <emmintrin.h>
#include <smmintrin.h>
#include "arch/x86/x86_intrins.h"

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
        d  = _mm_srli_epi64(invec, 20); \
    } while (0)

#define REALIGN_CHORBA(in0, in1, in2, in3, out0, out1, out2, out3, out4, shift) \
    do { \
        out0 = _mm_slli_si128(in0, shift); \
        out1 = _mm_alignr_epi8(in1, in0, shift); \
        out2 = _mm_alignr_epi8(in2, in1, shift); \
        out3 = _mm_alignr_epi8(in3, in2, shift); \
        out4 = _mm_srli_si128(in3, shift); \
    } while (0)

#define STORE4(out0, out1, out2, out3, out) \
    do { \
        _mm_store_si128(out++, out0); \
        _mm_store_si128(out++, out1); \
        _mm_store_si128(out++, out2); \
        _mm_store_si128(out++, out3); \
    } while (0)

#define READ4(out0, out1, out2, out3, in) \
    do { \
        out0 = _mm_load_si128(in++); \
        out1 = _mm_load_si128(in++); \
        out2 = _mm_load_si128(in++); \
        out3 = _mm_load_si128(in++); \
    } while (0)

/* This is intentionally shifted one down to compensate for the deferred store from
 * the last iteration */
#define READ4_WITHXOR(out0, out1, out2, out3, xor0, xor1, xor2, xor3, in) \
    do { \
        out0 = _mm_xor_si128(in[1], xor0); \
        out1 = _mm_xor_si128(in[2], xor1); \
        out2 = _mm_xor_si128(in[3], xor2); \
        out3 = _mm_xor_si128(in[4], xor3); \
    } while (0)

/* Stores into a lane that hasn't been written yet, the last vector carries into the next round */
#define STORE_LANE(carry, out) \
    do { \
        __m128i first = _mm_xor_si128(carry, in_1); \
        STORE4(first, in23, in45, in67, out); \
        carry = in8_; \
    } while (0)

/* Same, but XORs into what earlier rounds already stored in the lane */
#define STORE_LANE_XOR(carry, out) \
    do { \
        __m128i first = _mm_xor_si128(carry, in_1), x1, x2, x3; \
        READ4_WITHXOR(x1, x2, x3, carry, in23, in45, in67, in8_, out); \
        STORE4(first, x1, x2, x3, out); \
    } while (0)

Z_FORCEINLINE static uint32_t crc32_chorba_32768_nondestructive_sse41(uint32_t crc, const uint8_t *buf, size_t len) {
    /* The calling function ensured that this is aligned correctly */
    const uint64_t* input = (const uint64_t*)buf;
    ALIGNED_(16) uint64_t bitbuffer[32768 / sizeof(uint64_t)];
    __m128i *bitbuffer_v = (__m128i*)bitbuffer;
    __m128i z = _mm_setzero_si128();

    __m128i *bitbuf128 = &bitbuffer_v[64];
    __m128i *bitbuf144 = &bitbuffer_v[72];
    __m128i *bitbuf182 = &bitbuffer_v[91];
    __m128i *bitbuf210 = &bitbuffer_v[105];
    __m128i *bitbuf300 = &bitbuffer_v[150];
    __m128i *bitbuf0 = bitbuf128;
    __m128i *inptr = (__m128i*)input;

    /* We only need to zero out the bytes between the 128'th value and the 144th
     * that are actually read */
    __m128i *z_cursor = bitbuf128;
    for (size_t i = 0; i < 2; ++i) {
        STORE4(z, z, z, z, z_cursor);
    }

    /* We only need to zero out the bytes between the 144'th value and the 182nd that
     * are actually read */
    z_cursor = bitbuf144 + 8;
    for (size_t i = 0; i < 11; ++i) {
        _mm_store_si128(z_cursor++, z);
    }

    /* We only need to zero out the bytes between the 182nd value and the 210th that
     * are actually read. */
    z_cursor = bitbuf182;
    for (size_t i = 0; i < 4; ++i) {
        STORE4(z, z, z, z, z_cursor);
    }

    /* We need to mix this in */
    __m128i init_crc = _mm_cvtsi64_si128(~crc);

    size_t i = 0;

    /* Previous iteration runs carried over */
    __m128i buf144 = z;
    __m128i buf182 = z;
    __m128i buf210 = z;

    for (; i + 300*8+64 < len && i < 22 * 8; i += 64) {
        __m128i in12, in34, in56, in78,
                in_1, in23, in45, in67, in8_;

        READ4(in12, in34, in56, in78, inptr);

        if (i == 0) {
            in12 = _mm_xor_si128(in12, init_crc);
        }

        REALIGN_CHORBA(in12, in34, in56, in78,
                       in_1, in23, in45, in67, in8_, 8);

        STORE_LANE(buf144, bitbuf144);
        STORE_LANE(buf182, bitbuf182);
        STORE_LANE(buf210, bitbuf210);
        STORE4(in12, in34, in56, in78, bitbuf300);
    }

    for (; i + 300*8+64 < len && i < 32 * 8; i += 64) {
        __m128i in12, in34, in56, in78,
                in_1, in23, in45, in67, in8_;
        READ4(in12, in34, in56, in78, inptr);

        REALIGN_CHORBA(in12, in34, in56, in78,
                       in_1, in23, in45, in67, in8_, 8);

        STORE_LANE(buf144, bitbuf144);
        STORE_LANE_XOR(buf182, bitbuf182);
        STORE_LANE(buf210, bitbuf210);
        STORE4(in12, in34, in56, in78, bitbuf300);
    }

    for (; i + 300*8+64 < len && i < 84 * 8; i += 64) {
        __m128i in12, in34, in56, in78,
                in_1, in23, in45, in67, in8_;
        READ4(in12, in34, in56, in78, inptr);

        REALIGN_CHORBA(in12, in34, in56, in78,
                       in_1, in23, in45, in67, in8_, 8);

        STORE_LANE_XOR(buf144, bitbuf144);
        STORE_LANE_XOR(buf182, bitbuf182);
        STORE_LANE(buf210, bitbuf210);
        STORE4(in12, in34, in56, in78, bitbuf300);
    }

    for (; i + 300*8+64 < len; i += 64) {
        __m128i in12, in34, in56, in78,
                in_1, in23, in45, in67, in8_;

        if (i < 128 * 8) {
            READ4(in12, in34, in56, in78, inptr);
        } else {
            in12 = _mm_xor_si128(_mm_load_si128(inptr++), _mm_load_si128(bitbuf0++));
            in34 = _mm_xor_si128(_mm_load_si128(inptr++), _mm_load_si128(bitbuf0++));
            in56 = _mm_xor_si128(_mm_load_si128(inptr++), _mm_load_si128(bitbuf0++));
            in78 = _mm_xor_si128(_mm_load_si128(inptr++), _mm_load_si128(bitbuf0++));
        }

        // [0, 145, 183, 211]

        /* Pre Penryn CPUs the unpack should be faster */
        REALIGN_CHORBA(in12, in34, in56, in78,
                       in_1, in23, in45, in67, in8_, 8);

        STORE_LANE_XOR(buf144, bitbuf144);
        STORE_LANE_XOR(buf182, bitbuf182);

        __m128i n, o, p;
        __m128i m = _mm_xor_si128(buf210, in_1);

        /* Couldn't tell you why but despite knowing that this is always false,
         * removing this branch with GCC makes things significantly slower. Some
         * loop bodies must be being joined or something */
        if (i < 84 * 8) {
            n = in23;
            o = in45;
            p = in67;
            buf210 = in8_;
        } else {
            READ4_WITHXOR(n, o, p, buf210, in23, in45, in67, in8_, bitbuf210);
        }

        STORE4(m, n, o, p, bitbuf210);
        STORE4(in12, in34, in56, in78, bitbuf300);
    }

    /* Second half of stores bubbled out */
    _mm_store_si128(bitbuf144, buf144);
    _mm_store_si128(bitbuf182, buf182);
    _mm_store_si128(bitbuf210, buf210);

    /* We also have to zero out the tail */
    size_t left_to_z = len - (300*8 + i);
    __m128i *bitbuf_tail = (__m128i*)(bitbuffer + 300 + i/8);
    while (left_to_z >= 64) {
       STORE4(z, z, z, z, bitbuf_tail);
       left_to_z -= 64;
    }

    while (left_to_z >= 16) {
       _mm_store_si128(bitbuf_tail++, z);
       left_to_z -= 16;
    }

    uint8_t *tail_bytes = (uint8_t*)bitbuf_tail;
    while (left_to_z--) {
       *tail_bytes++ = 0;
    }

    static const chorba_word_t no_carry[CHORBA_CARRY_WORDS] = {0};
    return chorba_small_tail(buf, i, len, (const chorba_word_t*)bitbuffer, SIZE_MAX, no_carry);
}

Z_INTERNAL uint32_t crc32_chorba_sse41(uint32_t crc, const uint8_t *buf, size_t len) {
    uintptr_t align_diff = ALIGN_DIFF(buf, 16);
    /* At most 62 bytes, the tail folds anything longer */
    if (len <= align_diff + CHORBA_SMALL_THRESHOLD_64BIT)
        return ~crc32_copy_small(~crc, NULL, buf, len, 15 + CHORBA_SMALL_THRESHOLD_64BIT, 0);

    if (align_diff) {
        /* At most 15 bytes */
        crc = ~crc32_copy_small(~crc, NULL, buf, align_diff, 15, 0);
        len -= align_diff;
        buf += align_diff;
    }
#ifdef CRC32_CHORBA_FALLBACK
    if (len > CHORBA_LARGE_THRESHOLD)
        return crc32_chorba_118960_nondestructive(crc, buf, len);
#endif
    if (len > CHORBA_MEDIUM_LOWER_THRESHOLD && len <= CHORBA_MEDIUM_UPPER_THRESHOLD)
        return crc32_chorba_32768_nondestructive_sse41(crc, buf, len);
    return chorba_small_nondestructive_sse2(crc, buf, len);
}

Z_INTERNAL uint32_t crc32_copy_chorba_sse41(uint32_t crc, uint8_t *dst, const uint8_t *src, size_t len) {
    crc = crc32_chorba_sse41(crc, src, len);
    memcpy(dst, src, len);
    return crc;
}
#endif
