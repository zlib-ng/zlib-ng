/* adler32_fold.c -- adler32 folding interface
 * Copyright (C) 2022 Adam Stylinski
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#include "zbuild.h"
#include "functable.h"
#include "adler32_fold.h"

#include <limits.h>

Z_INTERNAL uint32_t adler32_fold_copy_c(uint32_t adler, uint8_t *dst, const uint8_t *src, uint64_t len) {
    adler = functable.adler32(adler, src, len);
/* Test that we don't try to copy more than actually fits in available address space */
#if INTPTR_MAX > SSIZE_MAX
    while (len > SSIZE_MAX) {
        memcpy(dst, src, SSIZE_MAX);
        dst += SSIZE_MAX;
        src += SSIZE_MAX;
        len -= SSIZE_MAX;
    }
#endif
    if (len) {
        memcpy(dst, src, (size_t)len);
    }
    return adler;
}
