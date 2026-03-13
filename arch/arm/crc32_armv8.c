/* crc32_armv8.c -- compute the CRC-32 of a data stream
 * Copyright (C) 1995-2006, 2010, 2011, 2012 Mark Adler
 * Copyright (C) 2016 Yang Zhang
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#ifdef ARM_CRC32

#include "zbuild.h"
#include "acle_intrins.h"
#include "crc32_armv8_p.h"

Z_FORCEINLINE static Z_TARGET_CRC uint32_t crc32_copy_impl(uint32_t crc, uint8_t *dst, const uint8_t *src, size_t len,
                                                           const int COPY) {
    uint32_t c = ~crc;

    if (UNLIKELY(len == 1)) {
        if (COPY)
            *dst = *src;
        c = __crc32b(c, *src);
        return ~c;
    }

    /* Align to 8-byte boundary for tail processing */
    uintptr_t align_diff = ALIGN_DIFF(src, 8);
    if (align_diff)
        c = crc32_armv8_align(c, &dst, &src, &len, align_diff, COPY);

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

        c = __crc32d(c, d0);
        c = __crc32d(c, d1);
        c = __crc32d(c, d2);
        c = __crc32d(c, d3);
        c = __crc32d(c, d4);
        c = __crc32d(c, d5);
        c = __crc32d(c, d6);
        c = __crc32d(c, d7);

        src += 64;
        len -= 64;
    }

    return crc32_armv8_tail(c, dst, src, len, COPY);
}

Z_INTERNAL Z_TARGET_CRC uint32_t crc32_armv8(uint32_t crc, const uint8_t *buf, size_t len) {
    return crc32_copy_impl(crc, NULL, buf, len, 0);
}

Z_INTERNAL Z_TARGET_CRC uint32_t crc32_copy_armv8(uint32_t crc, uint8_t *dst, const uint8_t *src, size_t len) {
#if OPTIMAL_CMP >= 32
    return crc32_copy_impl(crc, dst, src, len, 1);
#else
    /* Without unaligned access, interleaved stores get decomposed into byte ops */
    crc = crc32_armv8(crc, src, len);
    memcpy(dst, src, len);
    return crc;
#endif
}
#endif
