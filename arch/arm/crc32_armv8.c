/* crc32_armv8.c -- compute the CRC-32 of a data stream
 * Copyright (C) 1995-2006, 2010, 2011, 2012 Mark Adler
 * Copyright (C) 2016 Yang Zhang
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#if defined(ARM_CRC32)
#include "acle_intrins.h"
#include "zbuild.h"
#include "zmemory.h"
#include "crc32.h"

static Z_FORCEINLINE Z_TARGET_CRC uint32_t crc32_armv8_copy_impl(uint32_t crc, uint8_t *dst, const uint8_t *src, size_t len, const int COPY) {
    Z_REGISTER uint32_t c;
    Z_REGISTER uint16_t val2;
    Z_REGISTER uint32_t val4;
    Z_REGISTER uint64_t val8;

    c = ~crc;

    if (UNLIKELY(len == 1)) {
        if (COPY)
            *dst = *src;
        c = __crc32b(c, *src);
        c = ~c;
        return c;
    }

    if ((ptrdiff_t)src & (sizeof(uint64_t) - 1)) {
        if (len && ((ptrdiff_t)src & 1)) {
            if (COPY)
                *dst++ = *src;
            c = __crc32b(c, *src++);
            len--;
        }

        if ((len >= sizeof(uint16_t)) && ((ptrdiff_t)src & (sizeof(uint32_t) - 1))) {
            val2 = *((uint16_t*)src);
            if (COPY) {
                zng_memwrite_2(dst, val2);
                dst += sizeof(uint16_t);
            }
            c = __crc32h(c, val2);
            src += sizeof(uint16_t);
            len -= sizeof(uint16_t);
        }

        if ((len >= sizeof(uint32_t)) && ((ptrdiff_t)src & (sizeof(uint64_t) - 1))) {
            val4 = *((uint32_t*)src);
            if (COPY) {
                zng_memwrite_4(dst, val4);
                dst += sizeof(uint32_t);
            }
            c = __crc32w(c, val4);
            src += sizeof(uint32_t);
            len -= sizeof(uint32_t);
        }
    }

    while (len >= sizeof(uint64_t)) {
        val8 = *((uint64_t*)src);
        if (COPY) {
            zng_memwrite_8(dst, val8);
            dst += sizeof(uint64_t);
        }
        c = __crc32d(c, val8);
        src += sizeof(uint64_t);
        len -= sizeof(uint64_t);
    }

    if (len & sizeof(uint32_t)) {
        val4 = *((uint32_t*)src);
        if (COPY) {
            zng_memwrite_4(dst, val4);
            dst += sizeof(uint32_t);
        }
        c = __crc32w(c, val4);
        src += sizeof(uint32_t);
    }

    if (len & sizeof(uint16_t)) {
        val2 = *((uint16_t*)src);
        if (COPY) {
            zng_memwrite_2(dst, val2);
            dst += sizeof(uint16_t);
        }
        c = __crc32h(c, val2);
        src += sizeof(uint16_t);
    }

    if (len & sizeof(uint8_t)) {
        if (COPY)
            *dst = *src;
        c = __crc32b(c, *src);
    }

    c = ~c;
    return c;
}

Z_INTERNAL Z_TARGET_CRC uint32_t crc32_armv8(uint32_t crc, const uint8_t *src, size_t len) {
    return crc32_armv8_copy_impl(crc, NULL, src, len, 0);
}

/* Note: Based on generic crc32_fold_* implementation with functable call replaced by direct call. */
Z_INTERNAL Z_TARGET_CRC void crc32_fold_copy_armv8(crc32_fold *crc, uint8_t *dst, const uint8_t *src, size_t len) {
    crc->value = crc32_armv8_copy_impl(crc->value, dst, src, len, 1);
}

Z_INTERNAL Z_TARGET_CRC void crc32_fold_armv8(crc32_fold *crc, const uint8_t *src, size_t len, uint32_t init_crc) {
    Z_UNUSED(init_crc);
    crc->value = crc32_armv8_copy_impl(crc->value, NULL, src, len, 0);
}

#endif
