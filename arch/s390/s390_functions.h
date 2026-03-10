/* s390_functions.h -- s390 implementations for arch-specific functions.
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#ifndef S390_FUNCTIONS_H_
#define S390_FUNCTIONS_H_

#include "s390_natives.h"

#define ADLER32_FALLBACK
#define CHUNKSET_FALLBACK
#define COMPARE256_FALLBACK
#define CRC32_BRAID_FALLBACK  /* used by crc32_s390_vx */

#ifndef S390_VX_NATIVE
#  define SLIDE_HASH_FALLBACK
#endif

#ifdef S390_VX
uint32_t crc32_s390_vx(uint32_t crc, const uint8_t *buf, size_t len);
uint32_t crc32_copy_s390_vx(uint32_t crc, uint8_t *dst, const uint8_t *src, size_t len);
void slide_hash_vx(deflate_state *s);

#ifdef __clang__
#  if ((__clang_major__ == 18) || (__clang_major__ == 19 && (__clang_minor__ < 1 || (__clang_minor__ == 1 && __clang_patchlevel__ < 2))))
# error CRC32-VX optimizations are broken due to compiler bug in Clang versions: 18.0.0 <= clang_version < 19.1.2. \
        Either disable the zlib-ng S390 VX optimizations, or switch to another compiler/compiler version.
#  endif
#endif

#endif

#ifdef DISABLE_RUNTIME_CPU_DETECTION
#  ifdef S390_VX_NATIVE
#    undef native_crc32
#    define native_crc32 crc32_s390_vx
#    undef native_crc32_copy
#    define native_crc32_copy crc32_copy_s390_vx
#    undef native_slide_hash
#    define native_slide_hash slide_hash_vx
#  endif
#endif

#endif
