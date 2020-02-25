/* compare258_sse.c -- SSE4.2 version of extern compare258
 * Copyright (C) 2020 Nathan Moinvaziri
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#include "../../zbuild.h"
#include "../../zutil.h"

#include "compare258_sse.h"

#ifdef X86_SSE42_CMP_STR
// UNALIGNED_OK, SSE4.2 instrinic comparison
int32_t compare258_unaligned_sse(const unsigned char *src0, const unsigned char *src1) {
    return compare258_unaligned_sse_static(src0, src1);
}
#endif
