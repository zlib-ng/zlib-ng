/* zutil_p.h -- Private inline functions used internally in zlib-ng
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#ifndef ZUTIL_P_H
#define ZUTIL_P_H

#if defined(HAVE_POSIX_MEMALIGN) || defined(HAVE_ALIGNED_ALLOC)
#  include <stdlib.h>
#elif defined(__FreeBSD__)
#  include <stdlib.h>
#  include <malloc_np.h>
#elif defined(HAVE_MEMALIGN) || defined(HAVE__ALIGNED_MALLOC)
#  include <malloc.h>
#else
/* Fallback, when no other aligned allocation function was found */
#  include <stdlib.h>
#endif

/* Function to allocate 16 or 64-byte aligned memory */
static inline void *zng_alloc(size_t size) {
#ifdef HAVE_ALIGNED_ALLOC
    /* Size must be a multiple of alignment */
    size = (size + (64 - 1)) & ~(64 - 1);
    return (void *)aligned_alloc(64, size);  /* Defined in C11 */
#elif defined(HAVE_POSIX_MEMALIGN)
    void *ptr;
    return posix_memalign(&ptr, 64, size) ? NULL : ptr;
#elif defined(HAVE__ALIGNED_MALLOC)
    /* Fallback: Use MSVC extensions: _aligned_malloc() / _aligned_free() */
    return (void *)_aligned_malloc(size, 64);
#elif defined(HAVE_MEMALIGN)
    /* Fallback: Use deprecated GNU extension: memalign() */
    return (void *)memalign(64, size);
#else
    /* Fallback: Use a normal allocation (On macOS, alignment is 16 bytes) */
    /* zlib-ng adjust allocations for [de]compression to be properly aligned */
    return (void *)malloc(size);
#endif
}

/* Function that can free aligned memory */
static inline void zng_free(void *ptr) {
#if defined(HAVE__ALIGNED_MALLOC) && !defined(HAVE_POSIX_MEMALIGN) && !defined(HAVE_ALIGNED_ALLOC)
    _aligned_free(ptr);
#else
    free(ptr);
#endif
}

#endif
