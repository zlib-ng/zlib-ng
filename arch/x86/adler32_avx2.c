/* adler32_avx2.c -- compute the Adler-32 checksum of a data stream
 * Copyright (C) 1995-2011 Mark Adler
 * Authors:
 *   Brian Bockelman <bockelman@gmail.com>
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#include "../../zbuild.h"
#include "../../adler32_p.h"
#include "../../fallback_builtins.h"
#include "adler32_avx2_p.h"
#include "../../adler32_fold.h"

#include <immintrin.h>

#ifdef X86_AVX2_ADLER32

Z_INTERNAL void adler32_fold_reset_avx2(adler32_fold *adler, uint32_t init_adler) {
    adler->nsums = init_adler;
}

Z_INTERNAL uint32_t adler32_fold_final_avx2(adler32_fold *adler) {
    return adler->nsums;
}

#include "adler32_avx2_tpl.h"
#undef ADLER32_AVX2_TPL_H_
#define COPY
#include "adler32_avx2_tpl.h"
#undef COPY

Z_INTERNAL uint32_t adler32_avx2(uint32_t adler, const unsigned char *buf, size_t len) {
    if (buf == NULL) return 1L;
    if (len == 0) return adler;
    ALIGNED_(64) adler32_fold fold;
    adler32_fold_reset_avx2(&fold, adler);
    adler32_fold_avx2(&fold, buf, len);
    return adler32_fold_final_avx2(&fold);
}

#endif
