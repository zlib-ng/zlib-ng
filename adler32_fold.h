/* adler32_fold.h -- adler32 folding interface
 * Copyright (C) 2022 Adam Stylinski
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#ifndef ADLER32_FOLD_H_
#define ADLER32_FOLD_H_

#include <stdint.h>

typedef struct adler32_fold_s {
    uint8_t adler[64]; // First half of component sums
    uint8_t sum2[64]; // Second half of component sums
    uint32_t nsums; // The number of scalar sums performed
} adler32_fold;

Z_INTERNAL void adler32_fold_reset_c(adler32_fold *adler, uint32_t init_adler);
Z_INTERNAL void adler32_fold_copy_c(adler32_fold *adler, uint8_t *dst, const uint8_t *src, size_t len);
Z_INTERNAL void adler32_fold_c(adler32_fold *adler, const uint8_t *src, size_t len);
Z_INTERNAL uint32_t adler32_fold_final_c(adler32_fold *adler);

#endif
