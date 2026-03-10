/* zsanitizer.h -- sanitizer instrumentation for deliberate OOB and uninitialized memory access.
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#ifndef ZSANITIZER_H
#define ZSANITIZER_H

/* Detect sanitizer availability */
#if defined(__has_feature)
#  if __has_feature(address_sanitizer)
#    define Z_ADDRESS_SANITIZER 1
#  endif
#  if __has_feature(memory_sanitizer)
#    define Z_MEMORY_SANITIZER 1
#  endif
#elif defined(__SANITIZE_ADDRESS__)  /* GCC supports ASAN only */
#  define Z_ADDRESS_SANITIZER 1
#endif

/* Include sanitizer headers */
#ifdef Z_MEMORY_SANITIZER
#  include <sanitizer/msan_interface.h>
#endif

/*
 * __asan_loadN() and __asan_storeN() calls are inserted by compilers in order to check memory accesses.
 * They can be called manually too, with the following caveats:
 * gcc says: "warning: implicit declaration of function '...'"
 * g++ says: "error: new declaration '...' ambiguates built-in declaration '...'"
 * Accommodate both.
 */
#ifdef Z_ADDRESS_SANITIZER
#  ifndef __cplusplus
void __asan_loadN(uintptr_t, uintptr_t);
void __asan_storeN(uintptr_t, uintptr_t);
#  endif
#else
#  define __asan_loadN(a, size) do { Z_UNUSED(a); Z_UNUSED(size); } while (0)
#  define __asan_storeN(a, size) do { Z_UNUSED(a); Z_UNUSED(size); } while (0)
#endif

/* Handle intentional uninitialized memory access */
#ifndef Z_MEMORY_SANITIZER
#  define __msan_check_mem_is_initialized(a, size) do { Z_UNUSED(a); Z_UNUSED(size); } while (0)
#  define __msan_unpoison(a, size) do { Z_UNUSED(a); Z_UNUSED(size); } while (0)
#endif

/* Notify sanitizer runtime about an upcoming read access. */
#define instrument_read(a, size) do {                     \
    uintptr_t __a = (uintptr_t)(a);                       \
    uintptr_t __size = size;                              \
    __asan_loadN(__a, __size);                            \
    __msan_check_mem_is_initialized((void *)__a, __size); \
} while (0)

/* Notify sanitizer runtime about an upcoming write access. */
#define instrument_write(a, size) do {  \
    uintptr_t __a = (uintptr_t)(a);     \
    uintptr_t __size = size;            \
    __asan_storeN(__a, __size);         \
} while (0)

/* Notify sanitizer runtime about an upcoming read/write access. */
#define instrument_read_write(a, size) do {               \
    uintptr_t __a = (uintptr_t)(a);                       \
    uintptr_t __size = size;                              \
    __asan_storeN(__a, __size);                           \
    __msan_check_mem_is_initialized((void *)__a, __size); \
} while (0)

#endif
