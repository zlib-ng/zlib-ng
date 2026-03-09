/* x86_functions.h -- x86 implementations for arch-specific functions.
 * Copyright (C) 2013 Intel Corporation Jim Kukunas
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#ifndef X86_FUNCTIONS_H_
#define X86_FUNCTIONS_H_

#include "x86_natives.h"

/* So great news, your compiler is broken and causes stack smashing. Rather than
 * notching out its compilation we'll just remove the assignment in the functable.
 * Further context:
 * https://developercommunity.visualstudio.com/t/Stack-corruption-with-v142-toolchain-whe/10853479 */
#if defined(_MSC_VER) && defined(ARCH_32BIT) && _MSC_VER >= 1920 && _MSC_VER <= 1929
#define NO_CHORBA_SSE
#endif

#ifdef X86_SSE2
uint8_t* chunkmemset_safe_sse2(uint8_t *out, uint8_t *from, size_t len, size_t left);
uint32_t compare256_sse2(const uint8_t *src0, const uint8_t *src1);
void inflate_fast_sse2(PREFIX3(stream)* strm, uint32_t start);
uint32_t longest_match_sse2(deflate_state *const s, uint32_t cur_match);
uint32_t longest_match_slow_sse2(deflate_state *const s, uint32_t cur_match);
void slide_hash_sse2(deflate_state *s);

#  if !defined(WITHOUT_CHORBA_SSE)
    uint32_t crc32_chorba_sse2(uint32_t crc, const uint8_t *buf, size_t len);
    uint32_t crc32_copy_chorba_sse2(uint32_t crc, uint8_t *dst, const uint8_t *src, size_t len);
    uint32_t chorba_small_nondestructive_sse2(uint32_t crc, const uint8_t *buf, size_t len);
#  endif
#endif

#ifdef X86_SSSE3
uint32_t adler32_ssse3(uint32_t adler, const uint8_t *buf, size_t len);
uint32_t adler32_copy_ssse3(uint32_t adler, uint8_t *dst, const uint8_t *src, size_t len);
uint8_t* chunkmemset_safe_ssse3(uint8_t *out, uint8_t *from, size_t len, size_t left);
void inflate_fast_ssse3(PREFIX3(stream) *strm, uint32_t start);
#endif

#if defined(X86_SSE41) && !defined(WITHOUT_CHORBA_SSE)
    uint32_t crc32_chorba_sse41(uint32_t crc, const uint8_t *buf, size_t len);
    uint32_t crc32_copy_chorba_sse41(uint32_t crc, uint8_t *dst, const uint8_t *src, size_t len);
#endif

#ifdef X86_SSE42
uint32_t adler32_copy_sse42(uint32_t adler, uint8_t *dst, const uint8_t *src, size_t len);
#endif

#ifdef X86_AVX2
uint32_t adler32_avx2(uint32_t adler, const uint8_t *buf, size_t len);
uint32_t adler32_copy_avx2(uint32_t adler, uint8_t *dst, const uint8_t *src, size_t len);
uint8_t* chunkmemset_safe_avx2(uint8_t *out, uint8_t *from, size_t len, size_t left);
uint32_t compare256_avx2(const uint8_t *src0, const uint8_t *src1);
void inflate_fast_avx2(PREFIX3(stream)* strm, uint32_t start);
uint32_t longest_match_avx2(deflate_state *const s, uint32_t cur_match);
uint32_t longest_match_slow_avx2(deflate_state *const s, uint32_t cur_match);
void slide_hash_avx2(deflate_state *s);
#endif
#ifdef X86_AVX512
uint32_t adler32_avx512(uint32_t adler, const uint8_t *buf, size_t len);
uint32_t adler32_copy_avx512(uint32_t adler, uint8_t *dst, const uint8_t *src, size_t len);
uint8_t* chunkmemset_safe_avx512(uint8_t *out, uint8_t *from, size_t len, size_t left);
uint32_t compare256_avx512(const uint8_t *src0, const uint8_t *src1);
void inflate_fast_avx512(PREFIX3(stream)* strm, uint32_t start);
uint32_t longest_match_avx512(deflate_state *const s, uint32_t cur_match);
uint32_t longest_match_slow_avx512(deflate_state *const s, uint32_t cur_match);
#endif
#ifdef X86_AVX512VNNI
uint32_t adler32_avx512_vnni(uint32_t adler, const uint8_t *buf, size_t len);
uint32_t adler32_copy_avx512_vnni(uint32_t adler, uint8_t *dst, const uint8_t *src, size_t len);
#endif

#ifdef X86_PCLMULQDQ_CRC
uint32_t crc32_pclmulqdq(uint32_t crc, const uint8_t *buf, size_t len);
uint32_t crc32_copy_pclmulqdq(uint32_t crc, uint8_t *dst, const uint8_t *src, size_t len);
#endif
#ifdef X86_VPCLMULQDQ_AVX2
uint32_t crc32_vpclmulqdq_avx2(uint32_t crc, const uint8_t *buf, size_t len);
uint32_t crc32_copy_vpclmulqdq_avx2(uint32_t crc, uint8_t *dst, const uint8_t *src, size_t len);
#endif
#ifdef X86_VPCLMULQDQ_AVX512
uint32_t crc32_vpclmulqdq_avx512(uint32_t crc, const uint8_t *buf, size_t len);
uint32_t crc32_copy_vpclmulqdq_avx512(uint32_t crc, uint8_t *dst, const uint8_t *src, size_t len);
#endif

