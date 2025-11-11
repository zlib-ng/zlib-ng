/* crc32_zbc.c - RISCV Zbc version of crc32
 * Copyright (C) 2025 ByteDance. All rights reserved.
 * Contributed by Yin Tong <yintong.ustc@bytedance.com>
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#if defined(RISCV_CRC32_ZBC)
#include "zbuild.h"
#include "arch_functions.h"
#include <stdint.h>

#define CLMUL_MIN_LEN 16   // Minimum size of buffer for _crc32_clmul
#define CLMUL_CHUNK_LEN 16 // Length of chunk for clmul

#define CONSTANT_R3 0x1751997d0ULL
#define CONSTANT_R4 0x0ccaa009eULL
#define CONSTANT_R5 0x163cd6124ULL
#define MASK32 0xFFFFFFFF
#define CRCPOLY_TRUE_LE_FULL 0x1DB710641ULL
#define CONSTANT_RU 0x1F7011641ULL

static inline uint64_t clmul(uint64_t a, uint64_t b) {
  uint64_t res;
  __asm__ volatile("clmul %0, %1, %2" : "=r"(res) : "r"(a), "r"(b));
  return res;
}

static inline uint64_t clmulh(uint64_t a, uint64_t b) {
  uint64_t res;
  __asm__ volatile("clmulh %0, %1, %2" : "=r"(res) : "r"(a), "r"(b));
  return res;
}

static inline uint32_t crc32_clmul_impl(uint64_t crc, const unsigned char *buf,
                                        uint64_t len) {
  const uint64_t *buf64 = (const uint64_t *)buf;
  uint64_t low = buf64[0] ^ crc;
  uint64_t high = buf64[1];

  if (len < 16)
    goto finish_fold;
  len -= 16;
  buf64 += 2;

  // process each 16-byte block
  while (len >= 16) {
    uint64_t t2 = clmul(CONSTANT_R4, high);
    uint64_t t3 = clmulh(CONSTANT_R4, high);

    uint64_t t0_new = clmul(CONSTANT_R3, low);
    uint64_t t1_new = clmulh(CONSTANT_R3, low);

    // Combine the results and XOR with new data
    low = t0_new ^ t2;
    high = t1_new ^ t3;
    low ^= buf64[0];
    high ^= buf64[1];

    buf64 += 2;
    len -= 16;
  }

finish_fold:
  // Fold the 128-bit result into 64 bits
  uint64_t fold_t3 = clmulh(low, CONSTANT_R4);
  uint64_t fold_t2 = clmul(low, CONSTANT_R4);
  low = high ^ fold_t2;
  high = fold_t3;

  // Combine the low and high parts and perform polynomial reduction
  uint64_t combined = (low >> 32) | ((high & MASK32) << 32);
  uint64_t reduced_low = clmul(low & MASK32, CONSTANT_R5) ^ combined;

  // Barrett reduction step
  uint64_t barrett = clmul(reduced_low & MASK32, CONSTANT_RU) & MASK32;
  barrett = clmul(barrett, CRCPOLY_TRUE_LE_FULL);
  uint64_t final = barrett ^ reduced_low;

  // Return the high 32 bits as the final CRC
  return (uint32_t)(final >> 32);
}

Z_INTERNAL uint32_t crc32_riscv64_zbc(uint32_t crc, const uint8_t *buf,
                                      size_t len) {
  if (len < CLMUL_MIN_LEN) {
    return crc32_braid(crc, buf, len);
  }

  uint64_t unaligned_length = len % CLMUL_CHUNK_LEN;
  if (unaligned_length) {
    crc = crc32_braid(crc, buf, unaligned_length);
    buf += unaligned_length;
    len -= unaligned_length;
  }
  crc ^= 0xFFFFFFFF;
  crc = crc32_clmul_impl(crc, buf, len);
  return crc ^ 0xFFFFFFFF;
}

#endif
