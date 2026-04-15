#ifndef TEST_SHARED_NG_H
#define TEST_SHARED_NG_H

#include "test_shared.h"

/* Test definitions that can only be used in the zlib-ng build environment. */

static inline int deflate_prime_32(PREFIX3(stream) *stream, uint32_t value) {
    int err;

#ifndef TEST_STOCK_ZLIB
    err = PREFIX(deflatePrime)(stream, 32, (int32_t)value);
#else
    /* zlib's deflatePrime() takes at most 16 bits */
    err = PREFIX(deflatePrime)(stream, 16, (int32_t)(value & 0xffff));
    if (err != Z_OK) return err;
    err = PREFIX(deflatePrime)(stream, 16, (int32_t)(value >> 16));
#endif

    return err;
}

#endif
