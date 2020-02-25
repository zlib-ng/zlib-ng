/* compare258.c -- architecture specific versions of extern compare258
 * Copyright (C) 2020 Nathan Moinvaziri
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#include "zbuild.h"
#include "zutil.h"

#include "compare258.h"

// ALIGNED, byte comparison
int32_t compare258(const unsigned char *src0, const unsigned char *src1) {
    return compare258_static(src0, src1);
}

// UNALIGNED_OK, 16-bit integer comparison
int32_t compare258_unaligned_16(const unsigned char *src0, const unsigned char *src1) {
    return compare258_unaligned_16_static(src0, src1);
}

#ifdef HAVE_BUILTIN_CTZ
// UNALIGNED_OK, 32-bit integer comparison
int32_t compare258_unaligned_32(const unsigned char *src0, const unsigned char *src1) {
    return compare258_unaligned_32_static(src0, src1);
}
#endif

#ifdef HAVE_BUILTIN_CTZLL
// UNALIGNED_OK, 64-bit integer comparison
int32_t compare258_unaligned_64(const unsigned char *src0, const unsigned char *src1) {
    return compare258_unaligned_64_static(src0, src1);
}
#endif
