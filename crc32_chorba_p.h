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
#  define CHORBA_CARRY_PARAMS chorba_word_t next1, chorba_word_t next2, chorba_word_t next3, chorba_word_t next4, chorba_word_t next5
#  define CHORBA_NO_CARRY 0, 0, 0, 0, 0
#  define CHORBA_CARRY_FROM(a) (a)[0], (a)[1], (a)[2], (a)[3], (a)[4]
#  define CHORBA_WORD_FROM_LE(word) Z_U64_FROM_LE(word)
#  define CHORBA_WORD_TO_LE(word) Z_U64_TO_LE(word)
    typedef uint64_t chorba_word_t;
#else
#  define CHORBA_SMALL_THRESHOLD 43
#  define CHORBA_W 4
#  define CHORBA_CARRY_WORDS 10
#  define CHORBA_CARRY_PARAMS chorba_word_t next1, chorba_word_t next2, chorba_word_t next3, chorba_word_t next4, chorba_word_t next5, \
                              chorba_word_t next6, chorba_word_t next7, chorba_word_t next8, chorba_word_t next9, chorba_word_t next10
#  define CHORBA_NO_CARRY 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
#  define CHORBA_CARRY_FROM(a) (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5], (a)[6], (a)[7], (a)[8], (a)[9]
#  define CHORBA_WORD_FROM_LE(word) Z_U32_FROM_LE(word)
#  define CHORBA_WORD_TO_LE(word) Z_U32_TO_LE(word)
    typedef uint32_t chorba_word_t;
#endif

/* The register kernel over the end of a stream, in the build's word, shared by every variant.
 * The carry words arrive as parameters, so the first is in a register for the first fold. */
Z_INTERNAL uint32_t chorba_small_tail(const uint8_t *buf, size_t i, size_t len, const chorba_word_t *pending, size_t mask,
                                      CHORBA_CARRY_PARAMS);

Z_INTERNAL uint32_t crc32_chorba_118960_nondestructive(uint32_t crc, const uint8_t *buf, size_t len);
Z_INTERNAL uint32_t crc32_chorba_32768_nondestructive(uint32_t crc, const uint8_t *buf, size_t len);
Z_INTERNAL uint32_t crc32_chorba_small_nondestructive(uint32_t crc, const uint8_t *buf, size_t len);
Z_INTERNAL uint32_t crc32_chorba_small_nondestructive_32bit(uint32_t crc, const uint8_t *buf, size_t len);

#endif /* CRC32_CHORBA_P_H_ */
