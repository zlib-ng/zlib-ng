/* riscv_functions.h -- RISCV implementations for arch-specific functions.
 *
 * Copyright (C) 2023 SiFive, Inc. All rights reserved.
 * Contributed by Alex Chiang <alex.chiang@sifive.com>
 *
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#ifndef RISCV_FUNCTIONS_H_
#define RISCV_FUNCTIONS_H_

#include "riscv_natives.h"

#define CRC32_BRAID_FALLBACK  /* used by crc32_zbc */

#ifdef RISCV_RVV
uint32_t adler32_rvv(uint32_t adler, const uint8_t *buf, size_t len);
uint32_t adler32_copy_rvv(uint32_t adler, uint8_t *dst, const uint8_t *src, size_t len);
uint8_t* chunkmemset_safe_rvv(uint8_t *out, uint8_t *from, size_t len, size_t left);
uint32_t compare256_rvv(const uint8_t *src0, const uint8_t *src1);

uint32_t longest_match_rvv(deflate_state *const s, uint32_t cur_match);
uint32_t longest_match_slow_rvv(deflate_state *const s, uint32_t cur_match);
void slide_hash_rvv(deflate_state *s);
void inflate_fast_rvv(PREFIX3(stream) *strm, uint32_t start);
#endif

#ifndef RISCV_RVV_NATIVE
#  define ADLER32_FALLBACK
#  define CHUNKSET_FALLBACK
#  define COMPARE256_FALLBACK
#  define SLIDE_HASH_FALLBACK
#endif

#ifdef RISCV_CRC32_ZBC
uint32_t crc32_riscv64_zbc(uint32_t crc, const uint8_t *buf, size_t len);
uint32_t crc32_copy_riscv64_zbc(uint32_t crc, uint8_t *dst, const uint8_t *src, size_t len);
#endif

#ifdef DISABLE_RUNTIME_CPU_DETECTION
// RISCV - RVV
#  ifdef RISCV_RVV_NATIVE
#    define native_adler32 adler32_rvv
#    define native_adler32_copy adler32_copy_rvv
#    define native_chunkmemset_safe chunkmemset_safe_rvv
#    define native_compare256 compare256_rvv
#    define native_inflate_fast inflate_fast_rvv
#    define native_longest_match longest_match_rvv
#    define native_longest_match_slow longest_match_slow_rvv
#    define native_slide_hash slide_hash_rvv
#  endif
// RISCV - CRC32
#  ifdef RISCV_ZBC_NATIVE
#    undef native_crc32
#    define native_crc32 crc32_riscv64_zbc
#    undef native_crc32_copy
#    define native_crc32_copy crc32_copy_riscv64_zbc
#  endif
#endif

#endif /* RISCV_FUNCTIONS_H_ */
