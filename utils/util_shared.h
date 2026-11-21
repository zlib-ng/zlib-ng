/* util_shared.h -- Portability macros shared by the command-line utilities
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#ifndef UTIL_SHARED_H
#define UTIL_SHARED_H

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

#if defined(_WIN32)
#  if !defined(MAXPATHLEN)
#    define MAXPATHLEN _MAX_PATH
#  endif
#else
#  include <sys/param.h>
#endif

/* ===========================================================================
 * Sanitize file path or return NULL on error
 */
static inline char *sanitize_path(const char *in, char *out) {
    if (strlen(in) > MAXPATHLEN)
        return NULL;
#ifdef _WIN32
    return _fullpath(out, in, MAXPATHLEN);
#else
    return realpath(in, out); /* flawfinder: ignore */
#endif
}

#endif
