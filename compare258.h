#ifndef COMPARE258_H_
#define COMPARE258_H_

/* compare258.h -- architecture specific versions of static compare258
 * Copyright (C) 2020 Nathan Moinvaziri
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#include "zbuild.h"
#include "zutil.h"

#ifdef X86_CPUID
#  include "fallback_builtins.h"
#  include "arch/x86/compare258_sse.h"
#  include "arch/x86/compare258_avx.h"
#endif

// ALIGNED, byte comparison
static inline int32_t compare258_static(const unsigned char *src0, const unsigned char *src1) {
    register const unsigned char *src0start = src0;
    register const unsigned char *src0end = src0 + 258; // 258 % 6 = 0

    do {
        if (*src0 != *src1)
            break;
        src0 += 1, src1 += 1;
        if (*src0 != *src1)
            break;
        src0 += 1, src1 += 1;
        if (*src0 != *src1)
            break;
        src0 += 1, src1 += 1;
        if (*src0 != *src1)
            break;
        src0 += 1, src1 += 1;
        if (*src0 != *src1)
            break;
        src0 += 1, src1 += 1;
        if (*src0 != *src1)
            break;
        src0 += 1, src1 += 1;
    } while (src0 < src0end);
 
    return (int32_t)(src0 - src0start);
}

// UNALIGNED_OK, 16-bit integer comparison
static inline int32_t compare258_unaligned_16_static(const unsigned char *src0, const unsigned char *src1) {
    register const unsigned char *src0start = src0;
    register const unsigned char *src0end = src0 + 258; // 258 % 6 = 0

    do {
        if (*(uint16_t *)src0 != *(uint16_t *)src1)
            break;
        src0 += 2, src1 += 2;
        if (*(uint16_t *)src0 != *(uint16_t *)src1)
            break;
        src0 += 2, src1 += 2;
        if (*(uint16_t *)src0 != *(uint16_t *)src1)
            break;
        src0 += 2, src1 += 2;
    } while (src0 < src0end);

    if (*src0 == *src1)
        src0 += 1;

    return (int32_t)(src0 - src0start);
}

#ifdef HAVE_BUILTIN_CTZ
// UNALIGNED_OK, 32-bit integer comparison
static inline int32_t compare258_unaligned_32_static(const unsigned char *src0, const unsigned char *src1) {
    register const unsigned char *src0start = src0;
    register const unsigned char *src0end = src0 + 258; // (258 - 2) % 4 = 0

    if (*(uint16_t *)src0 != *(uint16_t *)src1)
        return (*src0 == *src1);

    src0 += 2, src1 += 2;
    if (*src0 != *src1)
        return 2;
    if (src0[1] != src1[1])
        return 3;

    do {
        uint32_t *sv = (uint32_t *)src0;
        uint32_t *mv = (uint32_t *)src1;
        uint32_t xor = *sv ^ *mv;

        if (xor) {
            uint32_t match_byte = __builtin_ctz(xor) / 8;
            return (int32_t)(src0 - src0start + match_byte);
        }

        src0 += 4, src1 += 4;
    } while (src0 < src0end);

    return (int32_t)(src0 - src0start);
}
#endif

#ifdef HAVE_BUILTIN_CTZLL
// UNALIGNED_OK, 64-bit integer comparison
static inline int32_t compare258_unaligned_64_static(const unsigned char *src0, const unsigned char *src1) {
    register const unsigned char *src0start = src0;
    register const unsigned char *src0end = src0 + 258; // (258 - 2) % 8 = 0

    if (*(uint16_t *)src0 != *(uint16_t *)src1)
        return (*src0 == *src1);

    src0 += 2, src1 += 2;
    if (*src0 != *src1)
        return 2;
    if (src0[1] != src1[1])
        return 3;

    do {
        uint64_t *sv = (uint64_t *)src0;
        uint64_t *mv = (uint64_t *)src1;
        uint64_t xor = *sv ^ *mv;

        if (xor) {
            uint64_t match_byte = __builtin_ctzll(xor) / 8;
            return (int32_t)(src0 - src0start + match_byte);
        }

        src0 += 8, src1 += 8;
    } while (src0 < src0end);

    return (int32_t)(src0 - src0start);
}
#endif

#endif
