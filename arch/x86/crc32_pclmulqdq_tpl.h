/* crc32_pclmulqdq_tpl.h -- Compute the CRC32 using a parallelized folding
 * approach with the PCLMULQDQ and VPCMULQDQ instructions.
 *
 * A white paper describing this algorithm can be found at:
 *     doc/crc-pclmulqdq.pdf
 *
 * Copyright (C) 2020 Wangyang Guo (wangyang.guo@intel.com) (VPCLMULQDQ support)
 * Copyright (C) 2013 Intel Corporation. All rights reserved.
 * Copyright (C) 2016 Marian Beermann (support for initial value)
 * Authors:
 *     Wajdi Feghali   <wajdi.k.feghali@intel.com>
 *     Jim Guilford    <james.guilford@intel.com>
 *     Vinodh Gopal    <vinodh.gopal@intel.com>
 *     Erdinc Ozturk   <erdinc.ozturk@intel.com>
 *     Jim Kukunas     <james.t.kukunas@linux.intel.com>
 *
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#include "zbuild.h"

#include <immintrin.h>
#include <wmmintrin.h>
#include <smmintrin.h> // _mm_extract_epi32

#include "crc32.h"
#include "crc32_braid_p.h"
#include "crc32_braid_tbl.h"
#include "x86_intrins.h"

static inline void fold_1(__m128i *xmm_crc0, __m128i *xmm_crc1, __m128i *xmm_crc2, __m128i *xmm_crc3) {
    const __m128i xmm_fold4 = _mm_set_epi32( 0x00000001, 0x54442bd4,
                                             0x00000001, 0xc6e41596);
    __m128i x_tmp3;
    __m128 ps_crc0, ps_crc3, ps_res;

    x_tmp3 = *xmm_crc3;

    *xmm_crc3 = *xmm_crc0;
    *xmm_crc0 = _mm_clmulepi64_si128(*xmm_crc0, xmm_fold4, 0x01);
    *xmm_crc3 = _mm_clmulepi64_si128(*xmm_crc3, xmm_fold4, 0x10);
    ps_crc0 = _mm_castsi128_ps(*xmm_crc0);
    ps_crc3 = _mm_castsi128_ps(*xmm_crc3);
    ps_res = _mm_xor_ps(ps_crc0, ps_crc3);

    *xmm_crc0 = *xmm_crc1;
    *xmm_crc1 = *xmm_crc2;
    *xmm_crc2 = x_tmp3;
    *xmm_crc3 = _mm_castps_si128(ps_res);
}

static inline void fold_2(__m128i *xmm_crc0, __m128i *xmm_crc1, __m128i *xmm_crc2, __m128i *xmm_crc3) {
    const __m128i xmm_fold4 = _mm_set_epi32( 0x00000001, 0x54442bd4,
                                             0x00000001, 0xc6e41596);
    __m128i x_tmp3, x_tmp2;
    __m128 ps_crc0, ps_crc1, ps_crc2, ps_crc3, ps_res31, ps_res20;

    x_tmp3 = *xmm_crc3;
    x_tmp2 = *xmm_crc2;

    *xmm_crc3 = *xmm_crc1;
    *xmm_crc1 = _mm_clmulepi64_si128(*xmm_crc1, xmm_fold4, 0x01);
    *xmm_crc3 = _mm_clmulepi64_si128(*xmm_crc3, xmm_fold4, 0x10);
    ps_crc3 = _mm_castsi128_ps(*xmm_crc3);
    ps_crc1 = _mm_castsi128_ps(*xmm_crc1);
    ps_res31 = _mm_xor_ps(ps_crc3, ps_crc1);

    *xmm_crc2 = *xmm_crc0;
    *xmm_crc0 = _mm_clmulepi64_si128(*xmm_crc0, xmm_fold4, 0x01);
    *xmm_crc2 = _mm_clmulepi64_si128(*xmm_crc2, xmm_fold4, 0x10);
    ps_crc0 = _mm_castsi128_ps(*xmm_crc0);
    ps_crc2 = _mm_castsi128_ps(*xmm_crc2);
    ps_res20 = _mm_xor_ps(ps_crc0, ps_crc2);

    *xmm_crc0 = x_tmp2;
    *xmm_crc1 = x_tmp3;
    *xmm_crc2 = _mm_castps_si128(ps_res20);
    *xmm_crc3 = _mm_castps_si128(ps_res31);
}

static inline void fold_3(__m128i *xmm_crc0, __m128i *xmm_crc1, __m128i *xmm_crc2, __m128i *xmm_crc3) {
    const __m128i xmm_fold4 = _mm_set_epi32( 0x00000001, 0x54442bd4,
                                             0x00000001, 0xc6e41596);
    __m128i x_tmp3;
    __m128 ps_crc0, ps_crc1, ps_crc2, ps_crc3, ps_res32, ps_res21, ps_res10;

    x_tmp3 = *xmm_crc3;

    *xmm_crc3 = *xmm_crc2;
    *xmm_crc2 = _mm_clmulepi64_si128(*xmm_crc2, xmm_fold4, 0x01);
    *xmm_crc3 = _mm_clmulepi64_si128(*xmm_crc3, xmm_fold4, 0x10);
    ps_crc2 = _mm_castsi128_ps(*xmm_crc2);
    ps_crc3 = _mm_castsi128_ps(*xmm_crc3);
    ps_res32 = _mm_xor_ps(ps_crc2, ps_crc3);

    *xmm_crc2 = *xmm_crc1;
    *xmm_crc1 = _mm_clmulepi64_si128(*xmm_crc1, xmm_fold4, 0x01);
    *xmm_crc2 = _mm_clmulepi64_si128(*xmm_crc2, xmm_fold4, 0x10);
    ps_crc1 = _mm_castsi128_ps(*xmm_crc1);
    ps_crc2 = _mm_castsi128_ps(*xmm_crc2);
    ps_res21 = _mm_xor_ps(ps_crc1, ps_crc2);

    *xmm_crc1 = *xmm_crc0;
    *xmm_crc0 = _mm_clmulepi64_si128(*xmm_crc0, xmm_fold4, 0x01);
    *xmm_crc1 = _mm_clmulepi64_si128(*xmm_crc1, xmm_fold4, 0x10);
    ps_crc0 = _mm_castsi128_ps(*xmm_crc0);
    ps_crc1 = _mm_castsi128_ps(*xmm_crc1);
    ps_res10 = _mm_xor_ps(ps_crc0, ps_crc1);

    *xmm_crc0 = x_tmp3;
    *xmm_crc1 = _mm_castps_si128(ps_res10);
    *xmm_crc2 = _mm_castps_si128(ps_res21);
    *xmm_crc3 = _mm_castps_si128(ps_res32);
}

static inline void fold_4(__m128i *xmm_crc0, __m128i *xmm_crc1, __m128i *xmm_crc2, __m128i *xmm_crc3) {
    const __m128i xmm_fold4 = _mm_set_epi32( 0x00000001, 0x54442bd4,
                                             0x00000001, 0xc6e41596);
    __m128i x_tmp0, x_tmp1, x_tmp2, x_tmp3;
    __m128 ps_crc0, ps_crc1, ps_crc2, ps_crc3;
    __m128 ps_t0, ps_t1, ps_t2, ps_t3;
    __m128 ps_res0, ps_res1, ps_res2, ps_res3;

    x_tmp0 = *xmm_crc0;
    x_tmp1 = *xmm_crc1;
    x_tmp2 = *xmm_crc2;
    x_tmp3 = *xmm_crc3;

    *xmm_crc0 = _mm_clmulepi64_si128(*xmm_crc0, xmm_fold4, 0x01);
    x_tmp0 = _mm_clmulepi64_si128(x_tmp0, xmm_fold4, 0x10);
    ps_crc0 = _mm_castsi128_ps(*xmm_crc0);
    ps_t0 = _mm_castsi128_ps(x_tmp0);
    ps_res0 = _mm_xor_ps(ps_crc0, ps_t0);

    *xmm_crc1 = _mm_clmulepi64_si128(*xmm_crc1, xmm_fold4, 0x01);
    x_tmp1 = _mm_clmulepi64_si128(x_tmp1, xmm_fold4, 0x10);
    ps_crc1 = _mm_castsi128_ps(*xmm_crc1);
    ps_t1 = _mm_castsi128_ps(x_tmp1);
    ps_res1 = _mm_xor_ps(ps_crc1, ps_t1);

    *xmm_crc2 = _mm_clmulepi64_si128(*xmm_crc2, xmm_fold4, 0x01);
    x_tmp2 = _mm_clmulepi64_si128(x_tmp2, xmm_fold4, 0x10);
    ps_crc2 = _mm_castsi128_ps(*xmm_crc2);
    ps_t2 = _mm_castsi128_ps(x_tmp2);
    ps_res2 = _mm_xor_ps(ps_crc2, ps_t2);

    *xmm_crc3 = _mm_clmulepi64_si128(*xmm_crc3, xmm_fold4, 0x01);
    x_tmp3 = _mm_clmulepi64_si128(x_tmp3, xmm_fold4, 0x10);
    ps_crc3 = _mm_castsi128_ps(*xmm_crc3);
    ps_t3 = _mm_castsi128_ps(x_tmp3);
    ps_res3 = _mm_xor_ps(ps_crc3, ps_t3);

    *xmm_crc0 = _mm_castps_si128(ps_res0);
    *xmm_crc1 = _mm_castps_si128(ps_res1);
    *xmm_crc2 = _mm_castps_si128(ps_res2);
    *xmm_crc3 = _mm_castps_si128(ps_res3);
}

static inline void fold_12(__m128i *xmm_crc0, __m128i *xmm_crc1, __m128i *xmm_crc2, __m128i *xmm_crc3) {
    const __m128i xmm_fold12 = _mm_set_epi64x(0x596C8D81, 0xF5E48C85);
    __m128i x_tmp0, x_tmp1, x_tmp2, x_tmp3;
    __m128 ps_crc0, ps_crc1, ps_crc2, ps_crc3;
    __m128 ps_t0, ps_t1, ps_t2, ps_t3;
    __m128 ps_res0, ps_res1, ps_res2, ps_res3;

    x_tmp0 = *xmm_crc0;
    x_tmp1 = *xmm_crc1;
    x_tmp2 = *xmm_crc2;
    x_tmp3 = *xmm_crc3;

    *xmm_crc0 = _mm_clmulepi64_si128(*xmm_crc0, xmm_fold12, 0x01);
    x_tmp0 = _mm_clmulepi64_si128(x_tmp0, xmm_fold12, 0x10);
    ps_crc0 = _mm_castsi128_ps(*xmm_crc0);
    ps_t0 = _mm_castsi128_ps(x_tmp0);
    ps_res0 = _mm_xor_ps(ps_crc0, ps_t0);

    *xmm_crc1 = _mm_clmulepi64_si128(*xmm_crc1, xmm_fold12, 0x01);
    x_tmp1 = _mm_clmulepi64_si128(x_tmp1, xmm_fold12, 0x10);
    ps_crc1 = _mm_castsi128_ps(*xmm_crc1);
    ps_t1 = _mm_castsi128_ps(x_tmp1);
    ps_res1 = _mm_xor_ps(ps_crc1, ps_t1);

    *xmm_crc2 = _mm_clmulepi64_si128(*xmm_crc2, xmm_fold12, 0x01);
    x_tmp2 = _mm_clmulepi64_si128(x_tmp2, xmm_fold12, 0x10);
    ps_crc2 = _mm_castsi128_ps(*xmm_crc2);
    ps_t2 = _mm_castsi128_ps(x_tmp2);
    ps_res2 = _mm_xor_ps(ps_crc2, ps_t2);

    *xmm_crc3 = _mm_clmulepi64_si128(*xmm_crc3, xmm_fold12, 0x01);
    x_tmp3 = _mm_clmulepi64_si128(x_tmp3, xmm_fold12, 0x10);
    ps_crc3 = _mm_castsi128_ps(*xmm_crc3);
    ps_t3 = _mm_castsi128_ps(x_tmp3);
    ps_res3 = _mm_xor_ps(ps_crc3, ps_t3);

    *xmm_crc0 = _mm_castps_si128(ps_res0);
    *xmm_crc1 = _mm_castps_si128(ps_res1);
    *xmm_crc2 = _mm_castps_si128(ps_res2);
    *xmm_crc3 = _mm_castps_si128(ps_res3);
}

#ifdef X86_VPCLMULQDQ
static inline void fold_16(__m512i *zmm_crc0, __m512i *zmm_crc1, __m512i *zmm_crc2, __m512i *zmm_crc3,
    const __m512i *zmm_t0, const __m512i *zmm_t1, const __m512i *zmm_t2, const __m512i *zmm_t3) {
    const __m512i zmm_fold16 = _mm512_set4_epi32(
        0x00000001, 0x1542778a, 0x00000001, 0x322d1430);

    __m512i z0 = _mm512_clmulepi64_epi128(*zmm_crc0, zmm_fold16, 0x01);
    __m512i z1 = _mm512_clmulepi64_epi128(*zmm_crc1, zmm_fold16, 0x01);
    __m512i z2 = _mm512_clmulepi64_epi128(*zmm_crc2, zmm_fold16, 0x01);
    __m512i z3 = _mm512_clmulepi64_epi128(*zmm_crc3, zmm_fold16, 0x01);

    *zmm_crc0 = _mm512_clmulepi64_epi128(*zmm_crc0, zmm_fold16, 0x10);
    *zmm_crc1 = _mm512_clmulepi64_epi128(*zmm_crc1, zmm_fold16, 0x10);
    *zmm_crc2 = _mm512_clmulepi64_epi128(*zmm_crc2, zmm_fold16, 0x10);
    *zmm_crc3 = _mm512_clmulepi64_epi128(*zmm_crc3, zmm_fold16, 0x10);

    *zmm_crc0 = _mm512_ternarylogic_epi32(*zmm_crc0, z0, *zmm_t0, 0x96);
    *zmm_crc1 = _mm512_ternarylogic_epi32(*zmm_crc1, z1, *zmm_t1, 0x96);
    *zmm_crc2 = _mm512_ternarylogic_epi32(*zmm_crc2, z2, *zmm_t2, 0x96);
    *zmm_crc3 = _mm512_ternarylogic_epi32(*zmm_crc3, z3, *zmm_t3, 0x96);
}
#endif

static const unsigned ALIGNED_(32) pshufb_shf_table[60] = {
    0x84838281, 0x88878685, 0x8c8b8a89, 0x008f8e8d, /* shl 15 (16 - 1)/shr1 */
    0x85848382, 0x89888786, 0x8d8c8b8a, 0x01008f8e, /* shl 14 (16 - 3)/shr2 */
    0x86858483, 0x8a898887, 0x8e8d8c8b, 0x0201008f, /* shl 13 (16 - 4)/shr3 */
    0x87868584, 0x8b8a8988, 0x8f8e8d8c, 0x03020100, /* shl 12 (16 - 4)/shr4 */
    0x88878685, 0x8c8b8a89, 0x008f8e8d, 0x04030201, /* shl 11 (16 - 5)/shr5 */
    0x89888786, 0x8d8c8b8a, 0x01008f8e, 0x05040302, /* shl 10 (16 - 6)/shr6 */
    0x8a898887, 0x8e8d8c8b, 0x0201008f, 0x06050403, /* shl  9 (16 - 7)/shr7 */
    0x8b8a8988, 0x8f8e8d8c, 0x03020100, 0x07060504, /* shl  8 (16 - 8)/shr8 */
    0x8c8b8a89, 0x008f8e8d, 0x04030201, 0x08070605, /* shl  7 (16 - 9)/shr9 */
    0x8d8c8b8a, 0x01008f8e, 0x05040302, 0x09080706, /* shl  6 (16 -10)/shr10*/
    0x8e8d8c8b, 0x0201008f, 0x06050403, 0x0a090807, /* shl  5 (16 -11)/shr11*/
    0x8f8e8d8c, 0x03020100, 0x07060504, 0x0b0a0908, /* shl  4 (16 -12)/shr12*/
    0x008f8e8d, 0x04030201, 0x08070605, 0x0c0b0a09, /* shl  3 (16 -13)/shr13*/
    0x01008f8e, 0x05040302, 0x09080706, 0x0d0c0b0a, /* shl  2 (16 -14)/shr14*/
    0x0201008f, 0x06050403, 0x0a090807, 0x0e0d0c0b  /* shl  1 (16 -15)/shr15*/
};

