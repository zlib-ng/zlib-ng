/* compare258_avx.c -- AVX version of extern compare258
 * Copyright (C) 2020 Nathan Moinvaziri
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#include "../../zbuild.h"
#include "../../zutil.h"

#include "compare258_avx.h"

#ifdef X86_AVX2
// UNALIGNED_OK, AVX2 instrinic comparison
int32_t compare258_unaligned_avx(const unsigned char *src0, const unsigned char *src1) {
    return compare258_unaligned_avx_static(src0, src1);
}
#endif
