/* adler32_avx512.c -- compute the Adler-32 checksum of a data stream
 * Copyright (C) 1995-2011 Mark Adler
 * Authors:
 *   Adam Stylinski <kungfujesus06@gmail.com>
 *   Brian Bockelman <bockelman@gmail.com>
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#ifdef X86_AVX512

#include "zbuild.h"
#include "adler32_avx512_tpl.h"

Z_INTERNAL uint32_t adler32_avx512(uint32_t adler, const uint8_t *src, size_t len) {
    return adler32_fold_impl(adler, NULL, src, len);
}


#define COPY
#include "adler32_avx512_tpl.h"

Z_INTERNAL uint32_t adler32_fold_copy_avx512(uint32_t adler, uint8_t *dst, const uint8_t *src, size_t len) {
    return adler32_fold_copy_impl(adler, dst, src, len);
}

#endif

