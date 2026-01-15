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
#include "crc32_p.h"
#include "x86_intrins.h"

#ifdef X86_VPCLMULQDQ
#  if defined(_MSC_VER) && _MSC_VER < 1920
     /* Use epi32 variants for older MSVC toolchains (v141/v140) to avoid cast warnings */
#    define z512_xor3_epi64(a, b, c)     _mm512_ternarylogic_epi32(a, b, c, 0x96)
#    define z512_inserti64x2(a, b, imm)  _mm512_inserti32x4(a, b, imm)
#    define z512_extracti64x2(a, imm)    _mm512_extracti32x4_epi32(a, imm)
#  else
#    define z512_xor3_epi64(a, b, c)     _mm512_ternarylogic_epi64(a, b, c, 0x96)
#    define z512_inserti64x2(a, b, imm)  _mm512_inserti64x2(a, b, imm)
#    define z512_extracti64x2(a, imm)    _mm512_extracti64x2_epi64(a, imm)
#  endif
#  ifdef __AVX512VL__
#    define z128_xor3_epi64(a, b, c)  _mm_ternarylogic_epi64(a, b, c, 0x96)
#  endif
#endif

#ifndef z128_xor3_epi64
#  define z128_xor3_epi64(a, b, c)    _mm_xor_si128(_mm_xor_si128(a, b), c)
#endif

static inline void fold_1(__m128i *xmm_crc0, __m128i *xmm_crc1, __m128i *xmm_crc2, __m128i *xmm_crc3, const __m128i xmm_fold4) {
    __m128i x_low  = _mm_clmulepi64_si128(*xmm_crc0, xmm_fold4, 0x01);
    __m128i x_high = _mm_clmulepi64_si128(*xmm_crc0, xmm_fold4, 0x10);

    *xmm_crc0 = *xmm_crc1;
    *xmm_crc1 = *xmm_crc2;
    *xmm_crc2 = *xmm_crc3;
    *xmm_crc3 = _mm_xor_si128(x_low, x_high);
}

static inline void fold_2(__m128i *xmm_crc0, __m128i *xmm_crc1, __m128i *xmm_crc2, __m128i *xmm_crc3, const __m128i xmm_fold4) {
    __m128i x_low0  = _mm_clmulepi64_si128(*xmm_crc0, xmm_fold4, 0x01);
    __m128i x_high0 = _mm_clmulepi64_si128(*xmm_crc0, xmm_fold4, 0x10);
    __m128i x_low1  = _mm_clmulepi64_si128(*xmm_crc1, xmm_fold4, 0x01);
    __m128i x_high1 = _mm_clmulepi64_si128(*xmm_crc1, xmm_fold4, 0x10);

    *xmm_crc0 = *xmm_crc2;
    *xmm_crc1 = *xmm_crc3;
    *xmm_crc2 = _mm_xor_si128(x_low0, x_high0);
    *xmm_crc3 = _mm_xor_si128(x_low1, x_high1);
}

static inline void fold_3(__m128i *xmm_crc0, __m128i *xmm_crc1, __m128i *xmm_crc2, __m128i *xmm_crc3, const __m128i xmm_fold4) {
    __m128i x_low0  = _mm_clmulepi64_si128(*xmm_crc0, xmm_fold4, 0x01);
    __m128i x_high0 = _mm_clmulepi64_si128(*xmm_crc0, xmm_fold4, 0x10);
    __m128i x_low1  = _mm_clmulepi64_si128(*xmm_crc1, xmm_fold4, 0x01);
    __m128i x_high1 = _mm_clmulepi64_si128(*xmm_crc1, xmm_fold4, 0x10);
    __m128i x_low2  = _mm_clmulepi64_si128(*xmm_crc2, xmm_fold4, 0x01);
    __m128i x_high2 = _mm_clmulepi64_si128(*xmm_crc2, xmm_fold4, 0x10);

    *xmm_crc0 = *xmm_crc3;
    *xmm_crc1 = _mm_xor_si128(x_low0, x_high0);
    *xmm_crc2 = _mm_xor_si128(x_low1, x_high1);
    *xmm_crc3 = _mm_xor_si128(x_low2, x_high2);
}

