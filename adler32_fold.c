/* crc32_fold.c -- adler32 folding interface
 * Copyright (C) 2022 Adam Stylinski 
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#include "zbuild.h"
#include "functable.h"
#include "adler32_fold.h"

Z_INTERNAL void adler32_fold_reset_c(adler32_fold *adler, uint32_t init_adler) {
    /* So, for the "C" version, we'll just stash the value into nsums.
     * This is mostly a compatibility shim, these functions in the functable
     * will have more optimal versions that make use of adler and sum2. In order
     * to make each implementation bisectable, each new implementation will be a
     * new commit */
    adler->nsums = init_adler;
}

Z_INTERNAL void adler32_fold_copy_c(adler32_fold *adler, uint8_t *dst, const uint8_t *src, size_t len) {
    adler->nsums = functable.adler32(adler->nsums, src, len);
    memcpy(dst, src, len);
}

Z_INTERNAL void adler32_fold_c(adler32_fold *adler, const uint8_t *src, size_t len) {
    adler->nsums = functable.adler32(adler->nsums, src, len);
}

Z_INTERNAL uint32_t adler32_fold_final_c(adler32_fold *adler) {
    return adler->nsums;
}