static inline void partial_fold(const size_t len, __m128i *xmm_crc0, __m128i *xmm_crc1, __m128i *xmm_crc2,
    __m128i *xmm_crc3, __m128i *xmm_crc_part) {
    const __m128i xmm_fold4 = _mm_set_epi32(0x00000001, 0x54442bd4,
                                            0x00000001, 0xc6e41596);
    const __m128i xmm_mask3 = _mm_set1_epi32((int32_t)0x80808080);

    __m128i xmm_shl, xmm_shr, xmm_tmp1, xmm_tmp2, xmm_tmp3;
    __m128i xmm_a0_0, xmm_a0_1;
    __m128 ps_crc3, psa0_0, psa0_1, ps_res;

    xmm_shl = _mm_load_si128((__m128i *)(pshufb_shf_table + (4 * (len - 1))));
    xmm_shr = xmm_shl;
    xmm_shr = _mm_xor_si128(xmm_shr, xmm_mask3);

    xmm_a0_0 = _mm_shuffle_epi8(*xmm_crc0, xmm_shl);

    *xmm_crc0 = _mm_shuffle_epi8(*xmm_crc0, xmm_shr);
    xmm_tmp1 = _mm_shuffle_epi8(*xmm_crc1, xmm_shl);
    *xmm_crc0 = _mm_or_si128(*xmm_crc0, xmm_tmp1);

    *xmm_crc1 = _mm_shuffle_epi8(*xmm_crc1, xmm_shr);
    xmm_tmp2 = _mm_shuffle_epi8(*xmm_crc2, xmm_shl);
    *xmm_crc1 = _mm_or_si128(*xmm_crc1, xmm_tmp2);

    *xmm_crc2 = _mm_shuffle_epi8(*xmm_crc2, xmm_shr);
    xmm_tmp3 = _mm_shuffle_epi8(*xmm_crc3, xmm_shl);
    *xmm_crc2 = _mm_or_si128(*xmm_crc2, xmm_tmp3);

    *xmm_crc3 = _mm_shuffle_epi8(*xmm_crc3, xmm_shr);
    *xmm_crc_part = _mm_shuffle_epi8(*xmm_crc_part, xmm_shl);
    *xmm_crc3 = _mm_or_si128(*xmm_crc3, *xmm_crc_part);

    xmm_a0_1 = _mm_clmulepi64_si128(xmm_a0_0, xmm_fold4, 0x10);
    xmm_a0_0 = _mm_clmulepi64_si128(xmm_a0_0, xmm_fold4, 0x01);

    ps_crc3 = _mm_castsi128_ps(*xmm_crc3);
    psa0_0 = _mm_castsi128_ps(xmm_a0_0);
    psa0_1 = _mm_castsi128_ps(xmm_a0_1);

    ps_res = _mm_xor_ps(ps_crc3, psa0_0);
    ps_res = _mm_xor_ps(ps_res, psa0_1);

    *xmm_crc3 = _mm_castps_si128(ps_res);
}

