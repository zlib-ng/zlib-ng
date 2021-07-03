/* crc32_fold.h -- crc32 folding interface
 * Copyright (C) 2021 Nathan Moinvaziri
 * For conditions of distribution and use, see copyright notice in zlib.h
 */
#ifndef CRC32_FOLD_H_
#define CRC32_FOLD_H_

typedef struct crc32_fold_s {
    uint32_t ALIGNED_(16) fold[4 * 5];
    uint32_t value;
} crc32_fold;

Z_INTERNAL uint32_t crc32_fold_reset_c(crc32_fold *crc);
Z_INTERNAL void     crc32_fold_copy_c(crc32_fold *crc, uint8_t *dst, const uint8_t *src, size_t len);
Z_INTERNAL uint32_t crc32_fold_final_c(crc32_fold *crc);

#endif
