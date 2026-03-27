/* crc32_la.c - LoongArch version of crc32
 * Copyright (C) 2025 Vladislav Shchapov <vladislav@shchapov.ru>
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#ifdef LOONGARCH_CRC

#include "zbuild.h"

#include <larchintrin.h>

#define Z_TARGET_CRC
#define CRC32B(crc, val) (uint32_t)__crc_w_b_w((char)(val), (int)(crc))
#define CRC32H(crc, val) (uint32_t)__crc_w_h_w((short)(val), (int)(crc))
#define CRC32W(crc, val) (uint32_t)__crc_w_w_w((int)(val), (int)(crc))
#define CRC32D(crc, val) (uint32_t)__crc_w_d_w((long int)(val), (int)(crc))

#include "arch/shared/crc32_hw_common_tpl.h"

#include "arch/shared/crc32_hw_copy_impl_tpl.h"


Z_INTERNAL uint32_t crc32_loongarch64(uint32_t crc, const uint8_t *buf, size_t len) {
    return crc32_hw_copy_impl(crc, NULL, buf, len, 0);
}

Z_INTERNAL uint32_t crc32_copy_loongarch64(uint32_t crc, uint8_t *dst, const uint8_t *src, size_t len) {
    return crc32_hw_copy_impl(crc, dst, src, len, 1);
}

#endif