static inline uint32_t crc32_copy_small(uint32_t crc, uint8_t *dst, const uint8_t *buf, size_t len, const int COPY) {
    uint32_t c = (~crc) & 0xffffffff;

    while (len) {
        len--;
        if (COPY) {
            *dst++ = *buf;
        }
        CRC_DO1;
    }

    return c ^ 0xffffffff;
}

static inline uint32_t fold_final(__m128i *xmm_crc0, __m128i *xmm_crc1, __m128i *xmm_crc2, __m128i *xmm_crc3) {
    __m128i x_tmp0, x_tmp1, x_tmp2;
    const __m128i k12 = _mm_set_epi32(0x00000001, 0x751997d0, 0x00000000, 0xccaa009e);
    const __m128i barrett_k = _mm_set_epi32(0x00000001, 0xdb710640, 0xb4e5b025, 0xf7011641);
    uint32_t crc;

    /* Fold 4x128-bit into a single 128-bit value using k1/k2 constants */
    x_tmp0 = _mm_clmulepi64_si128(*xmm_crc0, k12, 0x10);
    *xmm_crc0 = _mm_clmulepi64_si128(*xmm_crc0, k12, 0x01);
    *xmm_crc1 = _mm_xor_si128(*xmm_crc1, x_tmp0);
    *xmm_crc1 = _mm_xor_si128(*xmm_crc1, *xmm_crc0);

    x_tmp1 = _mm_clmulepi64_si128(*xmm_crc1, k12, 0x10);
    *xmm_crc1 = _mm_clmulepi64_si128(*xmm_crc1, k12, 0x01);
    *xmm_crc2 = _mm_xor_si128(*xmm_crc2, x_tmp1);
    *xmm_crc2 = _mm_xor_si128(*xmm_crc2, *xmm_crc1);

    x_tmp2 = _mm_clmulepi64_si128(*xmm_crc2, k12, 0x10);
    *xmm_crc2 = _mm_clmulepi64_si128(*xmm_crc2, k12, 0x01);
    *xmm_crc3 = _mm_xor_si128(*xmm_crc3, x_tmp2);
    *xmm_crc3 = _mm_xor_si128(*xmm_crc3, *xmm_crc2);

    /* Reduce 128-bits to 32-bits using two-stage Barrett reduction */
    x_tmp0 = _mm_clmulepi64_si128(*xmm_crc3, barrett_k, 0x00);
    x_tmp1 = _mm_clmulepi64_si128(x_tmp0, barrett_k, 0x10);
    x_tmp1 = _mm_and_si128(x_tmp1, _mm_setr_epi32(0, 0, ~0, 0));
    x_tmp0 = _mm_xor_si128(x_tmp1, *xmm_crc3);
    x_tmp0 = _mm_clmulepi64_si128(x_tmp0, barrett_k, 0x01);
    x_tmp0 = _mm_clmulepi64_si128(x_tmp0, barrett_k, 0x10);

    crc = ((uint32_t)_mm_extract_epi32(x_tmp0, 2));

    return ~crc;
}

