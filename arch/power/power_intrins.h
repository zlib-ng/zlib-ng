/* Helper functions to work around issues with clang builtins
 * Copyright (C) 2021 IBM Corporation
 *
 * Authors:
 *   Daniel Black <daniel@linux.vnet.ibm.com>
 *   Rogerio Alves <rogealve@br.ibm.com>
 *   Tulio Magno Quites Machado Filho <tuliom@linux.ibm.com>
 *
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#ifndef POWER_INTRINS_H
#define POWER_INTRINS_H

#include <altivec.h>

#if defined (__clang__)
/*
 * These stubs fix clang incompatibilities with GCC builtins.
 */

#ifndef __builtin_crypto_vpmsumw
#define __builtin_crypto_vpmsumw __builtin_crypto_vpmsumb
#endif
#ifndef __builtin_crypto_vpmsumd
#define __builtin_crypto_vpmsumd __builtin_crypto_vpmsumb
#endif

#ifdef __VSX__
static inline __vector unsigned long long __attribute__((overloadable))
vec_ld(int __a, const __vector unsigned long long* __b) {
    return (__vector unsigned long long)__builtin_altivec_lvx(__a, __b);
}
#endif

#endif

/* There's no version of this that operates over unsigned and if casted, it does
 * sign extension. Let's write an endian independent version and hope the compiler
 * eliminates creating another zero idiom for the zero value if one exists locally */
static inline vector unsigned short vec_unpackl(vector unsigned char a) {
    vector unsigned char zero = vec_splat_u8(0);

#if BYTE_ORDER == BIG_ENDIAN
    return (vector unsigned short)vec_mergel(zero, a);
#else
    return (vector unsigned short)vec_mergel(a, zero);
#endif
}

static inline vector unsigned short vec_unpackh(vector unsigned char a) {
    vector unsigned char zero = vec_splat_u8(0);

#if BYTE_ORDER == BIG_ENDIAN
    return (vector unsigned short)vec_mergeh(zero, a);
#else
    return (vector unsigned short)vec_mergeh(a, zero);
#endif
}

#endif