static inline void fold_4(__m128i *xmm_crc0, __m128i *xmm_crc1, __m128i *xmm_crc2, __m128i *xmm_crc3, const __m128i xmm_fold4) {
    __m128i x_low0  = _mm_clmulepi64_si128(*xmm_crc0, xmm_fold4, 0x01);
    __m128i x_high0 = _mm_clmulepi64_si128(*xmm_crc0, xmm_fold4, 0x10);
    __m128i x_low1  = _mm_clmulepi64_si128(*xmm_crc1, xmm_fold4, 0x01);
    __m128i x_high1 = _mm_clmulepi64_si128(*xmm_crc1, xmm_fold4, 0x10);
    __m128i x_low2  = _mm_clmulepi64_si128(*xmm_crc2, xmm_fold4, 0x01);
    __m128i x_high2 = _mm_clmulepi64_si128(*xmm_crc2, xmm_fold4, 0x10);
    __m128i x_low3  = _mm_clmulepi64_si128(*xmm_crc3, xmm_fold4, 0x01);
    __m128i x_high3 = _mm_clmulepi64_si128(*xmm_crc3, xmm_fold4, 0x10);

    *xmm_crc0 = _mm_xor_si128(x_low0, x_high0);
    *xmm_crc1 = _mm_xor_si128(x_low1, x_high1);
    *xmm_crc2 = _mm_xor_si128(x_low2, x_high2);
    *xmm_crc3 = _mm_xor_si128(x_low3, x_high3);
}

static inline void fold_12(__m128i *xmm_crc0, __m128i *xmm_crc1, __m128i *xmm_crc2, __m128i *xmm_crc3) {
    const __m128i xmm_fold12 = _mm_set_epi64x(0x596C8D81, 0xF5E48C85);
    __m128i x_low0  = _mm_clmulepi64_si128(*xmm_crc0, xmm_fold12, 0x01);
    __m128i x_high0 = _mm_clmulepi64_si128(*xmm_crc0, xmm_fold12, 0x10);
    __m128i x_low1  = _mm_clmulepi64_si128(*xmm_crc1, xmm_fold12, 0x01);
    __m128i x_high1 = _mm_clmulepi64_si128(*xmm_crc1, xmm_fold12, 0x10);
    __m128i x_low2  = _mm_clmulepi64_si128(*xmm_crc2, xmm_fold12, 0x01);
    __m128i x_high2 = _mm_clmulepi64_si128(*xmm_crc2, xmm_fold12, 0x10);
    __m128i x_low3  = _mm_clmulepi64_si128(*xmm_crc3, xmm_fold12, 0x01);
    __m128i x_high3 = _mm_clmulepi64_si128(*xmm_crc3, xmm_fold12, 0x10);

    *xmm_crc0 = _mm_xor_si128(x_low0, x_high0);
    *xmm_crc1 = _mm_xor_si128(x_low1, x_high1);
    *xmm_crc2 = _mm_xor_si128(x_low2, x_high2);
    *xmm_crc3 = _mm_xor_si128(x_low3, x_high3);
}

#ifdef X86_VPCLMULQDQ
static inline void fold_16(__m512i *zmm_crc0, __m512i *zmm_crc1, __m512i *zmm_crc2, __m512i *zmm_crc3,
    const __m512i zmm_t0, const __m512i zmm_t1, const __m512i zmm_t2, const __m512i zmm_t3, const __m512i zmm_fold16) {
    __m512i z_low0  = _mm512_clmulepi64_epi128(*zmm_crc0, zmm_fold16, 0x01);
    __m512i z_high0 = _mm512_clmulepi64_epi128(*zmm_crc0, zmm_fold16, 0x10);
    __m512i z_low1  = _mm512_clmulepi64_epi128(*zmm_crc1, zmm_fold16, 0x01);
    __m512i z_high1 = _mm512_clmulepi64_epi128(*zmm_crc1, zmm_fold16, 0x10);
    __m512i z_low2  = _mm512_clmulepi64_epi128(*zmm_crc2, zmm_fold16, 0x01);
    __m512i z_high2 = _mm512_clmulepi64_epi128(*zmm_crc2, zmm_fold16, 0x10);
    __m512i z_low3  = _mm512_clmulepi64_epi128(*zmm_crc3, zmm_fold16, 0x01);
    __m512i z_high3 = _mm512_clmulepi64_epi128(*zmm_crc3, zmm_fold16, 0x10);

    *zmm_crc0 = z512_xor3_epi64(z_low0, z_high0, zmm_t0);
    *zmm_crc1 = z512_xor3_epi64(z_low1, z_high1, zmm_t1);
    *zmm_crc2 = z512_xor3_epi64(z_low2, z_high2, zmm_t2);
    *zmm_crc3 = z512_xor3_epi64(z_low3, z_high3, zmm_t3);
}
#endif