#ifdef DISABLE_RUNTIME_CPU_DETECTION
// X86 - SSE2
#  ifdef X86_SSE2_NATIVE
#    undef native_chunkmemset_safe
#    define native_chunkmemset_safe chunkmemset_safe_sse2
#    undef native_compare256
#    define native_compare256 compare256_sse2
#    undef native_inflate_fast
#    define native_inflate_fast inflate_fast_sse2
#    undef native_longest_match
#    define native_longest_match longest_match_sse2
#    undef native_longest_match_slow
#    define native_longest_match_slow longest_match_slow_sse2
#    if !defined(WITHOUT_CHORBA_SSE)
#      undef native_crc32
#      define native_crc32 crc32_chorba_sse2
#      undef native_crc32_copy
#      define native_crc32_copy crc32_copy_chorba_sse2
#    endif
#    undef native_slide_hash
#    define native_slide_hash slide_hash_sse2
#  endif
// X86 - SSSE3
#  ifdef X86_SSSE3_NATIVE
#    undef native_adler32
#    define native_adler32 adler32_ssse3
#    undef native_adler32_copy
#    define native_adler32_copy adler32_copy_ssse3
#    undef native_chunkmemset_safe
#    define native_chunkmemset_safe chunkmemset_safe_ssse3
#    undef native_inflate_fast
#    define native_inflate_fast inflate_fast_ssse3
#  endif
// X86 - SSE4.1
#  if defined(X86_SSE41_NATIVE) && !defined(WITHOUT_CHORBA_SSE)
#    undef native_crc32
#    define native_crc32 crc32_chorba_sse41
#    undef native_crc32_copy
#    define native_crc32_copy crc32_copy_chorba_sse41
#  endif
// X86 - SSE4.2
#  ifdef X86_SSE42_NATIVE
#    undef native_adler32_copy
#    define native_adler32_copy adler32_copy_sse42
#  endif
// X86 - PCLMUL
#  ifdef X86_PCLMULQDQ_NATIVE
#    undef native_crc32
#    define native_crc32 crc32_pclmulqdq
#    undef native_crc32_copy
#    define native_crc32_copy crc32_copy_pclmulqdq
#  endif
// X86 - AVX2
#  ifdef X86_AVX2_NATIVE
#    undef native_adler32
#    define native_adler32 adler32_avx2
#    undef native_adler32_copy
#    define native_adler32_copy adler32_copy_avx2
#    undef native_chunkmemset_safe
#    define native_chunkmemset_safe chunkmemset_safe_avx2
#    undef native_compare256
#    define native_compare256 compare256_avx2
#    undef native_inflate_fast
#    define native_inflate_fast inflate_fast_avx2
#    undef native_longest_match
#    define native_longest_match longest_match_avx2
#    undef native_longest_match_slow
#    define native_longest_match_slow longest_match_slow_avx2
#    undef native_slide_hash
#    define native_slide_hash slide_hash_avx2
#  endif
// X86 - AVX512 (F,DQ,BW,Vl)
#  ifdef X86_AVX512_NATIVE
#    undef native_adler32
#    define native_adler32 adler32_avx512
#    undef native_adler32_copy
#    define native_adler32_copy adler32_copy_avx512
#    undef native_chunkmemset_safe
#    define native_chunkmemset_safe chunkmemset_safe_avx512
#    undef native_compare256
#    define native_compare256 compare256_avx512
#    undef native_inflate_fast
#    define native_inflate_fast inflate_fast_avx512
#    undef native_longest_match
#    define native_longest_match longest_match_avx512
#    undef native_longest_match_slow
#    define native_longest_match_slow longest_match_slow_avx512
// X86 - AVX512 (VNNI)
#    ifdef X86_AVX512VNNI_NATIVE
#      undef native_adler32
#      define native_adler32 adler32_avx512_vnni
#      undef native_adler32_copy
#      define native_adler32_copy adler32_copy_avx512_vnni
#    endif
#  endif
// X86 - VPCLMULQDQ
#  ifdef X86_VPCLMULQDQ_AVX512_NATIVE
#    undef native_crc32
#    define native_crc32 crc32_vpclmulqdq_avx512
#    undef native_crc32_copy
#    define native_crc32_copy crc32_copy_vpclmulqdq_avx512
#  elif defined(X86_VPCLMULQDQ_AVX2_NATIVE)
#    undef native_crc32
#    define native_crc32 crc32_vpclmulqdq_avx2
#    undef native_crc32_copy
#    define native_crc32_copy crc32_copy_vpclmulqdq_avx2
#  endif
#endif

#endif /* X86_FUNCTIONS_H_ */
