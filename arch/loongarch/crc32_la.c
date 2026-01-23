/* crc32_la.c - LoongArch version of crc32
 * Copyright (C) 2025 Vladislav Shchapov <vladislav@shchapov.ru>
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#ifdef LOONGARCH_CRC

#include "zbuild.h"
#include "crc32.h"

#include <larchintrin.h>

Z_INTERNAL uint32_t crc32_loongarch64(uint32_t crc, const uint8_t *buf, size_t len) {
    uint32_t c = ~crc;

    if (UNLIKELY(len == 1)) {
        c = (uint32_t)__crc_w_b_w((char)(*buf), (int)c);
        c = ~c;
        return c;
    }

    uintptr_t align_diff = ALIGN_DIFF(buf, 8);
    if (align_diff) {
        if (len && (align_diff & 1)) {
            c = (uint32_t)__crc_w_b_w((char)(*buf++), (int)c);
            len--;
        }

        if (len >= 2 && (align_diff & 2)) {
            c = (uint32_t)__crc_w_h_w((short)*((uint16_t*)buf), (int)c);
            buf += 2;
            len -= 2;
        }

        if (len >= 4 && (align_diff & 4)) {
            c = (uint32_t)__crc_w_w_w((int)*((uint32_t*)buf), (int)c);
            len -= 4;
            buf += 4;
        }

    }

    while (len >= 8) {
        c = (uint32_t)__crc_w_d_w((long int)*((uint64_t*)buf), (int)c);
        len -= 8;
        buf += 8;
    }

    if (len & 4) {
        c = (uint32_t)__crc_w_w_w((int)*((uint32_t*)buf), (int)c);
        buf += 4;
    }

    if (len & 2) {
        c = (uint32_t)__crc_w_h_w((short)*((uint16_t*)buf), (int)c);
        buf += 2;
    }

    if (len & 1) {
        c = (uint32_t)__crc_w_b_w((char)(*buf), (int)c);
    }

    c = ~c;
    return c;
}

Z_INTERNAL uint32_t crc32_copy_loongarch64(uint32_t crc, uint8_t *dst, const uint8_t *src, size_t len) {
    crc = crc32_loongarch64(crc, src, len);
    memcpy(dst, src, len);
    return crc;
}
#endif
