/* riscv_functions.h -- RISCV implementations for arch-specific functions.
 *
 * Copyright (C) 2023 SiFive, Inc. All rights reserved.
 * Contributed by Alex Chiang <alex.chiang@sifive.com>
 *
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#ifndef RISCV_FUNCTIONS_H_
#define RISCV_FUNCTIONS_H_

#ifdef RISCV_RVV
uint32_t adler32_rvv(uint32_t adler, const uint8_t *buf, size_t len);
uint32_t adler32_fold_copy_rvv(uint32_t adler, uint8_t *dst, const uint8_t *src, size_t len);
uint32_t chunksize_rvv(void);
uint8_t* chunkmemset_safe_rvv(uint8_t *out, unsigned dist, unsigned len, unsigned left);
uint32_t compare256_rvv(const uint8_t *src0, const uint8_t *src1);

uint32_t longest_match_rvv(deflate_state *const s, Pos cur_match);
uint32_t longest_match_slow_rvv(deflate_state *const s, Pos cur_match);
void slide_hash_rvv(deflate_state *s);
void inflate_fast_rvv(PREFIX3(stream) *strm, uint32_t start);
#endif

#endif /* RISCV_FUNCTIONS_H_ */
