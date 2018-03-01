/* gzendian.h -- define BYTE_ORDER for endian tests
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#ifndef ENDIAN_H_
#define ENDIAN_H_

#ifdef __MINGW32__
# include <sys/param.h>
#elif defined(WIN32) || defined(_WIN32)
# define LITTLE_ENDIAN 1234
# define BIG_ENDIAN 4321
# if defined(_M_IX86) || defined(_M_AMD64) || defined(_M_IA64) || defined (_M_ARM)
#  define BYTE_ORDER LITTLE_ENDIAN
# else
#  error Unknown endianness!
# endif
#elif defined(__APPLE__)
# include <machine/endian.h>
#elif defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__bsdi__) || defined(__DragonFly__)
# include <sys/endian.h>
#elif defined(__sun) || defined(sun)
# include <sys/byteorder.h>
# if !defined(LITTLE_ENDIAN)
#  define LITTLE_ENDIAN 4321
# endif
# if !defined(BIG_ENDIAN)
#  define BIG_ENDIAN 1234
# endif
# if !defined(BYTE_ORDER)
#  if defined(_BIG_ENDIAN)
#   define BYTE_ORDER BIG_ENDIAN
#  else
#   define BYTE_ORDER LITTLE_ENDIAN
#  endif
# endif
#else
# include <endian.h>
#endif

#endif
