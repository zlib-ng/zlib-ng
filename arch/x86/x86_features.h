/* x86_features.h -- check for CPU features
 * Copyright (C) 2013 Intel Corporation Jim Kukunas
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#ifndef X86_FEATURES_H_
#define X86_FEATURES_H_

struct x86_cpu_features {
    int has_avx2;
    int has_avx512;
    int has_avx512vnni;
    int has_sse2;
    int has_ssse3;
    int has_sse42;
    int has_pclmulqdq;
    int has_vpclmulqdq;
    int has_os_save_ymm;
    int has_os_save_zmm;
};

void Z_INTERNAL x86_check_features(struct x86_cpu_features *features);

#ifdef CPU_FEATURES_H_

#include "fallback_builtins.h"
#include "crc32.h"

#ifdef X86_SSE2
extern uint32_t chunksize_sse2(void);
extern uint8_t* chunkmemset_safe_sse2(uint8_t *out, unsigned dist, unsigned len, unsigned left);

#  ifdef HAVE_BUILTIN_CTZ
    extern uint32_t compare256_sse2(const uint8_t *src0, const uint8_t *src1);
#    ifdef DEFLATE_H_
        extern uint32_t longest_match_sse2(deflate_state *const s, Pos cur_match);
        extern uint32_t longest_match_slow_sse2(deflate_state *const s, Pos cur_match);
        extern void slide_hash_sse2(deflate_state *s);
#    endif
#  endif
#  ifdef INFLATE_H_
        extern void inflate_fast_sse2(PREFIX3(stream)* strm, uint32_t start);
#  endif
#endif

#ifdef X86_SSSE3
extern uint32_t adler32_ssse3(uint32_t adler, const uint8_t *buf, size_t len);
extern uint8_t* chunkmemset_safe_ssse3(uint8_t *out, unsigned dist, unsigned len, unsigned left);
#  ifdef INFLATE_H_
    extern void inflate_fast_ssse3(PREFIX3(stream) *strm, uint32_t start);
#  endif
#endif

#ifdef X86_SSE42
extern uint32_t adler32_fold_copy_sse42(uint32_t adler, uint8_t *dst, const uint8_t *src, size_t len);
#  ifdef DEFLATE_H_
    extern void insert_string_sse42(deflate_state *const s, const uint32_t str, uint32_t count);
    extern Pos quick_insert_string_sse42(deflate_state *const s, const uint32_t str);
    extern uint32_t update_hash_sse42(deflate_state *const s, uint32_t h, uint32_t val);
#  endif
#endif

#ifdef X86_AVX2
extern uint32_t adler32_avx2(uint32_t adler, const uint8_t *buf, size_t len);
extern uint32_t adler32_fold_copy_avx2(uint32_t adler, uint8_t *dst, const uint8_t *src, size_t len);
extern uint32_t chunksize_avx2(void);
extern uint8_t* chunkmemset_safe_avx2(uint8_t *out, unsigned dist, unsigned len, unsigned left);

#  ifdef HAVE_BUILTIN_CTZ
    extern uint32_t compare256_avx2(const uint8_t *src0, const uint8_t *src1);
#    ifdef DEFLATE_H_
        extern uint32_t longest_match_avx2(deflate_state *const s, Pos cur_match);
        extern uint32_t longest_match_slow_avx2(deflate_state *const s, Pos cur_match);
        extern void slide_hash_avx2(deflate_state *s);
#    endif
#  endif
#  ifdef INFLATE_H_
        extern void inflate_fast_avx2(PREFIX3(stream)* strm, uint32_t start);
#  endif
#endif
#ifdef X86_AVX512
extern uint32_t adler32_avx512(uint32_t adler, const uint8_t *buf, size_t len);
extern uint32_t adler32_fold_copy_avx512(uint32_t adler, uint8_t *dst, const uint8_t *src, size_t len);
#endif
#ifdef X86_AVX512VNNI
extern uint32_t adler32_avx512_vnni(uint32_t adler, const uint8_t *buf, size_t len);
extern uint32_t adler32_fold_copy_avx512_vnni(uint32_t adler, uint8_t *dst, const uint8_t *src, size_t len);
#endif

#ifdef X86_PCLMULQDQ_CRC
extern uint32_t crc32_fold_pclmulqdq_reset(crc32_fold *crc);
extern void     crc32_fold_pclmulqdq_copy(crc32_fold *crc, uint8_t *dst, const uint8_t *src, size_t len);
extern void     crc32_fold_pclmulqdq(crc32_fold *crc, const uint8_t *src, size_t len, uint32_t init_crc);
extern uint32_t crc32_fold_pclmulqdq_final(crc32_fold *crc);
extern uint32_t crc32_pclmulqdq(uint32_t crc32, const uint8_t *buf, size_t len);
#endif
#ifdef X86_VPCLMULQDQ_CRC
extern uint32_t crc32_fold_vpclmulqdq_reset(crc32_fold *crc);
extern void     crc32_fold_vpclmulqdq_copy(crc32_fold *crc, uint8_t *dst, const uint8_t *src, size_t len);
extern void     crc32_fold_vpclmulqdq(crc32_fold *crc, const uint8_t *src, size_t len, uint32_t init_crc);
extern uint32_t crc32_fold_vpclmulqdq_final(crc32_fold *crc);
extern uint32_t crc32_vpclmulqdq(uint32_t crc32, const uint8_t *buf, size_t len);
#endif

#endif

#endif /* CPU_H_ */
