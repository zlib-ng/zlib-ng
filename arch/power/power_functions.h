/* power_functions.h -- POWER implementations for arch-specific functions.
 * Copyright (C) 2020 Matheus Castanho <msc@linux.ibm.com>, IBM
 * Copyright (C) 2021 Mika T. Lindqvist <postmaster@raasu.org>
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#ifndef POWER_FUNCTIONS_H_
#define POWER_FUNCTIONS_H_

#ifdef PPC_VMX
uint32_t adler32_vmx(uint32_t adler, const uint8_t *buf, size_t len);
void slide_hash_vmx(deflate_state *s);
#endif

#ifdef POWER8_VSX
uint32_t adler32_power8(uint32_t adler, const uint8_t *buf, size_t len);
uint32_t chunksize_power8(void);
uint8_t* chunkmemset_safe_power8(uint8_t *out, unsigned dist, unsigned len, unsigned left);
uint32_t crc32_power8(uint32_t crc, const uint8_t *buf, size_t len);
void slide_hash_power8(deflate_state *s);
void inflate_fast_power8(PREFIX3(stream) *strm, uint32_t start);
#endif

#ifdef POWER9
uint32_t compare256_power9(const uint8_t *src0, const uint8_t *src1);
uint32_t longest_match_power9(deflate_state *const s, Pos cur_match);
uint32_t longest_match_slow_power9(deflate_state *const s, Pos cur_match);
#endif

#endif /* POWER_FUNCTIONS_H_ */
