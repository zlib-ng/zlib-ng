/* util_shared.h -- Portability macros shared by the command-line utilities
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#ifndef UTIL_SHARED_H
#define UTIL_SHARED_H

#include <stdio.h>

#if defined(_WIN32) || defined(__CYGWIN__)
#  include <fcntl.h>
#  include <io.h>
#  define SET_BINARY_MODE(file) setmode(fileno(file), O_BINARY)
#else
#  define SET_BINARY_MODE(file)
#endif

/* Default read/write i/o buffer size based on GZBUFSIZE */
#ifndef GZBUFSIZE
#  define GZBUFSIZE 131072
#endif
/* CVE2018-* checks fail if buffer is not at least 48K */
#if GZBUFSIZE < 49152
#  undef GZBUFSIZE
#  define GZBUFSIZE 49152
#endif

#endif