Z_FORCEINLINE static uint32_t crc32_copy_impl(uint32_t crc, uint8_t *dst, const uint8_t *src, size_t len, const int COPY) {
    size_t copy_len = len;
    if (len >= 16) {
        /* Calculate 16-byte alignment offset */
        unsigned algn_diff = ((uintptr_t)16 - ((uintptr_t)src & 0xF)) & 0xF;

        /* If total length is less than (alignment bytes + 16), use the faster small method.
         * Handles both initially small buffers and cases where alignment would leave < 16 bytes */
        copy_len = len < algn_diff + 16 ? len : algn_diff;
    }

    if (copy_len > 0) {
        crc = crc32_copy_small(crc, dst, src, copy_len, COPY);
        src += copy_len;
        len -= copy_len;
        if (COPY) {
            dst += copy_len;
        }
    }

    if (len == 0)
        return crc;

    __m128i xmm_t0, xmm_t1, xmm_t2, xmm_t3;
    __m128i xmm_crc_part = _mm_setzero_si128();
    __m128i xmm_crc0 = _mm_cvtsi32_si128(0x9db42487);
    __m128i xmm_crc1 = _mm_setzero_si128();
    __m128i xmm_crc2 = _mm_setzero_si128();
    __m128i xmm_crc3 = _mm_setzero_si128();

    if (crc != 0) {
        // Process the first 16 bytes and handle initial CRC
        fold_1(&xmm_crc0, &xmm_crc1, &xmm_crc2, &xmm_crc3);
        len -= 16;
        xmm_t0 = _mm_load_si128((__m128i *)src);
        src += 16;
        if (COPY) {
            _mm_storeu_si128((__m128i *)dst, xmm_t0);
            dst += 16;
        }
        xmm_t0 = _mm_xor_si128(xmm_t0, _mm_cvtsi32_si128(crc));
        xmm_crc3 = _mm_xor_si128(xmm_crc3, xmm_t0);
    }

#ifdef X86_VPCLMULQDQ
    if (len >= 256) {
        len -= 256;

        // zmm register init from xmm state and first 256 bytes
        __m512i zmm_crc0 = _mm512_setzero_si512();
        __m512i zmm_crc1, zmm_crc2, zmm_crc3;
        __m512i zmm_t0, zmm_t1, zmm_t2, zmm_t3;
        const __m512i zmm_fold4 = _mm512_set4_epi32(
            0x00000001, 0x54442bd4, 0x00000001, 0xc6e41596);

        zmm_t0 = _mm512_loadu_si512((__m512i *)src);
        zmm_crc1 = _mm512_loadu_si512((__m512i *)src + 1);
        zmm_crc2 = _mm512_loadu_si512((__m512i *)src + 2);
        zmm_crc3 = _mm512_loadu_si512((__m512i *)src + 3);
        src += 256;
        if (COPY) {
            _mm512_storeu_si512((__m512i *)dst, zmm_t0);
            _mm512_storeu_si512((__m512i *)dst + 1, zmm_crc1);
            _mm512_storeu_si512((__m512i *)dst + 2, zmm_crc2);
            _mm512_storeu_si512((__m512i *)dst + 3, zmm_crc3);
            dst += 256;
        }

        // Combine 4 partial CRCs into zmm_crc0 using clmul with the folding polynomial (zmm_fold4)
        zmm_crc0 = _mm512_inserti32x4(zmm_crc0, xmm_crc0, 0);
        zmm_crc0 = _mm512_inserti32x4(zmm_crc0, xmm_crc1, 1);
        zmm_crc0 = _mm512_inserti32x4(zmm_crc0, xmm_crc2, 2);
        zmm_crc0 = _mm512_inserti32x4(zmm_crc0, xmm_crc3, 3);
        __m512i z0 = _mm512_clmulepi64_epi128(zmm_crc0, zmm_fold4, 0x01);
        zmm_crc0 = _mm512_clmulepi64_epi128(zmm_crc0, zmm_fold4, 0x10);
        zmm_crc0 = _mm512_ternarylogic_epi32(zmm_crc0, z0, zmm_t0, 0x96);

        while (len >= 256) {
            len -= 256;
            zmm_t0 = _mm512_loadu_si512((__m512i *)src);
            zmm_t1 = _mm512_loadu_si512((__m512i *)src + 1);
            zmm_t2 = _mm512_loadu_si512((__m512i *)src + 2);
            zmm_t3 = _mm512_loadu_si512((__m512i *)src + 3);
            src += 256;
            if (COPY) {
                _mm512_storeu_si512((__m512i *)dst, zmm_t0);
                _mm512_storeu_si512((__m512i *)dst + 1, zmm_t1);
                _mm512_storeu_si512((__m512i *)dst + 2, zmm_t2);
                _mm512_storeu_si512((__m512i *)dst + 3, zmm_t3);
                dst += 256;
            }

            fold_16(&zmm_crc0, &zmm_crc1, &zmm_crc2, &zmm_crc3, &zmm_t0, &zmm_t1, &zmm_t2, &zmm_t3);
        }

        // zmm_crc[0,1,2,3] -> zmm_crc0
        z0 = _mm512_clmulepi64_epi128(zmm_crc0, zmm_fold4, 0x01);
        zmm_crc0 = _mm512_clmulepi64_epi128(zmm_crc0, zmm_fold4, 0x10);
        zmm_crc0 = _mm512_ternarylogic_epi32(zmm_crc0, z0, zmm_crc1, 0x96);

        z0 = _mm512_clmulepi64_epi128(zmm_crc0, zmm_fold4, 0x01);
        zmm_crc0 = _mm512_clmulepi64_epi128(zmm_crc0, zmm_fold4, 0x10);
        zmm_crc0 = _mm512_ternarylogic_epi32(zmm_crc0, z0, zmm_crc2, 0x96);

        z0 = _mm512_clmulepi64_epi128(zmm_crc0, zmm_fold4, 0x01);
        zmm_crc0 = _mm512_clmulepi64_epi128(zmm_crc0, zmm_fold4, 0x10);
        zmm_crc0 = _mm512_ternarylogic_epi32(zmm_crc0, z0, zmm_crc3, 0x96);

        // zmm_crc0 -> xmm_crc[0, 1, 2, 3]
        xmm_crc0 = _mm512_extracti32x4_epi32(zmm_crc0, 0);
        xmm_crc1 = _mm512_extracti32x4_epi32(zmm_crc0, 1);
        xmm_crc2 = _mm512_extracti32x4_epi32(zmm_crc0, 2);
        xmm_crc3 = _mm512_extracti32x4_epi32(zmm_crc0, 3);
    }
#endif

    /* Implement Chorba algorithm from https://arxiv.org/abs/2412.16398
     * We interleave the PCLMUL-base folds with 8x scaled generator
     * polynomial copies; we read 8x QWORDS and then XOR them into
     * the stream at the following offsets: 6, 9, 10, 16, 20, 22,
     * 24, 25, 27, 28, 30, 31, 32 - this is detailed in the paper
     * as "generator_64_bits_unrolled_8" */
#ifndef __AVX512VL__
    if (!COPY) {
#endif
    while (len >= 512 + 64 + 16*8) {
        fold_12(&xmm_crc0, &xmm_crc1, &xmm_crc2, &xmm_crc3);

        __m128i chorba8 = _mm_load_si128((__m128i *)src);
        __m128i chorba7 = _mm_load_si128((__m128i *)src + 1);
        __m128i chorba6 = _mm_load_si128((__m128i *)src + 2);
        __m128i chorba5 = _mm_load_si128((__m128i *)src + 3);
        __m128i chorba4 = _mm_load_si128((__m128i *)src + 4);
        __m128i chorba3 = _mm_load_si128((__m128i *)src + 5);
        __m128i chorba2 = _mm_load_si128((__m128i *)src + 6);
        __m128i chorba1 = _mm_load_si128((__m128i *)src + 7);
        if (COPY) {
            _mm_storeu_si128((__m128i *)dst, chorba8);
            _mm_storeu_si128((__m128i *)dst + 1, chorba7);
            _mm_storeu_si128((__m128i *)dst + 2, chorba6);
            _mm_storeu_si128((__m128i *)dst + 3, chorba5);
            _mm_storeu_si128((__m128i *)dst + 4, chorba4);
            _mm_storeu_si128((__m128i *)dst + 5, chorba3);
            _mm_storeu_si128((__m128i *)dst + 6, chorba2);
            _mm_storeu_si128((__m128i *)dst + 7, chorba1);
            dst += 16*8;
        }

        chorba2 = _mm_xor_si128(chorba2, chorba8);
        chorba1 = _mm_xor_si128(chorba1, chorba7);
        src += 16*8;
        len -= 16*8;

        xmm_t0 = _mm_load_si128((__m128i *)src);
        xmm_t1 = _mm_load_si128((__m128i *)src + 1);
        xmm_t2 = _mm_load_si128((__m128i *)src + 2);
        xmm_t3 = _mm_load_si128((__m128i *)src + 3);
        if (COPY) {
            _mm_storeu_si128((__m128i *)dst, xmm_t0);
            _mm_storeu_si128((__m128i *)dst + 1, xmm_t1);
            _mm_storeu_si128((__m128i *)dst + 2, xmm_t2);
            _mm_storeu_si128((__m128i *)dst + 3, xmm_t3);
            dst += 64;
        }

        xmm_t0 = _mm_xor_si128(xmm_t0, chorba6);
        xmm_t1 = _mm_xor_si128(_mm_xor_si128(xmm_t1, chorba5), chorba8);
        xmm_t2 = _mm_xor_si128(_mm_xor_si128(_mm_xor_si128(xmm_t2, chorba4), chorba8), chorba7);
        xmm_t3 = _mm_xor_si128(_mm_xor_si128(_mm_xor_si128(xmm_t3, chorba3), chorba7), chorba6);
        xmm_crc0 = _mm_xor_si128(xmm_t0, xmm_crc0);
        xmm_crc1 = _mm_xor_si128(xmm_t1, xmm_crc1);
        xmm_crc2 = _mm_xor_si128(xmm_t2, xmm_crc2);
        xmm_crc3 = _mm_xor_si128(xmm_t3, xmm_crc3);
        fold_4(&xmm_crc0, &xmm_crc1, &xmm_crc2, &xmm_crc3);

        xmm_t0 = _mm_load_si128((__m128i *)src + 4);
        xmm_t1 = _mm_load_si128((__m128i *)src + 5);
        xmm_t2 = _mm_load_si128((__m128i *)src + 6);
        xmm_t3 = _mm_load_si128((__m128i *)src + 7);
        if (COPY) {
            _mm_storeu_si128((__m128i *)dst, xmm_t0);
            _mm_storeu_si128((__m128i *)dst + 1, xmm_t1);
            _mm_storeu_si128((__m128i *)dst + 2, xmm_t2);
            _mm_storeu_si128((__m128i *)dst + 3, xmm_t3);
            dst += 64;
        }

        xmm_t0 = _mm_xor_si128(_mm_xor_si128(_mm_xor_si128(xmm_t0, chorba2), chorba6), chorba5);
        xmm_t1 = _mm_xor_si128(_mm_xor_si128(_mm_xor_si128(xmm_t1, chorba1), chorba4), chorba5);
        xmm_t2 = _mm_xor_si128(_mm_xor_si128(xmm_t2, chorba3), chorba4);
        xmm_t3 = _mm_xor_si128(_mm_xor_si128(xmm_t3, chorba2), chorba3);
        xmm_crc0 = _mm_xor_si128(xmm_t0, xmm_crc0);
        xmm_crc1 = _mm_xor_si128(xmm_t1, xmm_crc1);
        xmm_crc2 = _mm_xor_si128(xmm_t2, xmm_crc2);
        xmm_crc3 = _mm_xor_si128(xmm_t3, xmm_crc3);
        fold_4(&xmm_crc0, &xmm_crc1, &xmm_crc2, &xmm_crc3);

        xmm_t0 = _mm_load_si128((__m128i *)src + 8);
        xmm_t1 = _mm_load_si128((__m128i *)src + 9);
        xmm_t2 = _mm_load_si128((__m128i *)src + 10);
        xmm_t3 = _mm_load_si128((__m128i *)src + 11);
        if (COPY) {
            _mm_storeu_si128((__m128i *)dst, xmm_t0);
            _mm_storeu_si128((__m128i *)dst + 1, xmm_t1);
            _mm_storeu_si128((__m128i *)dst + 2, xmm_t2);
            _mm_storeu_si128((__m128i *)dst + 3, xmm_t3);
            dst += 64;
        }

        xmm_t0 = _mm_xor_si128(_mm_xor_si128(_mm_xor_si128(xmm_t0, chorba1), chorba2), chorba8);
        xmm_t1 = _mm_xor_si128(_mm_xor_si128(xmm_t1, chorba1), chorba7);
        xmm_t2 = _mm_xor_si128(xmm_t2, chorba6);
        xmm_t3 = _mm_xor_si128(xmm_t3, chorba5);
        xmm_crc0 = _mm_xor_si128(xmm_t0, xmm_crc0);
        xmm_crc1 = _mm_xor_si128(xmm_t1, xmm_crc1);
        xmm_crc2 = _mm_xor_si128(xmm_t2, xmm_crc2);
        xmm_crc3 = _mm_xor_si128(xmm_t3, xmm_crc3);
        fold_4(&xmm_crc0, &xmm_crc1, &xmm_crc2, &xmm_crc3);

        xmm_t0 = _mm_load_si128((__m128i *)src + 12);
        xmm_t1 = _mm_load_si128((__m128i *)src + 13);
        xmm_t2 = _mm_load_si128((__m128i *)src + 14);
        xmm_t3 = _mm_load_si128((__m128i *)src + 15);
        if (COPY) {
            _mm_storeu_si128((__m128i *)dst, xmm_t0);
            _mm_storeu_si128((__m128i *)dst + 1, xmm_t1);
            _mm_storeu_si128((__m128i *)dst + 2, xmm_t2);
            _mm_storeu_si128((__m128i *)dst + 3, xmm_t3);
            dst += 64;
        }

        xmm_t0 = _mm_xor_si128(_mm_xor_si128(xmm_t0, chorba4), chorba8);
        xmm_t1 = _mm_xor_si128(_mm_xor_si128(_mm_xor_si128(xmm_t1, chorba3), chorba8), chorba7);
        xmm_t2 = _mm_xor_si128(_mm_xor_si128(_mm_xor_si128(_mm_xor_si128(xmm_t2, chorba2), chorba8), chorba7), chorba6);
        xmm_t3 = _mm_xor_si128(_mm_xor_si128(_mm_xor_si128(_mm_xor_si128(xmm_t3, chorba1), chorba7), chorba6), chorba5);
        xmm_crc0 = _mm_xor_si128(xmm_t0, xmm_crc0);
        xmm_crc1 = _mm_xor_si128(xmm_t1, xmm_crc1);
        xmm_crc2 = _mm_xor_si128(xmm_t2, xmm_crc2);
        xmm_crc3 = _mm_xor_si128(xmm_t3, xmm_crc3);
        fold_4(&xmm_crc0, &xmm_crc1, &xmm_crc2, &xmm_crc3);

        xmm_t0 = _mm_load_si128((__m128i *)src + 16);
        xmm_t1 = _mm_load_si128((__m128i *)src + 17);
        xmm_t2 = _mm_load_si128((__m128i *)src + 18);
        xmm_t3 = _mm_load_si128((__m128i *)src + 19);
        if (COPY) {
            _mm_storeu_si128((__m128i *)dst, xmm_t0);
            _mm_storeu_si128((__m128i *)dst + 1, xmm_t1);
            _mm_storeu_si128((__m128i *)dst + 2, xmm_t2);
            _mm_storeu_si128((__m128i *)dst + 3, xmm_t3);
            dst += 64;
        }

        xmm_t0 = _mm_xor_si128(_mm_xor_si128(_mm_xor_si128(_mm_xor_si128(xmm_t0, chorba4), chorba8), chorba6), chorba5);
        xmm_t1 = _mm_xor_si128(_mm_xor_si128(_mm_xor_si128(_mm_xor_si128(_mm_xor_si128(xmm_t1, chorba3), chorba4), chorba8), chorba7), chorba5);
        xmm_t2 = _mm_xor_si128(_mm_xor_si128(_mm_xor_si128(_mm_xor_si128(_mm_xor_si128(xmm_t2, chorba2), chorba3), chorba4), chorba7), chorba6);
        xmm_t3 = _mm_xor_si128(_mm_xor_si128(_mm_xor_si128(_mm_xor_si128(_mm_xor_si128(_mm_xor_si128(xmm_t3, chorba1), chorba2), chorba3), chorba8), chorba6), chorba5);
        xmm_crc0 = _mm_xor_si128(xmm_t0, xmm_crc0);
        xmm_crc1 = _mm_xor_si128(xmm_t1, xmm_crc1);
        xmm_crc2 = _mm_xor_si128(xmm_t2, xmm_crc2);
        xmm_crc3 = _mm_xor_si128(xmm_t3, xmm_crc3);
        fold_4(&xmm_crc0, &xmm_crc1, &xmm_crc2, &xmm_crc3);

        xmm_t0 = _mm_load_si128((__m128i *)src + 20);
        xmm_t1 = _mm_load_si128((__m128i *)src + 21);
        xmm_t2 = _mm_load_si128((__m128i *)src + 22);
        xmm_t3 = _mm_load_si128((__m128i *)src + 23);
        if (COPY) {
            _mm_storeu_si128((__m128i *)dst, xmm_t0);
            _mm_storeu_si128((__m128i *)dst + 1, xmm_t1);
            _mm_storeu_si128((__m128i *)dst + 2, xmm_t2);
            _mm_storeu_si128((__m128i *)dst + 3, xmm_t3);
            dst += 64;
        }

        xmm_t0 = _mm_xor_si128(_mm_xor_si128(_mm_xor_si128(_mm_xor_si128(_mm_xor_si128(_mm_xor_si128(xmm_t0, chorba1), chorba2), chorba4), chorba8), chorba7), chorba5);
        xmm_t1 = _mm_xor_si128(_mm_xor_si128(_mm_xor_si128(_mm_xor_si128(_mm_xor_si128(xmm_t1, chorba1), chorba3), chorba4), chorba7), chorba6);
        xmm_t2 = _mm_xor_si128(_mm_xor_si128(_mm_xor_si128(_mm_xor_si128(_mm_xor_si128(xmm_t2, chorba2), chorba3), chorba8), chorba6), chorba5);
        xmm_t3 = _mm_xor_si128(_mm_xor_si128(_mm_xor_si128(_mm_xor_si128(_mm_xor_si128(_mm_xor_si128(xmm_t3, chorba1), chorba2), chorba4), chorba8), chorba7), chorba5);
        xmm_crc0 = _mm_xor_si128(xmm_t0, xmm_crc0);
        xmm_crc1 = _mm_xor_si128(xmm_t1, xmm_crc1);
        xmm_crc2 = _mm_xor_si128(xmm_t2, xmm_crc2);
        xmm_crc3 = _mm_xor_si128(xmm_t3, xmm_crc3);
        fold_4(&xmm_crc0, &xmm_crc1, &xmm_crc2, &xmm_crc3);

        xmm_t0 = _mm_load_si128((__m128i *)src + 24);
        xmm_t1 = _mm_load_si128((__m128i *)src + 25);
        xmm_t2 = _mm_load_si128((__m128i *)src + 26);
        xmm_t3 = _mm_load_si128((__m128i *)src + 27);
        if (COPY) {
            _mm_storeu_si128((__m128i *)dst, xmm_t0);
            _mm_storeu_si128((__m128i *)dst + 1, xmm_t1);
            _mm_storeu_si128((__m128i *)dst + 2, xmm_t2);
            _mm_storeu_si128((__m128i *)dst + 3, xmm_t3);
            dst += 64;
        }
        xmm_t0 = _mm_xor_si128(_mm_xor_si128(_mm_xor_si128(_mm_xor_si128(_mm_xor_si128(_mm_xor_si128(xmm_t0, chorba1), chorba3), chorba4), chorba8), chorba7), chorba6);
        xmm_t1 = _mm_xor_si128(_mm_xor_si128(_mm_xor_si128(_mm_xor_si128(_mm_xor_si128(xmm_t1, chorba2), chorba3), chorba7), chorba6), chorba5);
        xmm_t2 = _mm_xor_si128(_mm_xor_si128(_mm_xor_si128(_mm_xor_si128(_mm_xor_si128(xmm_t2, chorba1), chorba2), chorba4), chorba6), chorba5);
        xmm_t3 = _mm_xor_si128(_mm_xor_si128(_mm_xor_si128(_mm_xor_si128(xmm_t3, chorba1), chorba3), chorba4), chorba5);
        xmm_crc0 = _mm_xor_si128(xmm_t0, xmm_crc0);
        xmm_crc1 = _mm_xor_si128(xmm_t1, xmm_crc1);
        xmm_crc2 = _mm_xor_si128(xmm_t2, xmm_crc2);
        xmm_crc3 = _mm_xor_si128(xmm_t3, xmm_crc3);
        fold_4(&xmm_crc0, &xmm_crc1, &xmm_crc2, &xmm_crc3);

        xmm_t0 = _mm_load_si128((__m128i *)src + 28);
        xmm_t1 = _mm_load_si128((__m128i *)src + 29);
        xmm_t2 = _mm_load_si128((__m128i *)src + 30);
        xmm_t3 = _mm_load_si128((__m128i *)src + 31);
        if (COPY) {
            _mm_storeu_si128((__m128i *)dst, xmm_t0);
            _mm_storeu_si128((__m128i *)dst + 1, xmm_t1);
            _mm_storeu_si128((__m128i *)dst + 2, xmm_t2);
            _mm_storeu_si128((__m128i *)dst + 3, xmm_t3);
            dst += 64;
        }
        xmm_t0 = _mm_xor_si128(_mm_xor_si128(_mm_xor_si128(xmm_t0, chorba2), chorba3), chorba4);
        xmm_t1 = _mm_xor_si128(_mm_xor_si128(_mm_xor_si128(xmm_t1, chorba1), chorba2), chorba3);
        xmm_t2 = _mm_xor_si128(_mm_xor_si128(xmm_t2, chorba1), chorba2);
        xmm_t3 = _mm_xor_si128(xmm_t3, chorba1);
        xmm_crc0 = _mm_xor_si128(xmm_t0, xmm_crc0);
        xmm_crc1 = _mm_xor_si128(xmm_t1, xmm_crc1);
        xmm_crc2 = _mm_xor_si128(xmm_t2, xmm_crc2);
        xmm_crc3 = _mm_xor_si128(xmm_t3, xmm_crc3);

        len -= 512;
        src += 512;
    }
#ifndef __AVX512VL__
    }
#endif

    while (len >= 64) {
        len -= 64;
        fold_4(&xmm_crc0, &xmm_crc1, &xmm_crc2, &xmm_crc3);

        xmm_t0 = _mm_load_si128((__m128i *)src);
        xmm_t1 = _mm_load_si128((__m128i *)src + 1);
        xmm_t2 = _mm_load_si128((__m128i *)src + 2);
        xmm_t3 = _mm_load_si128((__m128i *)src + 3);
        src += 64;
        if (COPY) {
            _mm_storeu_si128((__m128i *)dst, xmm_t0);
            _mm_storeu_si128((__m128i *)dst + 1, xmm_t1);
            _mm_storeu_si128((__m128i *)dst + 2, xmm_t2);
            _mm_storeu_si128((__m128i *)dst + 3, xmm_t3);
            dst += 64;
        }

        xmm_crc0 = _mm_xor_si128(xmm_crc0, xmm_t0);
        xmm_crc1 = _mm_xor_si128(xmm_crc1, xmm_t1);
        xmm_crc2 = _mm_xor_si128(xmm_crc2, xmm_t2);
        xmm_crc3 = _mm_xor_si128(xmm_crc3, xmm_t3);
    }

    /*
     * len = num bytes left - 64
     */
    if (len >= 48) {
        len -= 48;
        fold_3(&xmm_crc0, &xmm_crc1, &xmm_crc2, &xmm_crc3);

        xmm_t0 = _mm_load_si128((__m128i *)src);
        xmm_t1 = _mm_load_si128((__m128i *)src + 1);
        xmm_t2 = _mm_load_si128((__m128i *)src + 2);
        src += 48;
        if (COPY) {
            _mm_storeu_si128((__m128i *)dst, xmm_t0);
            _mm_storeu_si128((__m128i *)dst + 1, xmm_t1);
            _mm_storeu_si128((__m128i *)dst + 2, xmm_t2);
            dst += 48;
        }

        xmm_crc1 = _mm_xor_si128(xmm_crc1, xmm_t0);
        xmm_crc2 = _mm_xor_si128(xmm_crc2, xmm_t1);
        xmm_crc3 = _mm_xor_si128(xmm_crc3, xmm_t2);
    } else if (len >= 32) {
        len -= 32;
        fold_2(&xmm_crc0, &xmm_crc1, &xmm_crc2, &xmm_crc3);

        xmm_t0 = _mm_load_si128((__m128i *)src);
        xmm_t1 = _mm_load_si128((__m128i *)src + 1);
        src += 32;
        if (COPY) {
            _mm_storeu_si128((__m128i *)dst, xmm_t0);
            _mm_storeu_si128((__m128i *)dst + 1, xmm_t1);
            dst += 32;
        }

        xmm_crc2 = _mm_xor_si128(xmm_crc2, xmm_t0);
        xmm_crc3 = _mm_xor_si128(xmm_crc3, xmm_t1);
    } else if (len >= 16) {
        len -= 16;
        fold_1(&xmm_crc0, &xmm_crc1, &xmm_crc2, &xmm_crc3);

        xmm_t0 = _mm_load_si128((__m128i *)src);
        src += 16;
        if (COPY) {
            _mm_storeu_si128((__m128i *)dst, xmm_t0);
            dst += 16;
        }

        xmm_crc3 = _mm_xor_si128(xmm_crc3, xmm_t0);
    }

    if (len) {
        memcpy(&xmm_crc_part, src, len);
        if (COPY) {
            uint8_t ALIGNED_(16) partial_buf[16] = { 0 };
            _mm_storeu_si128((__m128i *)partial_buf, xmm_crc_part);
            memcpy(dst, partial_buf, len);
        }
        partial_fold(len, &xmm_crc0, &xmm_crc1, &xmm_crc2, &xmm_crc3, &xmm_crc_part);
    }

    return fold_final(&xmm_crc0, &xmm_crc1, &xmm_crc2, &xmm_crc3);
}
