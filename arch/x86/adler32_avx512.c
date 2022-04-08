/* adler32_avx512.c -- compute the Adler-32 checksum of a data stream
 * Copyright (C) 1995-2011 Mark Adler
 * Authors:
 *   Adam Stylinski <kungfujesus06@gmail.com>
 *   Brian Bockelman <bockelman@gmail.com>
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#include "../../zbuild.h"
#include "../../adler32_p.h"
#include "../../cpu_features.h"
#include "../../fallback_builtins.h"
#include <immintrin.h>
#include "adler32_avx512_p.h"
#include "../../adler32_fold.h"

#ifdef X86_AVX512_ADLER32

#include "adler32_avx512_tpl.h"
#undef ADLER32_AVX512_TPL_H_
#define COPY
#include "adler32_avx512_tpl.h"
#undef COPY

#endif
