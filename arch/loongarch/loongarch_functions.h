/* loongarch_functions.h -- LoongArch implementations for arch-specific functions.
 *
 * Copyright (C) 2025 Vladislav Shchapov <vladislav@shchapov.ru>
 *
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#ifndef LOONGARCH_FUNCTIONS_H_
#define LOONGARCH_FUNCTIONS_H_

#include "loongarch_natives.h"

#ifdef LOONGARCH_CRC
uint32_t crc32_loongarch64(uint32_t crc, const uint8_t *buf, size_t len);
uint32_t crc32_copy_loongarch64(uint32_t crc, uint8_t *dst, const uint8_t *src, size_t len);
#endif

#ifndef LOONGARCH_CRC_NATIVE
#  define CRC32_BRAID_FALLBACK
#endif

#ifdef LOONGARCH_LSX
uint32_t adler32_lsx(uint32_t adler, const uint8_t *src, size_t len);
uint32_t adler32_copy_lsx(uint32_t adler, uint8_t *dst, const uint8_t *src, size_t len);
uint8_t* chunkmemset_safe_lsx(uint8_t *out, uint8_t *from, size_t len, size_t left);
uint32_t compare256_lsx(const uint8_t *src0, const uint8_t *src1);
void inflate_fast_lsx(PREFIX3(stream) *strm, uint32_t start);
uint32_t longest_match_lsx(deflate_state *const s, uint32_t cur_match);
uint32_t longest_match_roll_lsx(deflate_state *const s, uint32_t cur_match);
void slide_hash_lsx(deflate_state *s);
#endif

#ifndef LOONGARCH_LSX_NATIVE
#  define ADLER32_FALLBACK
#  define CHUNKSET_FALLBACK
#  define COMPARE256_FALLBACK
#  define SLIDE_HASH_FALLBACK
#endif

#ifdef LOONGARCH_LASX
uint32_t adler32_lasx(uint32_t adler, const uint8_t *src, size_t len);
uint32_t adler32_copy_lasx(uint32_t adler, uint8_t *dst, const uint8_t *src, size_t len);
uint8_t* chunkmemset_safe_lasx(uint8_t *out, uint8_t *from, size_t len, size_t left);
uint32_t compare256_lasx(const uint8_t *src0, const uint8_t *src1);
void inflate_fast_lasx(PREFIX3(stream) *strm, uint32_t start);
uint32_t longest_match_lasx(deflate_state *const s, uint32_t cur_match);
uint32_t longest_match_roll_lasx(deflate_state *const s, uint32_t cur_match);
void slide_hash_lasx(deflate_state *s);
#endif

#ifdef DISABLE_RUNTIME_CPU_DETECTION
// LOONGARCH - CRC32
#  ifdef LOONGARCH_CRC_NATIVE
#    undef native_crc32
#    define native_crc32 crc32_loongarch64
#    undef native_crc32_copy
#    define native_crc32_copy crc32_copy_loongarch64
#  endif
#  ifdef LOONGARCH_LSX_NATIVE
#    undef native_adler32
#    define native_adler32 adler32_lsx
#    undef native_adler32_copy
#    define native_adler32_copy adler32_copy_lsx
#    undef native_chunkmemset_safe
#    define native_chunkmemset_safe chunkmemset_safe_lsx
#    undef native_compare256
#    define native_compare256 compare256_lsx
#    undef native_inflate_fast
#    define native_inflate_fast inflate_fast_lsx
#    undef native_longest_match
#    define native_longest_match longest_match_lsx
#    undef native_longest_match_roll
#    define native_longest_match_roll longest_match_roll_lsx
#    undef native_slide_hash
#    define native_slide_hash slide_hash_lsx
#  endif
#  ifdef LOONGARCH_LASX_NATIVE
#    undef native_adler32
#    define native_adler32 adler32_lasx
#    undef native_adler32_copy
#    define native_adler32_copy adler32_copy_lasx
#    undef native_chunkmemset_safe
#    define native_chunkmemset_safe chunkmemset_safe_lasx
#    undef native_compare256
#    define native_compare256 compare256_lasx
#    undef native_inflate_fast
#    define native_inflate_fast inflate_fast_lasx
#    undef native_longest_match
#    define native_longest_match longest_match_lasx
#    undef native_longest_match_roll
#    define native_longest_match_roll longest_match_roll_lasx
#    undef native_slide_hash
#    define native_slide_hash slide_hash_lasx
#  endif
#endif

#endif /* LOONGARCH_FUNCTIONS_H_ */
