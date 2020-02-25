/* match.h -- inline versions of longest_match 
 * Copyright (C) 2020 Nathan Moinvaziri
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#include "zbuild.h"
#include "deflate.h"
#include "compare258.h"

#ifdef UNALIGNED_OK
#if MIN_MATCH >= 4
typedef uint32_t        bestcmp_t;
#elif MIN_MATCH > 1
typedef uint16_t        bestcmp_t;
#else
typedef uint8_t         bestcmp_t;
#endif

#define LONGEST_MATCH   longest_match_unaligned_16
#undef  COMPARE258
#define COMPARE258      compare258_unaligned_16_static
#include "match_p.h"

#ifdef HAVE_BUILTIN_CTZ
#undef  LONGEST_MATCH
#define LONGEST_MATCH   longest_match_unaligned_32
#undef  COMPARE258
#define COMPARE258      compare258_unaligned_32_static
#include "match_p.h"
#endif

#ifdef HAVE_BUILTIN_CTZLL
#undef  LONGEST_MATCH
#define LONGEST_MATCH   longest_match_unaligned_64
#undef  COMPARE258
#define COMPARE258      compare258_unaligned_64_static
#include "match_p.h"
#endif

#ifdef X86_AVX2
#undef  LONGEST_MATCH
#define LONGEST_MATCH   longest_match_unaligned_avx
#undef  COMPARE258
#define COMPARE258      compare258_unaligned_avx_static
#include "match_p.h"
#endif

#ifdef X86_SSE42_CMP_STR
#undef  LONGEST_MATCH
#define LONGEST_MATCH   longest_match_unaligned_sse
#undef  COMPARE258
#define COMPARE258      compare258_unaligned_sse_static
#include "match_p.h"
#endif

#else
typedef uint8_t         bestcmp_t;
#define LONGEST_MATCH   longest_match
#define COMPARE258      compare258_static

#include "match_p.h"
#endif
