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
extern uint32_t adler32_neon(uint32_t adler, const uint8_t *buf, size_t len);
extern uint32_t chunksize_neon(void);
extern uint8_t* chunkmemset_safe_neon(uint8_t *out, unsigned dist, unsigned len, unsigned left);

#  ifdef HAVE_BUILTIN_CTZLL
    extern uint32_t compare256_neon(const uint8_t *src0, const uint8_t *src1);
#    ifdef DEFLATE_H_
        extern uint32_t longest_match_neon(deflate_state *const s, Pos cur_match);
        extern uint32_t longest_match_slow_neon(deflate_state *const s, Pos cur_match);
#    endif
#  endif
#  ifdef DEFLATE_H_
    extern void slide_hash_neon(deflate_state *s);
#  endif
#  ifdef INFLATE_H_
    extern void inflate_fast_neon(PREFIX3(stream) *strm, uint32_t start);
#  endif
#endif

#ifdef ARM_ACLE
extern uint32_t crc32_acle(uint32_t crc, const uint8_t *buf, size_t len);

#  ifdef DEFLATE_H_
    extern void insert_string_acle(deflate_state *const s, const uint32_t str, uint32_t count);
    extern Pos quick_insert_string_acle(deflate_state *const s, const uint32_t str);
    extern uint32_t update_hash_acle(deflate_state *const s, uint32_t h, uint32_t val);
#  endif
#endif

#ifdef ARM_SIMD
#  ifdef DEFLATE_H_
    extern void slide_hash_armv6(deflate_state *s);
#  endif
#endif

#endif

#endif /* ARM_H_ */
