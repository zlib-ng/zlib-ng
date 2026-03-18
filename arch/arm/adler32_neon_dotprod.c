/* Copyright (C) 1995-2011, 2016 Mark Adler
 * Copyright (C) 2017 ARM Holdings Inc.
 * Copyright (C) 2025 Nathan Moinvaziri
 * Authors:
 *   Adenilson Cavalcanti <adenilson.cavalcanti@arm.com>
 *   Adam Stylinski <kungfujesus06@gmail.com>
 *   Nathan Moinvaziri <nathan@nathanm.com>
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#if defined(ARM_NEON) && defined(ARM_NEON_DOTPROD)

#define USE_DOTPROD
#include "adler32_neon_tpl.h"

Z_INTERNAL uint32_t adler32_neon_dotprod(uint32_t adler, const uint8_t *src, size_t len) {
    return adler32_copy_impl(adler, NULL, src, len, 0);
}

Z_INTERNAL uint32_t adler32_copy_neon_dotprod(uint32_t adler, uint8_t *dst, const uint8_t *src, size_t len) {
#if OPTIMAL_CMP >= 32
    return adler32_copy_impl(adler, dst, src, len, 1);
#else
    /* Without unaligned access, interleaved stores get decomposed into byte ops */
    adler = adler32_neon_dotprod(adler, src, len);
    memcpy(dst, src, len);
    return adler;
#endif
}

#endif