Z_FORCEINLINE static uint32_t crc32_copy_impl(uint32_t crc, uint8_t *dst, const uint8_t *src, size_t len, const int COPY) {
    size_t copy_len = len;
    if (len >= 16) {
        /* Calculate 16-byte alignment offset */
        uintptr_t align_diff = ALIGN_DIFF(src, 16);

        /* If total length is less than (alignment bytes + 16), use the faster small method.
         * Handles both initially small buffers and cases where alignment would leave < 16 bytes */
        copy_len = len < align_diff + 16 ? len : align_diff;
    }

    if (copy_len > 0) {
        crc = crc32_copy_small(~crc, dst, src, copy_len, COPY);
        src += copy_len;
        len -= copy_len;
        if (COPY) {
            dst += copy_len;
        }
    }

    if (len == 0)
        return crc;

    const __m128i xmm_fold4 = _mm_set_epi32(0x00000001, 0x54442bd4, 0x00000001, 0xc6e41596);

    __m128i xmm_t0, xmm_t1, xmm_t2, xmm_t3;
    __m128i xmm_crc0 = _mm_cvtsi32_si128(0x9db42487);
    __m128i xmm_crc1 = _mm_setzero_si128();
    __m128i xmm_crc2 = _mm_setzero_si128();
    __m128i xmm_crc3 = _mm_setzero_si128();

    if (crc != 0) {
        // Process the first 16 bytes and handle initial CRC
        len -= 16;
        xmm_t0 = _mm_load_si128((__m128i *)src);
        src += 16;

        fold_1(&xmm_crc0, &xmm_crc1, &xmm_crc2, &xmm_crc3, xmm_fold4);
        if (COPY) {
            _mm_storeu_si128((__m128i *)dst, xmm_t0);
            dst += 16;
        }
        xmm_crc3 = z128_xor3_epi64(xmm_crc3, xmm_t0, _mm_cvtsi32_si128(crc));
    }

#ifdef X86_VPCLMULQDQ
    if (len >= 256) {
        len -= 256;

        __m512i zmm_crc0, zmm_crc1, zmm_crc2, zmm_crc3;
        __m512i zmm_t0, zmm_t1, zmm_t2, zmm_t3;
        __m512i z_low0, z_high0;
        const __m512i zmm_fold4 = _mm512_set4_epi32(
            0x00000001, 0x54442bd4, 0x00000001, 0xc6e41596);
        const __m512i zmm_fold16 = _mm512_set4_epi32(
            0x00000001, 0x1542778a, 0x00000001, 0x322d1430);

        zmm_crc0 = _mm512_loadu_si512((__m512i *)src);
        zmm_crc1 = _mm512_loadu_si512((__m512i *)src + 1);
        zmm_crc2 = _mm512_loadu_si512((__m512i *)src + 2);
        zmm_crc3 = _mm512_loadu_si512((__m512i *)src + 3);
        src += 256;
        if (COPY) {
            _mm512_storeu_si512((__m512i *)dst, zmm_crc0);
            _mm512_storeu_si512((__m512i *)dst + 1, zmm_crc1);
            _mm512_storeu_si512((__m512i *)dst + 2, zmm_crc2);
            _mm512_storeu_si512((__m512i *)dst + 3, zmm_crc3);
            dst += 256;
        }

        // Fold existing xmm state into first 64 bytes
        zmm_t0 = _mm512_castsi128_si512(xmm_crc0);
        zmm_t0 = z512_inserti64x2(zmm_t0, xmm_crc1, 1);
        zmm_t0 = z512_inserti64x2(zmm_t0, xmm_crc2, 2);
        zmm_t0 = z512_inserti64x2(zmm_t0, xmm_crc3, 3);

        z_low0 = _mm512_clmulepi64_epi128(zmm_t0, zmm_fold4, 0x01);
        z_high0 = _mm512_clmulepi64_epi128(zmm_t0, zmm_fold4, 0x10);
        zmm_crc0 = z512_xor3_epi64(zmm_crc0, z_low0, z_high0);

        while (len >= 256) {
            len -= 256;
            zmm_t0 = _mm512_loadu_si512((__m512i *)src);
            zmm_t1 = _mm512_loadu_si512((__m512i *)src + 1);
            zmm_t2 = _mm512_loadu_si512((__m512i *)src + 2);
            zmm_t3 = _mm512_loadu_si512((__m512i *)src + 3);
            src += 256;

            fold_16(&zmm_crc0, &zmm_crc1, &zmm_crc2, &zmm_crc3, zmm_t0, zmm_t1, zmm_t2, zmm_t3, zmm_fold16);
            if (COPY) {
                _mm512_storeu_si512((__m512i *)dst, zmm_t0);
                _mm512_storeu_si512((__m512i *)dst + 1, zmm_t1);
                _mm512_storeu_si512((__m512i *)dst + 2, zmm_t2);
                _mm512_storeu_si512((__m512i *)dst + 3, zmm_t3);
                dst += 256;
            }
        }

        // zmm_crc[0,1,2,3] -> zmm_crc0
        z_low0 = _mm512_clmulepi64_epi128(zmm_crc0, zmm_fold4, 0x01);
        z_high0 = _mm512_clmulepi64_epi128(zmm_crc0, zmm_fold4, 0x10);
        zmm_crc0 = z512_xor3_epi64(z_low0, z_high0, zmm_crc1);

        z_low0 = _mm512_clmulepi64_epi128(zmm_crc0, zmm_fold4, 0x01);
        z_high0 = _mm512_clmulepi64_epi128(zmm_crc0, zmm_fold4, 0x10);
        zmm_crc0 = z512_xor3_epi64(z_low0, z_high0, zmm_crc2);

        z_low0 = _mm512_clmulepi64_epi128(zmm_crc0, zmm_fold4, 0x01);
        z_high0 = _mm512_clmulepi64_epi128(zmm_crc0, zmm_fold4, 0x10);
        zmm_crc0 = z512_xor3_epi64(z_low0, z_high0, zmm_crc3);

        // zmm_crc0 -> xmm_crc[0, 1, 2, 3]
        xmm_crc0 = z512_extracti64x2(zmm_crc0, 0);
        xmm_crc1 = z512_extracti64x2(zmm_crc0, 1);
        xmm_crc2 = z512_extracti64x2(zmm_crc0, 2);
        xmm_crc3 = z512_extracti64x2(zmm_crc0, 3);
    }
#else
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

        fold_12(&xmm_crc0, &xmm_crc1, &xmm_crc2, &xmm_crc3);
        if (COPY) {
            _mm_storeu_si128((__m128i *)dst, xmm_t0);
            _mm_storeu_si128((__m128i *)dst + 1, xmm_t1);
            _mm_storeu_si128((__m128i *)dst + 2, xmm_t2);
            _mm_storeu_si128((__m128i *)dst + 3, xmm_t3);
            dst += 64;
        }

        xmm_crc0 = z128_xor3_epi64(xmm_t0, chorba6, xmm_crc0);
        xmm_crc1 = _mm_xor_si128(z128_xor3_epi64(xmm_t1, chorba5, chorba8), xmm_crc1);
        xmm_crc2 = z128_xor3_epi64(z128_xor3_epi64(xmm_t2, chorba4, chorba8), chorba7, xmm_crc2);
        xmm_crc3 = z128_xor3_epi64(z128_xor3_epi64(xmm_t3, chorba3, chorba7), chorba6, xmm_crc3);

        xmm_t0 = _mm_load_si128((__m128i *)src + 4);
        xmm_t1 = _mm_load_si128((__m128i *)src + 5);
        xmm_t2 = _mm_load_si128((__m128i *)src + 6);
        xmm_t3 = _mm_load_si128((__m128i *)src + 7);

        fold_4(&xmm_crc0, &xmm_crc1, &xmm_crc2, &xmm_crc3, xmm_fold4);
        if (COPY) {
            _mm_storeu_si128((__m128i *)dst, xmm_t0);
            _mm_storeu_si128((__m128i *)dst + 1, xmm_t1);
            _mm_storeu_si128((__m128i *)dst + 2, xmm_t2);
            _mm_storeu_si128((__m128i *)dst + 3, xmm_t3);
            dst += 64;
        }

        xmm_crc0 = z128_xor3_epi64(z128_xor3_epi64(xmm_t0, chorba2, chorba6), chorba5, xmm_crc0);
        xmm_crc1 = z128_xor3_epi64(z128_xor3_epi64(xmm_t1, chorba1, chorba4), chorba5, xmm_crc1);
        xmm_crc2 = _mm_xor_si128(z128_xor3_epi64(xmm_t2, chorba3, chorba4), xmm_crc2);
        xmm_crc3 = _mm_xor_si128(z128_xor3_epi64(xmm_t3, chorba2, chorba3), xmm_crc3);

        xmm_t0 = _mm_load_si128((__m128i *)src + 8);
        xmm_t1 = _mm_load_si128((__m128i *)src + 9);
        xmm_t2 = _mm_load_si128((__m128i *)src + 10);
        xmm_t3 = _mm_load_si128((__m128i *)src + 11);

        fold_4(&xmm_crc0, &xmm_crc1, &xmm_crc2, &xmm_crc3, xmm_fold4);
        if (COPY) {
            _mm_storeu_si128((__m128i *)dst, xmm_t0);
            _mm_storeu_si128((__m128i *)dst + 1, xmm_t1);
            _mm_storeu_si128((__m128i *)dst + 2, xmm_t2);
            _mm_storeu_si128((__m128i *)dst + 3, xmm_t3);
            dst += 64;
        }

        xmm_crc0 = z128_xor3_epi64(z128_xor3_epi64(xmm_t0, chorba1, chorba2), chorba8, xmm_crc0);
        xmm_crc1 = _mm_xor_si128(z128_xor3_epi64(xmm_t1, chorba1, chorba7), xmm_crc1);
        xmm_crc2 = z128_xor3_epi64(xmm_t2, chorba6, xmm_crc2);
        xmm_crc3 = z128_xor3_epi64(xmm_t3, chorba5, xmm_crc3);

        xmm_t0 = _mm_load_si128((__m128i *)src + 12);
        xmm_t1 = _mm_load_si128((__m128i *)src + 13);
        xmm_t2 = _mm_load_si128((__m128i *)src + 14);
        xmm_t3 = _mm_load_si128((__m128i *)src + 15);

        fold_4(&xmm_crc0, &xmm_crc1, &xmm_crc2, &xmm_crc3, xmm_fold4);
        if (COPY) {
            _mm_storeu_si128((__m128i *)dst, xmm_t0);
            _mm_storeu_si128((__m128i *)dst + 1, xmm_t1);
            _mm_storeu_si128((__m128i *)dst + 2, xmm_t2);
            _mm_storeu_si128((__m128i *)dst + 3, xmm_t3);
            dst += 64;
        }

        xmm_crc0 = _mm_xor_si128(z128_xor3_epi64(xmm_t0, chorba4, chorba8), xmm_crc0);
        xmm_crc1 = z128_xor3_epi64(z128_xor3_epi64(xmm_t1, chorba3, chorba8), chorba7, xmm_crc1);
        xmm_crc2 = _mm_xor_si128(z128_xor3_epi64(z128_xor3_epi64(xmm_t2, chorba2, chorba8), chorba7, chorba6), xmm_crc2);
        xmm_crc3 = _mm_xor_si128(z128_xor3_epi64(z128_xor3_epi64(xmm_t3, chorba1, chorba7), chorba6, chorba5), xmm_crc3);

        xmm_t0 = _mm_load_si128((__m128i *)src + 16);
        xmm_t1 = _mm_load_si128((__m128i *)src + 17);
        xmm_t2 = _mm_load_si128((__m128i *)src + 18);
        xmm_t3 = _mm_load_si128((__m128i *)src + 19);

        fold_4(&xmm_crc0, &xmm_crc1, &xmm_crc2, &xmm_crc3, xmm_fold4);
        if (COPY) {
            _mm_storeu_si128((__m128i *)dst, xmm_t0);
            _mm_storeu_si128((__m128i *)dst + 1, xmm_t1);
            _mm_storeu_si128((__m128i *)dst + 2, xmm_t2);
            _mm_storeu_si128((__m128i *)dst + 3, xmm_t3);
            dst += 64;
        }

        xmm_crc0 = _mm_xor_si128(z128_xor3_epi64(z128_xor3_epi64(xmm_t0, chorba4, chorba8), chorba6, chorba5), xmm_crc0);
        xmm_crc1 = z128_xor3_epi64(z128_xor3_epi64(z128_xor3_epi64(xmm_t1, chorba3, chorba4), chorba8, chorba7), chorba5, xmm_crc1);
        xmm_crc2 = z128_xor3_epi64(z128_xor3_epi64(z128_xor3_epi64(xmm_t2, chorba2, chorba3), chorba4, chorba7), chorba6, xmm_crc2);
        xmm_crc3 = _mm_xor_si128(z128_xor3_epi64(z128_xor3_epi64(z128_xor3_epi64(xmm_t3, chorba1, chorba2), chorba3, chorba8), chorba6, chorba5), xmm_crc3);

        xmm_t0 = _mm_load_si128((__m128i *)src + 20);
        xmm_t1 = _mm_load_si128((__m128i *)src + 21);
        xmm_t2 = _mm_load_si128((__m128i *)src + 22);
        xmm_t3 = _mm_load_si128((__m128i *)src + 23);

        fold_4(&xmm_crc0, &xmm_crc1, &xmm_crc2, &xmm_crc3, xmm_fold4);
        if (COPY) {
            _mm_storeu_si128((__m128i *)dst, xmm_t0);
            _mm_storeu_si128((__m128i *)dst + 1, xmm_t1);
            _mm_storeu_si128((__m128i *)dst + 2, xmm_t2);
            _mm_storeu_si128((__m128i *)dst + 3, xmm_t3);
            dst += 64;
        }

        xmm_crc0 = _mm_xor_si128(z128_xor3_epi64(z128_xor3_epi64(z128_xor3_epi64(xmm_t0, chorba1, chorba2), chorba4, chorba8), chorba7, chorba5), xmm_crc0);
        xmm_crc1 = z128_xor3_epi64(z128_xor3_epi64(z128_xor3_epi64(xmm_t1, chorba1, chorba3), chorba4, chorba7), chorba6, xmm_crc1);
        xmm_crc2 = z128_xor3_epi64(z128_xor3_epi64(z128_xor3_epi64(xmm_t2, chorba2, chorba3), chorba8, chorba6), chorba5, xmm_crc2);
        xmm_crc3 = _mm_xor_si128(z128_xor3_epi64(z128_xor3_epi64(z128_xor3_epi64(xmm_t3, chorba1, chorba2), chorba4, chorba8), chorba7, chorba5), xmm_crc3);

        xmm_t0 = _mm_load_si128((__m128i *)src + 24);
        xmm_t1 = _mm_load_si128((__m128i *)src + 25);
        xmm_t2 = _mm_load_si128((__m128i *)src + 26);
        xmm_t3 = _mm_load_si128((__m128i *)src + 27);

        fold_4(&xmm_crc0, &xmm_crc1, &xmm_crc2, &xmm_crc3, xmm_fold4);
        if (COPY) {
            _mm_storeu_si128((__m128i *)dst, xmm_t0);
            _mm_storeu_si128((__m128i *)dst + 1, xmm_t1);
            _mm_storeu_si128((__m128i *)dst + 2, xmm_t2);
            _mm_storeu_si128((__m128i *)dst + 3, xmm_t3);
            dst += 64;
        }

        xmm_crc0 = _mm_xor_si128(z128_xor3_epi64(z128_xor3_epi64(z128_xor3_epi64(xmm_t0, chorba1, chorba3), chorba4, chorba8), chorba7, chorba6), xmm_crc0);
        xmm_crc1 = z128_xor3_epi64(z128_xor3_epi64(z128_xor3_epi64(xmm_t1, chorba2, chorba3), chorba7, chorba6), chorba5, xmm_crc1);
        xmm_crc2 = z128_xor3_epi64(z128_xor3_epi64(z128_xor3_epi64(xmm_t2, chorba1, chorba2), chorba4, chorba6), chorba5, xmm_crc2);
        xmm_crc3 = _mm_xor_si128(z128_xor3_epi64(z128_xor3_epi64(xmm_t3, chorba1, chorba3), chorba4, chorba5), xmm_crc3);

        xmm_t0 = _mm_load_si128((__m128i *)src + 28);
        xmm_t1 = _mm_load_si128((__m128i *)src + 29);
        xmm_t2 = _mm_load_si128((__m128i *)src + 30);
        xmm_t3 = _mm_load_si128((__m128i *)src + 31);

        fold_4(&xmm_crc0, &xmm_crc1, &xmm_crc2, &xmm_crc3, xmm_fold4);
        if (COPY) {
            _mm_storeu_si128((__m128i *)dst, xmm_t0);
            _mm_storeu_si128((__m128i *)dst + 1, xmm_t1);
            _mm_storeu_si128((__m128i *)dst + 2, xmm_t2);
            _mm_storeu_si128((__m128i *)dst + 3, xmm_t3);
            dst += 64;
        }

        xmm_crc0 = z128_xor3_epi64(z128_xor3_epi64(xmm_t0, chorba2, chorba3), chorba4, xmm_crc0);
        xmm_crc1 = z128_xor3_epi64(z128_xor3_epi64(xmm_t1, chorba1, chorba2), chorba3, xmm_crc1);
        xmm_crc2 = _mm_xor_si128(z128_xor3_epi64(xmm_t2, chorba1, chorba2), xmm_crc2);
        xmm_crc3 = z128_xor3_epi64(xmm_t3, chorba1, xmm_crc3);

        len -= 512;
        src += 512;
    }
