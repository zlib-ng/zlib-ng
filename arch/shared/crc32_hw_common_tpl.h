/* crc32_hw_common_tpl.h -- Private shared inline CRC32 functions for CPU with native crc instructions
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#include "zbuild.h"
#include "zmemory.h"

/* CRC32D(CRC32_HW_FINAL, val) == ~CRC32D(0, val), so this seed applies the final complement. */
#define CRC32_HW_FINAL 0xefe4d0afu

Z_FORCEINLINE static Z_TARGET_CRC uint32_t crc32_hw_align(uint32_t crc, uint8_t **dst, const uint8_t **buf,
                                                          size_t *len, uintptr_t align_diff, const int COPY) {
    if (*len && (align_diff & 1)) {
        uint8_t val = **buf;
        if (COPY) {
            **dst = val;
            *dst += 1;
        }
        crc = CRC32B(crc, val);
        *buf += 1;
        *len -= 1;
    }

    if (*len >= 2 && (align_diff & 2)) {
        uint16_t val = *((uint16_t*)*buf);
        if (COPY) {
            memcpy(*dst, &val, 2);
            *dst += 2;
        }
        crc = CRC32H(crc, val);
        *buf += 2;
        *len -= 2;
    }

    if (*len >= 4 && (align_diff & 4)) {
        uint32_t val = *((uint32_t*)*buf);
        if (COPY) {
            memcpy(*dst, &val, 4);
            *dst += 4;
        }
        crc = CRC32W(crc, val);
        *buf += 4;
        *len -= 4;
    }

    if (*len >= 8 && (align_diff & 8)) {
        uint64_t val = *((uint64_t*)*buf);
        if (COPY) {
            memcpy(*dst, &val, 8);
            *dst += 8;
        }
        crc = CRC32D(crc, val);
        *buf += 8;
        *len -= 8;
    }

    return crc;
}

/* Finish with the 1 to 7 bytes after the last full word, given the last 8 bytes of the buffer.
 * The bytes still to do are the top n bytes of that word. Their CRC and the crc carried past them
 * are computed separately, so only a shift, one CRC32D and one xor follow the running crc.
 * Returns the final, complemented crc. */
Z_FORCEINLINE static Z_TARGET_CRC uint32_t crc32_hw_tail_bytes(uint32_t crc, uint64_t last, uint32_t n) {
    uint32_t shift = 64 - n * 8;
    /* The n bytes on their own, with the final complement. */
    uint32_t bytes = CRC32D(CRC32_HW_FINAL, (last >> shift) << shift);
    /* The part of the crc that n bytes do not reach only shifts down. */
    uint32_t rest = bytes ^ (uint32_t)((uint64_t)crc >> (n * 8));
    /* The part they do reach goes through n bytes of zeros. */
    return CRC32D(0, (uint64_t)crc << shift) ^ rest;
}

Z_FORCEINLINE static Z_TARGET_CRC uint32_t crc32_hw_tail(uint32_t crc, uint8_t *dst, const uint8_t *buf,
                                                         size_t len, const int COPY) {
    const uint8_t *start = buf;

    while (len >= 8) {
        uint64_t val = *((uint64_t*)buf);
        if (COPY) {
            memcpy(dst, &val, 8);
            dst += 8;
        }
        crc = CRC32D(crc, val);
        buf += 8;
        len -= 8;
    }

    /* After a full word, the bytes left are the top of the buffer's last 8 bytes. */
    if (len && buf != start) {
        uint64_t last = zng_memread_8(buf + len - 8);
        if (COPY)
            memcpy(dst + len - 8, &last, 8);
        return crc32_hw_tail_bytes(crc, last, (uint32_t)len);
    }

    if (len & 4) {
        uint32_t val = *((uint32_t*)buf);
        if (COPY) {
            memcpy(dst, &val, 4);
            dst += 4;
        }
        crc = CRC32W(crc, val);
        buf += 4;
    }

    if (len & 2) {
        uint16_t val = *((uint16_t*)buf);
        if (COPY) {
            memcpy(dst, &val, 2);
            dst += 2;
        }
        crc = CRC32H(crc, val);
        buf += 2;
    }

    if (len & 1) {
        uint8_t val = *buf;
        if (COPY)
            *dst = val;
        crc = CRC32B(crc, val);
    }

    return ~crc;
}
