/* crc32_chorba_p.h -- crc32 chorba interface
 * Copyright (C) 2021 Nathan Moinvaziri
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#ifndef CRC32_CHORBA_P_H_
#define CRC32_CHORBA_P_H_

#include "zendian.h"

/* Size thresholds for Chorba algorithm variants */

#define CHORBA_LARGE_THRESHOLD (sizeof(chorba_word_t) * 64 * 1024)
#define CHORBA_MEDIUM_UPPER_THRESHOLD 32768
#define CHORBA_MEDIUM_LOWER_THRESHOLD 8192
#define CHORBA_SMALL_THRESHOLD_64BIT 47
#ifdef ARCH_64BIT
#  define CHORBA_SMALL_THRESHOLD 47
#  define CHORBA_W 8
#  define CHORBA_CARRY_WORDS 5
#  define CHORBA_WORD_FROM_LE(word) Z_U64_FROM_LE(word)
    typedef uint64_t chorba_word_t;
#else
#  define CHORBA_SMALL_THRESHOLD 43
#  define CHORBA_W 4
#  define CHORBA_CARRY_WORDS 10
#  define CHORBA_WORD_FROM_LE(word) Z_U32_FROM_LE(word)
    typedef uint32_t chorba_word_t;
#endif

/* The register kernel over the end of a stream, in the build's word, shared by every variant */
Z_INTERNAL uint32_t chorba_small_tail(const uint8_t *buf, size_t i, size_t len, const chorba_word_t *pending, size_t mask,
                                      const chorba_word_t carry[CHORBA_CARRY_WORDS]);

Z_INTERNAL uint32_t crc32_chorba_118960_nondestructive(uint32_t crc, const uint8_t *buf, size_t len);
Z_INTERNAL uint32_t crc32_chorba_32768_nondestructive(uint32_t crc, const uint8_t *buf, size_t len);
Z_INTERNAL uint32_t crc32_chorba_small_nondestructive(uint32_t crc, const uint8_t *buf, size_t len);
Z_INTERNAL uint32_t crc32_chorba_small_nondestructive_32bit(uint32_t crc, const uint8_t *buf, size_t len);

#endif /* CRC32_CHORBA_P_H_ */
