/* crc32_hw_copy_impl_tpl.h -- compute the CRC-32 of a data stream for CPU with native crc instructions
 * Copyright (C) 1995-2006, 2010, 2011, 2012 Mark Adler
 * Copyright (C) 2016 Yang Zhang
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#include "zbuild.h"


Z_FORCEINLINE static Z_TARGET_CRC uint32_t crc32_hw_copy_impl(uint32_t crc, uint8_t *dst, const uint8_t *src, size_t len,
                                                              const int COPY) {
    uint32_t c = ~crc;

    if (UNLIKELY(len == 1)) {
        if (COPY)
            *dst = *src;
        c = CRC32B(c, *src);
        return ~c;
    }

    /* Align to 8-byte boundary for tail processing */
    uintptr_t align_diff = ALIGN_DIFF(src, 8);
    if (align_diff)
        c = crc32_hw_align(c, &dst, &src, &len, align_diff, COPY);

    while (len >= 64) {
        uint64_t d0 = *(const uint64_t *)src;
        uint64_t d1 = *(const uint64_t *)(src + 8);
        uint64_t d2 = *(const uint64_t *)(src + 16);
        uint64_t d3 = *(const uint64_t *)(src + 24);
        uint64_t d4 = *(const uint64_t *)(src + 32);
        uint64_t d5 = *(const uint64_t *)(src + 40);
        uint64_t d6 = *(const uint64_t *)(src + 48);
        uint64_t d7 = *(const uint64_t *)(src + 56);

        if (COPY) {
            memcpy(dst,      &d0, 8);
            memcpy(dst + 8,  &d1, 8);
            memcpy(dst + 16, &d2, 8);
            memcpy(dst + 24, &d3, 8);
            memcpy(dst + 32, &d4, 8);
            memcpy(dst + 40, &d5, 8);
            memcpy(dst + 48, &d6, 8);
            memcpy(dst + 56, &d7, 8);
            dst += 64;
        }

        c = CRC32D(c, d0);
        c = CRC32D(c, d1);
        c = CRC32D(c, d2);
        c = CRC32D(c, d3);
        c = CRC32D(c, d4);
        c = CRC32D(c, d5);
        c = CRC32D(c, d6);
        c = CRC32D(c, d7);

        src += 64;
        len -= 64;
    }

    return crc32_hw_tail(c, dst, src, len, COPY);
}
