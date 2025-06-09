/* crc32_la.c - LoongArch version of crc32
 * Copyright (C) 2025 Vladislav Shchapov <vladislav@shchapov.ru>
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#if defined(LOONGARCH_CRC)
#include "zbuild.h"
#include "zmemory.h"
#include <stdint.h>

#include <larchintrin.h>

Z_INTERNAL uint32_t crc32_loongarch64(uint32_t crc, const uint8_t *buf,
                                      size_t len) {
    crc = (~crc) & 0xffffffff;

    while (len >= 8) {
        crc = (uint32_t)__crc_w_d_w((long int)zng_memread_8(buf), (int)crc);
        buf += 8;
        len -= 8;
    }
    if (len & 4) {
        crc = (uint32_t)__crc_w_w_w((int)zng_memread_4(buf), (int)crc);
        buf += 4;
    }
    if (len & 2) {
        crc = (uint32_t)__crc_w_h_w((short)zng_memread_2(buf), (int)crc);
        buf += 2;
    }
    if (len & 1) {
        crc = (uint32_t)__crc_w_b_w((char)(*buf), (int)crc);
    }

    return crc ^ 0xffffffff;
}

#endif
