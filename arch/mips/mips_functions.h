/* mips_functions.h -- MIPS implementations for arch-specific functions.
 * Copyright (C) 2026 Mika T. Lindqvist <postmaster@raasu.org>
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#ifndef MIPS_FUNCTIONS_H_
#define MIPS_FUNCTIONS_H_

#include "mips_natives.h"

#ifdef MIPS_MSA
uint32_t adler32_msa(uint32_t adler, const uint8_t *buf, size_t len);
uint32_t adler32_copy_msa(uint32_t adler, uint8_t *dst, const uint8_t *src, size_t len);
void slide_hash_msa(deflate_state *s);
#endif

#define CHUNKSET_FALLBACK
#define COMPARE256_FALLBACK
#define CRC32_BRAID_FALLBACK

#if !defined(MIPS_MSA_NATIVE)
#  define ADLER32_FALLBACK
#  define SLIDE_HASH_FALLBACK
#endif

#ifdef DISABLE_RUNTIME_CPU_DETECTION
// MIPS - MSA
#  ifdef MIPS_MSA_NATIVE
#    undef native_adler32
#    define native_adler32 adler32_msa
#    undef native_adler32_copy
#    define native_adler32_copy adler32_copy_msa
#    undef native_slide_hash
#    define native_slide_hash slide_hash_msa
#  endif
#endif

#endif /* MIPS_FUNCTIONS_H_ */
