/* crc32_la.c - LoongArch version of crc32
 * Copyright (C) 2025 Vladislav Shchapov <vladislav@shchapov.ru>
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#if defined(LOONGARCH_CRC)
#include "zbuild.h"
#include "crc32.h"
#include <stdint.h>
#include <larchintrin.h>

Z_INTERNAL uint32_t crc32_loongarch64(uint32_t crc, const uint8_t *buf, size_t len) {
    Z_REGISTER uint32_t c;
    Z_REGISTER uint16_t buf2;
    Z_REGISTER uint32_t buf4;
    Z_REGISTER uint64_t buf8;

    c = ~crc;

    if (UNLIKELY(len == 1)) {
        c = (uint32_t)__crc_w_b_w((char)(*buf), (int)c);
        c = ~c;
        return c;
    }

    if ((ptrdiff_t)buf & (sizeof(uint64_t) - 1)) {
        if (len && ((ptrdiff_t)buf & 1)) {
            c = (uint32_t)__crc_w_b_w((char)(*buf++), (int)c);
            len--;
        }

        if ((len >= sizeof(uint16_t)) && ((ptrdiff_t)buf & (sizeof(uint32_t) - 1))) {
            buf2 = *((uint16_t*)buf);
            c = (uint32_t)__crc_w_h_w((short)buf2, (int)c);
            buf += sizeof(uint16_t);
            len -= sizeof(uint16_t);
        }

        if ((len >= sizeof(uint32_t)) && ((ptrdiff_t)buf & (sizeof(uint64_t) - 1))) {
            buf4 = *((uint32_t*)buf);
            c = (uint32_t)__crc_w_w_w((int)buf4, (int)c);
            len -= sizeof(uint32_t);
            buf += sizeof(uint32_t);
        }

    }

    while (len >= sizeof(uint64_t)) {
        buf8 = *((uint64_t*)buf);
        c = (uint32_t)__crc_w_d_w((long int)buf8, (int)c);
        len -= sizeof(uint64_t);
        buf += sizeof(uint64_t);
    }

    if (len & sizeof(uint32_t)) {
        buf4 = *((uint32_t*)buf);
        c = (uint32_t)__crc_w_w_w((int)buf4, (int)c);
        buf += sizeof(uint32_t);
    }

    if (len & sizeof(uint16_t)) {
        buf2 = *((uint16_t*)buf);
        c = (uint32_t)__crc_w_h_w((short)buf2, (int)c);
        buf += sizeof(uint16_t);
    }

    if (len & sizeof(uint8_t)) {
        c = (uint32_t)__crc_w_b_w((char)(*buf), (int)c);
    }

    c = ~c;
    return c;
}

/* Note: Based on generic crc32_fold_* implementation with functable call replaced by direct call. */
Z_INTERNAL void crc32_fold_copy_loongarch64(crc32_fold *crc, uint8_t *dst, const uint8_t *src, size_t len) {
    crc->value = crc32_loongarch64(crc->value, src, len);
    memcpy(dst, src, len);
}

Z_INTERNAL void crc32_fold_loongarch64(crc32_fold *crc, const uint8_t *src, size_t len, uint32_t init_crc) {
    Z_UNUSED(init_crc);
    crc->value = crc32_loongarch64(crc->value, src, len);
}

#endif
