/* generic_functions.h -- generic C implementations for arch-specific functions.
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#ifndef GENERIC_FUNCTIONS_H_
#define GENERIC_FUNCTIONS_H_

typedef uint32_t (*adler32_func)(uint32_t adler, const uint8_t *buf, size_t len);
typedef uint32_t (*adler32_copy_func)(uint32_t adler, uint8_t *dst, const uint8_t *src, size_t len);
typedef uint32_t (*compare256_func)(const uint8_t *src0, const uint8_t *src1);
typedef uint32_t (*crc32_func)(uint32_t crc, const uint8_t *buf, size_t len);
typedef uint32_t (*crc32_copy_func)(uint32_t crc, uint8_t *dst, const uint8_t *src, size_t len);
typedef void     (*slide_hash_func)(deflate_state *s);

#ifdef ADLER32_FALLBACK
uint32_t adler32_c(uint32_t adler, const uint8_t *buf, size_t len);
uint32_t adler32_copy_c(uint32_t adler, uint8_t *dst, const uint8_t *src, size_t len);
#endif
#ifdef CHUNKSET_FALLBACK
uint8_t* chunkmemset_safe_c(uint8_t *out, uint8_t *from, size_t len, size_t left);
#endif
#ifdef COMPARE256_FALLBACK
uint32_t compare256_8(const uint8_t *src0, const uint8_t *src1);
uint32_t compare256_64(const uint8_t *src0, const uint8_t *src1);
uint32_t compare256_c(const uint8_t *src0, const uint8_t *src1);
#endif

#ifdef CRC32_BRAID_FALLBACK
uint32_t crc32_braid(uint32_t crc, const uint8_t *buf, size_t len);
uint32_t crc32_copy_braid(uint32_t crc, uint8_t *dst, const uint8_t *src, size_t len);
#endif

/* Chorba is available whenever braid is needed as a fallback and hasn't been disabled. */
#if defined(CRC32_BRAID_FALLBACK) && !defined(WITHOUT_CHORBA)
#  define CRC32_CHORBA_FALLBACK
#endif

#ifdef CRC32_CHORBA_FALLBACK
  uint32_t crc32_chorba(uint32_t crc, const uint8_t *buf, size_t len);
  uint32_t crc32_copy_chorba(uint32_t crc, uint8_t *dst, const uint8_t *src, size_t len);
#endif
#ifdef CHUNKSET_FALLBACK
void     inflate_fast_c(PREFIX3(stream) *strm, uint32_t start);
#endif
#ifdef COMPARE256_FALLBACK
uint32_t longest_match_c(deflate_state *const s, uint32_t cur_match);
uint32_t longest_match_roll_c(deflate_state *const s, uint32_t cur_match);
#endif
#ifdef SLIDE_HASH_FALLBACK
void     slide_hash_c(deflate_state *s);
#endif

#ifdef DISABLE_RUNTIME_CPU_DETECTION
// Generic fallbacks when no native implementation exists
#  ifdef ADLER32_FALLBACK
#    define native_adler32 adler32_c
#    define native_adler32_copy adler32_copy_c
#  endif
#  ifdef CHUNKSET_FALLBACK
#    define native_chunkmemset_safe chunkmemset_safe_c
#    define native_inflate_fast inflate_fast_c
#  endif
#  ifdef COMPARE256_FALLBACK
#    define native_compare256 compare256_c
#    define native_longest_match longest_match_c
#    define native_longest_match_roll longest_match_roll_c
#  endif
#  ifdef CRC32_CHORBA_FALLBACK
#    define native_crc32 crc32_chorba
#    define native_crc32_copy crc32_copy_chorba
#  elif defined(CRC32_BRAID_FALLBACK)
#    define native_crc32 crc32_braid
#    define native_crc32_copy crc32_copy_braid
#  endif
#  ifdef SLIDE_HASH_FALLBACK
#    define native_slide_hash slide_hash_c
#  endif
#endif

#endif
