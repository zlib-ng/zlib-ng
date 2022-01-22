/* crc32_fold.c -- crc32 folding interface
 * Copyright (C) 2021 Nathan Moinvaziri
 * For conditions of distribution and use, see copyright notice in zlib.h
 */
#include "zbuild.h"
#include "functable.h"

#include "crc32_fold.h"

Z_INTERNAL uint32_t crc32_fold_reset_c(crc32_fold *crc) {
    crc->value = CRC32_INITIAL_VALUE;
    return crc->value;
}

Z_INTERNAL void crc32_fold_copy_c(crc32_fold *crc, uint8_t *dst, const uint8_t *src, size_t len) {
    crc->value = functable.crc32(crc->value, src, len);
    memcpy(dst, src, len);
}

Z_INTERNAL uint32_t crc32_fold_final_c(crc32_fold *crc) {
    return crc->value;
}
