/* generic_features.h -- generic C implementations for arch-specific features
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#ifndef GENERIC_FEATURES_H_
#define GENERIC_FEATURES_H_

typedef uint32_t (*adler32_func)(uint32_t adler, const uint8_t *buf, size_t len);
typedef uint32_t (*compare256_func)(const uint8_t *src0, const uint8_t *src1);
typedef uint32_t (*crc32_func)(uint32_t crc32, const uint8_t *buf, size_t len);

extern uint32_t adler32_c(uint32_t adler, const uint8_t *buf, size_t len);
extern uint32_t chunksize_c(void);
extern uint8_t* chunkmemset_safe_c(uint8_t *out, unsigned dist, unsigned len, unsigned left);
#ifdef INFLATE_H_
extern void     inflate_fast_c(PREFIX3(stream) *strm, uint32_t start);
#endif

extern uint32_t PREFIX(crc32_braid)(uint32_t crc, const uint8_t *buf, size_t len);

extern uint32_t compare256_c(const uint8_t *src0, const uint8_t *src1);
#if defined(UNALIGNED_OK) && BYTE_ORDER == LITTLE_ENDIAN
extern uint32_t compare256_unaligned_16(const uint8_t *src0, const uint8_t *src1);
#  ifdef HAVE_BUILTIN_CTZ
    extern uint32_t compare256_unaligned_32(const uint8_t *src0, const uint8_t *src1);
#  endif
#  if defined(UNALIGNED64_OK) && defined(HAVE_BUILTIN_CTZLL)
    extern uint32_t compare256_unaligned_64(const uint8_t *src0, const uint8_t *src1);
#  endif
#endif

#ifdef DEFLATE_H_
typedef void    (*slide_hash_func)(deflate_state *s);

extern void     insert_string_c(deflate_state *const s, const uint32_t str, uint32_t count);
extern Pos      quick_insert_string_c(deflate_state *const s, const uint32_t str);
extern void     slide_hash_c(deflate_state *s);
extern uint32_t update_hash_c(deflate_state *const s, uint32_t h, uint32_t val);

extern uint32_t longest_match_c(deflate_state *const s, Pos cur_match);
#  if defined(UNALIGNED_OK) && BYTE_ORDER == LITTLE_ENDIAN
    extern uint32_t longest_match_unaligned_16(deflate_state *const s, Pos cur_match);
#    ifdef HAVE_BUILTIN_CTZ
        extern uint32_t longest_match_unaligned_32(deflate_state *const s, Pos cur_match);
#    endif
#    if defined(UNALIGNED64_OK) && defined(HAVE_BUILTIN_CTZLL)
        extern uint32_t longest_match_unaligned_64(deflate_state *const s, Pos cur_match);
#    endif
#  endif

extern uint32_t longest_match_slow_c(deflate_state *const s, Pos cur_match);
#  if defined(UNALIGNED_OK) && BYTE_ORDER == LITTLE_ENDIAN
    extern uint32_t longest_match_slow_unaligned_16(deflate_state *const s, Pos cur_match);
    extern uint32_t longest_match_slow_unaligned_32(deflate_state *const s, Pos cur_match);
#    ifdef UNALIGNED64_OK
        extern uint32_t longest_match_slow_unaligned_64(deflate_state *const s, Pos cur_match);
#    endif
#  endif

#endif

#endif
