/* arm_features.h -- check for ARM features.
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#ifndef ARM_H_
#define ARM_H_

struct arm_cpu_features {
    int has_simd;
    int has_neon;
    int has_crc32;
};

void Z_INTERNAL arm_check_features(struct arm_cpu_features *features);

#ifdef CPU_FEATURES_H_

#ifdef ARM_NEON
uint32_t adler32_neon(uint32_t adler, const uint8_t *buf, size_t len);
uint32_t chunksize_neon(void);
uint8_t* chunkmemset_safe_neon(uint8_t *out, unsigned dist, unsigned len, unsigned left);

#  ifdef HAVE_BUILTIN_CTZLL
    uint32_t compare256_neon(const uint8_t *src0, const uint8_t *src1);
#    ifdef DEFLATE_H_
        uint32_t longest_match_neon(deflate_state *const s, Pos cur_match);
        uint32_t longest_match_slow_neon(deflate_state *const s, Pos cur_match);
#    endif
#  endif
#  ifdef DEFLATE_H_
    void slide_hash_neon(deflate_state *s);
#  endif
#  ifdef INFLATE_H_
    void inflate_fast_neon(PREFIX3(stream) *strm, uint32_t start);
#  endif
#endif

#ifdef ARM_ACLE
uint32_t crc32_acle(uint32_t crc, const uint8_t *buf, size_t len);

#  ifdef DEFLATE_H_
    void insert_string_acle(deflate_state *const s, const uint32_t str, uint32_t count);
    Pos quick_insert_string_acle(deflate_state *const s, const uint32_t str);
    uint32_t update_hash_acle(deflate_state *const s, uint32_t h, uint32_t val);
#  endif
#endif

#ifdef ARM_SIMD
#  ifdef DEFLATE_H_
    void slide_hash_armv6(deflate_state *s);
#  endif
#endif

#endif


#ifdef DISABLE_RUNTIME_CPU_DETECTION
//// ARM - SIMD
#  if (defined(ARM_SIMD) && defined(__ARM_FEATURE_SIMD32)) || defined(ARM_NOCHECK_SIMD)
#    undef native_slide_hash
#    define native_slide_hash slide_hash_armv6
#  endif
// ARM - NEON
#  if (defined(ARM_NEON) && (defined(__ARM_NEON__) || defined(__ARM_NEON))) || ARM_NOCHECK_NEON
#    undef native_adler32
#    define native_adler32 adler32_neon
#    undef native_chunkmemset_safe
#    define native_chunkmemset_safe chunkmemset_safe_neon
#    undef native_chunksize
#    define native_chunksize chunksize_neon
#    undef native_inflate_fast
#    define native_inflate_fast inflate_fast_neon
#    undef native_slide_hash
#    define native_slide_hash slide_hash_neon
#    ifdef HAVE_BUILTIN_CTZLL
#      undef native_compare256
#      define native_compare256 compare256_neon
#      undef native_longest_match
#      define native_longest_match longest_match_neon
#      undef native_longest_match_slow
#      define native_longest_match_slow longest_match_slow_neon
#    endif
#  endif
// ARM - ACLE
#  if defined(ARM_ACLE) && defined(__ARM_ACLE) && defined(__ARM_FEATURE_CRC32)
#    undef native_crc32
#    define native_crc32 crc32_acle
#    undef native_insert_string
#    define native_insert_string insert_string_acle
#    undef native_quick_insert_string
#    define native_quick_insert_string quick_insert_string_acle
#    undef native_update_hash
#    define native_update_hash update_hash_acle
#  endif
#endif

#endif /* ARM_H_ */
