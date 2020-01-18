/* update_hash_sse -- update_hash variant using SSE4.2's CRC instructions
 *
 * Copyright (C) 1995-2013 Jean-loup Gailly and Mark Adler
 * For conditions of distribution and use, see copyright notice in zlib.h
 *
 */

#include "../../zbuild.h"
#include <immintrin.h>
#ifdef _MSC_VER
#  include <nmmintrin.h>
#endif
#include "../../deflate.h"

#ifdef X86_SSE42_CRC_HASH
ZLIB_INTERNAL uint32_t update_hash_sse(deflate_state *const s, uint32_t hash, uint32_t val) {
#if defined(X86_SSE42_CRC_INTRIN)
#  ifdef _MSC_VER
    hash = _mm_crc32_u32(hash, val);
#  else
    hash = __builtin_ia32_crc32si(hash, val);
#  endif
#else
#  ifdef _MSC_VER
    __asm {
        mov edx, hash
        mov eax, val
        crc32 eax, edx
        mov val, eax
    };
#  else
    __asm__ __volatile__ (
        "crc32 %1,%0\n\t"
        : "+r" (hash)
        : "r" (val)
    );
#  endif
#endif
    return hash;
}
#endif
