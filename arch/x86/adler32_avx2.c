/* adler32_avx2.c -- compute the Adler-32 checksum of a data stream
 * Copyright (C) 1995-2011 Mark Adler
 * Copyright (C) 2022 Adam Stylinski
 * Authors:
 *   Brian Bockelman <bockelman@gmail.com>
 *   Adam Stylinski <kungfujesus06@gmail.com>
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#ifdef X86_AVX2

#include "zbuild.h"

#include "adler32_avx2_tpl.h"

Z_INTERNAL uint32_t adler32_avx2(uint32_t adler, const uint8_t *src, size_t len) {
    return adler32_fold_impl(adler, NULL, src, len);
}

#define COPY
#include "adler32_avx2_tpl.h"

Z_INTERNAL uint32_t adler32_fold_copy_avx2(uint32_t adler, uint8_t *dst, const uint8_t *src, size_t len) {
    return adler32_fold_copy_impl(adler, dst, src, len);
}

#endif
