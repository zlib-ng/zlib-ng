/* adler32_sse4.c -- compute the Adler-32 checksum of a data stream
 * Copyright (C) 1995-2011 Mark Adler
 * Authors:
 *   Adam Stylinski <kungfujesus06@gmail.com>
 *   Brian Bockelman <bockelman@gmail.com>
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#include "../../zbuild.h"
#include "../../adler32_p.h"
#include "../../adler32_fold.h"
#include "adler32_ssse3_p.h"
#include <immintrin.h>

#ifdef X86_SSE42_ADLER32

Z_INTERNAL void adler32_fold_reset_sse42(adler32_fold *adler, uint32_t init_adler) {
    adler->nsums = init_adler;
}

Z_INTERNAL uint32_t adler32_fold_final_sse42(adler32_fold *adler) {
    return adler->nsums;
}

#include "adler32_sse42_tpl.h"
#undef ADLER32_SSE42_TPL_H_
#define COPY
#include "adler32_sse42_tpl.h"
#undef COPY

#endif
