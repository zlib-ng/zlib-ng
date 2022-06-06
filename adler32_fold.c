/* adler32_fold.c -- adler32 folding interface
 * Copyright (C) 2022 Adam Stylinski
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#include "zbuild.h"
#include "functable.h"
#include "adler32_fold.h"

Z_INTERNAL uint32_t adler32_fold_copy_c(uint32_t adler, uint8_t *dst, const uint8_t *src, uint64_t len) {
    adler = functable.adler32(adler, src, len);
    while (len > SIZE_MAX) {
        memcpy(dst, src, SIZE_MAX);
        dst += SIZE_MAX;
        src += SIZE_MAX;
        len -= SIZE_MAX;
    }
    if (len) {
        memcpy(dst, src, (size_t)len);
    }
    return adler;
}
