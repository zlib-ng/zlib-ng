/* zutil_p.h -- Private inline functions used internally in zlib-ng
 *
 */

#ifndef ZUTIL_P_H
#define ZUTIL_P_H

#if !defined(MAL_IMPL) && !defined(_WIN32)
#  if __STDC_VERSION__+0 >= 201112L
#    define MAL_IMPL 2 /* ISO C11 `aligned_alloc` */
#  elif _POSIX_C_SOURCE+0 >= 200112L
#    define MAL_IMPL 1 /* POSIX.1-2001 optional `posix_memalign` */
#  else
#    pragma message("No portable function for aligned memory allocation found, defaulting to memalign()")
#  endif
#endif

#include <stddef.h> /* NULL, size_t */
#if MAL_IMPL+0 != 0
#  include <stdlib.h> /* aligned_alloc, free, posix_memalign */
#else
#  include <malloc.h> /* _aligned_malloc, _aligned_free, free, memalign */
#endif

/* Function to allocate 64-byte aligned memory */
static inline void *zng_alloc(size_t size) {
#if defined(_WIN32)
    return _aligned_malloc(size, 64);
#elif MAL_IMPL == 2
    return aligned_alloc(64, size);
#elif MAL_IMPL == 1
    void *ptr;
    return posix_memalign(&ptr, 64, size) ? NULL : ptr;
#else
    return memalign(64, size);
#endif
}

/* Function that can free aligned memory */
static inline void zng_free(void *ptr) {
#if defined(_WIN32)
    _aligned_free(ptr);
#else
    free(ptr);
#endif
}

#endif
