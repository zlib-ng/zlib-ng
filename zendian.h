/* zendian.h -- define BYTE_ORDER for endian tests
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#ifndef ENDIAN_H_
#define ENDIAN_H_

/* First check whether the compiler knows the target __BYTE_ORDER__. */
#if defined(__BYTE_ORDER__)
#  if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#    if !defined(LITTLE_ENDIAN)
#      define LITTLE_ENDIAN __ORDER_LITTLE_ENDIAN__
#    endif
#    if !defined(BYTE_ORDER)
#      define BYTE_ORDER LITTLE_ENDIAN
#    endif
#  elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#    if !defined(BIG_ENDIAN)
#      define BIG_ENDIAN __ORDER_BIG_ENDIAN__
#    endif
#    if !defined(BYTE_ORDER)
#      define BYTE_ORDER BIG_ENDIAN
#    endif
#  endif
#elif defined(__MINGW32__)
#  include <sys/param.h>
#elif defined(_WIN32)
#  define LITTLE_ENDIAN 1234
#  define BIG_ENDIAN 4321
#  if defined(ARCH_X86) || defined(ARCH_IA64) || defined(ARCH_ARM)
#    define BYTE_ORDER LITTLE_ENDIAN
#  else
#    error Unknown endianness!
#  endif
#elif defined(__linux__)
#  include <endian.h>
#elif defined(__APPLE__)
#  include <machine/endian.h>
#elif defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__bsdi__) || defined(__DragonFly__)
#  include <sys/endian.h>
#elif defined(__sun) || defined(sun)
#  include <sys/byteorder.h>
#  if !defined(LITTLE_ENDIAN)
#    define LITTLE_ENDIAN 4321
#   endif
#  if !defined(BIG_ENDIAN)
#    define BIG_ENDIAN 1234
#  endif
#  if !defined(BYTE_ORDER)
#    if defined(_BIG_ENDIAN)
#      define BYTE_ORDER BIG_ENDIAN
#    else
#      define BYTE_ORDER LITTLE_ENDIAN
#    endif
#  endif
#else
#  include <endian.h>
#endif

#if BYTE_ORDER == LITTLE_ENDIAN
#  define Z_U16_TO_LE(x)    (x)
#  define Z_U32_TO_LE(x)    (x)
#  define Z_U64_TO_LE(x)    (x)
#  define Z_U16_FROM_LE(x)  (x)
#  define Z_U32_FROM_LE(x)  (x)
#  define Z_U64_FROM_LE(x)  (x)
#  define Z_U16_TO_BE(x)    ZSWAP16(x)
#  define Z_U32_TO_BE(x)    ZSWAP32(x)
#  define Z_U64_TO_BE(x)    ZSWAP64(x)
#  define Z_U16_FROM_BE(x)  ZSWAP16(x)
#  define Z_U32_FROM_BE(x)  ZSWAP32(x)
#  define Z_U64_FROM_BE(x)  ZSWAP64(x)
#elif BYTE_ORDER == BIG_ENDIAN
#  define Z_U16_TO_LE(x)    ZSWAP16(x)
#  define Z_U32_TO_LE(x)    ZSWAP32(x)
#  define Z_U64_TO_LE(x)    ZSWAP64(x)
#  define Z_U16_FROM_LE(x)  ZSWAP16(x)
#  define Z_U32_FROM_LE(x)  ZSWAP32(x)
#  define Z_U64_FROM_LE(x)  ZSWAP64(x)
#  define Z_U16_TO_BE(x)    (x)
#  define Z_U32_TO_BE(x)    (x)
#  define Z_U64_TO_BE(x)    (x)
#  define Z_U16_FROM_BE(x)  (x)
#  define Z_U32_FROM_BE(x)  (x)
#  define Z_U64_FROM_BE(x)  (x)
#else
#  error "Unknown byte order"
#endif

#endif