#ifndef __AVX512VL__
    }
#endif

#endif  /* X86_VPCLMULQDQ */

    while (len >= 64) {
        len -= 64;
        xmm_t0 = _mm_load_si128((__m128i *)src);
        xmm_t1 = _mm_load_si128((__m128i *)src + 1);
        xmm_t2 = _mm_load_si128((__m128i *)src + 2);
        xmm_t3 = _mm_load_si128((__m128i *)src + 3);
        src += 64;

        fold_4(&xmm_crc0, &xmm_crc1, &xmm_crc2, &xmm_crc3, xmm_fold4);
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

        xmm_t0 = _mm_load_si128((__m128i *)src);
        xmm_t1 = _mm_load_si128((__m128i *)src + 1);
        xmm_t2 = _mm_load_si128((__m128i *)src + 2);
        src += 48;

        fold_3(&xmm_crc0, &xmm_crc1, &xmm_crc2, &xmm_crc3, xmm_fold4);
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

        xmm_t0 = _mm_load_si128((__m128i *)src);
        xmm_t1 = _mm_load_si128((__m128i *)src + 1);
        src += 32;

        fold_2(&xmm_crc0, &xmm_crc1, &xmm_crc2, &xmm_crc3, xmm_fold4);
        if (COPY) {
            _mm_storeu_si128((__m128i *)dst, xmm_t0);
            _mm_storeu_si128((__m128i *)dst + 1, xmm_t1);
            dst += 32;
        }

        xmm_crc2 = _mm_xor_si128(xmm_crc2, xmm_t0);
        xmm_crc3 = _mm_xor_si128(xmm_crc3, xmm_t1);
    } else if (len >= 16) {
        len -= 16;
        xmm_t0 = _mm_load_si128((__m128i *)src);
        src += 16;

        fold_1(&xmm_crc0, &xmm_crc1, &xmm_crc2, &xmm_crc3, xmm_fold4);
        if (COPY) {
            _mm_storeu_si128((__m128i *)dst, xmm_t0);
            dst += 16;
        }

        xmm_crc3 = _mm_xor_si128(xmm_crc3, xmm_t0);
    }

    const __m128i k12 = _mm_set_epi32(0x00000001, 0x751997d0, 0x00000000, 0xccaa009e);
    const __m128i barrett_k = _mm_set_epi32(0x00000001, 0xdb710640, 0xb4e5b025, 0xf7011641);

    /* Fold 4x128-bit into a single 128-bit value using k1/k2 constants */
    __m128i x_low0  = _mm_clmulepi64_si128(xmm_crc0, k12, 0x01);
    __m128i x_high0 = _mm_clmulepi64_si128(xmm_crc0, k12, 0x10);
    xmm_crc1 = z128_xor3_epi64(xmm_crc1, x_low0, x_high0);

    __m128i x_low1  = _mm_clmulepi64_si128(xmm_crc1, k12, 0x01);
    __m128i x_high1 = _mm_clmulepi64_si128(xmm_crc1, k12, 0x10);
    xmm_crc2 = z128_xor3_epi64(xmm_crc2, x_low1, x_high1);

    __m128i x_low2  = _mm_clmulepi64_si128(xmm_crc2, k12, 0x01);
    __m128i x_high2 = _mm_clmulepi64_si128(xmm_crc2, k12, 0x10);
    xmm_crc3 = z128_xor3_epi64(xmm_crc3, x_low2, x_high2);

    /* Fold remaining bytes into the 128-bit state */
    if (len) {
        const __m128i xmm_mask3 = _mm_set1_epi32((int32_t)0x80808080);
        const __m128i xmm_seq = _mm_setr_epi8(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);

        /* Create masks to shift bytes for partial input */
        __m128i xmm_shl = _mm_add_epi8(xmm_seq, _mm_set1_epi8((char)len - 16));
        __m128i xmm_shr = _mm_xor_si128(xmm_shl, xmm_mask3);

        /* Shift out bytes from crc3 to make space for new data */
        __m128i xmm_overflow = _mm_shuffle_epi8(xmm_crc3, xmm_shl);
        xmm_crc3 = _mm_shuffle_epi8(xmm_crc3, xmm_shr);

        /* Insert the partial input into crc3 */
#if defined(__AVX512BW__) && defined(__AVX512VL__)
        __mmask16 k = (1 << len) - 1;
        __m128i xmm_crc_part = _mm_maskz_loadu_epi8(k, src);
        if (COPY) {
            _mm_mask_storeu_epi8(dst, k, xmm_crc_part);
        }
#else
        __m128i xmm_crc_part = _mm_setzero_si128();
        memcpy(&xmm_crc_part, src, len);
        if (COPY) {
            memcpy(dst, src, len);
        }
#endif
        __m128i part_aligned = _mm_shuffle_epi8(xmm_crc_part, xmm_shl);
        xmm_crc3 = _mm_xor_si128(xmm_crc3, part_aligned);

        /* Fold the bytes that were shifted out back into crc3 */
        __m128i ovf_low  = _mm_clmulepi64_si128(xmm_overflow, k12, 0x01);
        __m128i ovf_high = _mm_clmulepi64_si128(xmm_overflow, k12, 0x10);
        xmm_crc3 = z128_xor3_epi64(xmm_crc3, ovf_low, ovf_high);
    }

    /* Reduce 128-bits to 32-bits using two-stage Barrett reduction */
    __m128i x_tmp0 = _mm_clmulepi64_si128(xmm_crc3, barrett_k, 0x00);
    __m128i x_tmp1 = _mm_clmulepi64_si128(x_tmp0, barrett_k, 0x10);

    x_tmp1 = _mm_blend_epi16(x_tmp1, _mm_setzero_si128(), 0xcf);
    x_tmp0 = _mm_xor_si128(x_tmp1, xmm_crc3);

    __m128i x_res_a = _mm_clmulepi64_si128(x_tmp0, barrett_k, 0x01);
    __m128i x_res_b = _mm_clmulepi64_si128(x_res_a, barrett_k, 0x10);

    crc = ((uint32_t)_mm_extract_epi32(x_res_b, 2));

    return ~crc;
}
