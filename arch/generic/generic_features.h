/* generic_features.h -- generic C implementations for arch-specific features
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#ifndef GENERIC_FEATURES_H_
#define GENERIC_FEATURES_H_

typedef uint32_t (*adler32_func)(uint32_t adler, const uint8_t *buf, size_t len);
typedef uint32_t (*compare256_func)(const uint8_t *src0, const uint8_t *src1);
typedef uint32_t (*crc32_func)(uint32_t crc32, const uint8_t *buf, size_t len);

uint32_t adler32_c(uint32_t adler, const uint8_t *buf, size_t len);
uint32_t chunksize_c(void);
uint8_t* chunkmemset_safe_c(uint8_t *out, unsigned dist, unsigned len, unsigned left);
#ifdef INFLATE_H_
void     inflate_fast_c(PREFIX3(stream) *strm, uint32_t start);
#endif

uint32_t PREFIX(crc32_braid)(uint32_t crc, const uint8_t *buf, size_t len);

uint32_t compare256_c(const uint8_t *src0, const uint8_t *src1);
#if defined(UNALIGNED_OK) && BYTE_ORDER == LITTLE_ENDIAN
uint32_t compare256_unaligned_16(const uint8_t *src0, const uint8_t *src1);
#  ifdef HAVE_BUILTIN_CTZ
    uint32_t compare256_unaligned_32(const uint8_t *src0, const uint8_t *src1);
#  endif
#  if defined(UNALIGNED64_OK) && defined(HAVE_BUILTIN_CTZLL)
    uint32_t compare256_unaligned_64(const uint8_t *src0, const uint8_t *src1);
#  endif
#endif

#ifdef DEFLATE_H_
typedef void    (*slide_hash_func)(deflate_state *s);

void     insert_string_c(deflate_state *const s, const uint32_t str, uint32_t count);
Pos      quick_insert_string_c(deflate_state *const s, const uint32_t str);
void     slide_hash_c(deflate_state *s);
uint32_t update_hash_c(deflate_state *const s, uint32_t h, uint32_t val);

uint32_t longest_match_c(deflate_state *const s, Pos cur_match);
#  if defined(UNALIGNED_OK) && BYTE_ORDER == LITTLE_ENDIAN
    uint32_t longest_match_unaligned_16(deflate_state *const s, Pos cur_match);
#    ifdef HAVE_BUILTIN_CTZ
        uint32_t longest_match_unaligned_32(deflate_state *const s, Pos cur_match);
#    endif
#    if defined(UNALIGNED64_OK) && defined(HAVE_BUILTIN_CTZLL)
        uint32_t longest_match_unaligned_64(deflate_state *const s, Pos cur_match);
#    endif
#  endif

uint32_t longest_match_slow_c(deflate_state *const s, Pos cur_match);
#  if defined(UNALIGNED_OK) && BYTE_ORDER == LITTLE_ENDIAN
    uint32_t longest_match_slow_unaligned_16(deflate_state *const s, Pos cur_match);
    uint32_t longest_match_slow_unaligned_32(deflate_state *const s, Pos cur_match);
#    ifdef UNALIGNED64_OK
        uint32_t longest_match_slow_unaligned_64(deflate_state *const s, Pos cur_match);
#    endif
#  endif

#endif


#if defined(UNALIGNED_OK) && BYTE_ORDER == LITTLE_ENDIAN
#  if defined(UNALIGNED64_OK) && defined(HAVE_BUILTIN_CTZLL)
#    define longest_match_generic longest_match_unaligned_64
#    define longest_match_slow_generic longest_match_slow_unaligned_64
#    define compare256_generic compare256_unaligned_64
#  elif defined(HAVE_BUILTIN_CTZ)
#    define longest_match_generic longest_match_unaligned_32
#    define longest_match_slow_generic longest_match_slow_unaligned_32
#    define compare256_generic compare256_unaligned_32
#  else
#    define longest_match_generic longest_match_unaligned_16
#    define longest_match_slow_generic longest_match_slow_unaligned_16
#    define compare256_generic compare256_unaligned_16
#  endif
#else
#  define longest_match_generic longest_match_c
#  define longest_match_slow_generic longest_match_slow_c
#  define compare256_generic compare256_c
#endif


#ifdef DISABLE_RUNTIME_CPU_DETECTION
// Generic code
#  define native_adler32 adler32_c
#  define native_adler32_fold_copy adler32_fold_copy_c
#  define native_chunkmemset_safe chunkmemset_safe_c
#  define native_chunksize chunksize_c
#  define native_crc32 PREFIX(crc32_braid)
#  define native_crc32_fold crc32_fold_c
#  define native_crc32_fold_copy crc32_fold_copy_c
#  define native_crc32_fold_final crc32_fold_final_c
#  define native_crc32_fold_reset crc32_fold_reset_c
#  define native_inflate_fast inflate_fast_c
#  define native_insert_string insert_string_c
#  define native_quick_insert_string quick_insert_string_c
#  define native_slide_hash slide_hash_c
#  define native_update_hash update_hash_c
#  define native_longest_match longest_match_generic
#  define native_longest_match_slow longest_match_slow_generic
#  define native_compare256 compare256_generic
#endif


#endif
