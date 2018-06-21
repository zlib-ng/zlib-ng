 /* x86.h -- check for CPU features
 * Copyright (C) 2013 Intel Corporation Jim Kukunas
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#ifndef X86_H
#define X86_H

#if defined(__x86_64__) || defined(_M_X64)
# define ZLIB_X86 64
#endif

#if defined(_M_IX86) || defined(__i386)
# define ZLIB_X86 32
#endif

#ifdef ZLIB_X86

#define UNALIGNED_OK
#define ADLER32_UNROLL_LESS
#define CRC32_UNROLL_LESS
#define USE_SSE_SLIDE

extern int x86_cpu_has_sse2;
extern int x86_cpu_has_sse42;
extern int x86_cpu_has_pclmul;

void x86_check_features(void);

#endif
#endif
