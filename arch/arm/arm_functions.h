/* arm_functions.h -- ARM implementations for arch-specific functions.
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#ifndef ARM_FUNCTIONS_H_
#define ARM_FUNCTIONS_H_


#ifdef ARM_NEON
uint32_t adler32_neon(uint32_t adler, const uint8_t *buf, size_t len);
uint32_t chunksize_neon(void);
uint8_t* chunkmemset_safe_neon(uint8_t *out, unsigned dist, unsigned len, unsigned left);

#  ifdef HAVE_BUILTIN_CTZLL
uint32_t compare256_neon(const uint8_t *src0, const uint8_t *src1);
uint32_t longest_match_neon(deflate_state *const s, Pos cur_match);
uint32_t longest_match_slow_neon(deflate_state *const s, Pos cur_match);
#  endif
void slide_hash_neon(deflate_state *s);
void inflate_fast_neon(PREFIX3(stream) *strm, uint32_t start);
#endif

#ifdef ARM_ACLE
uint32_t crc32_acle(uint32_t crc, const uint8_t *buf, size_t len);

void insert_string_acle(deflate_state *const s, const uint32_t str, uint32_t count);
Pos quick_insert_string_acle(deflate_state *const s, const uint32_t str);
uint32_t update_hash_acle(deflate_state *const s, uint32_t h, uint32_t val);
#endif

#ifdef ARM_SIMD
void slide_hash_armv6(deflate_state *s);
#endif

#endif /* ARM_FUNCTIONS_H_ */
