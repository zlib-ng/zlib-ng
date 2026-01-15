#ifndef CRC32_BRAID_P_H_
#define CRC32_BRAID_P_H_

#include "zendian.h"

/* Define BRAID_N, valid range is 1..6 */
#define BRAID_N 5

/* Define BRAID_W and the associated z_word_t type. If BRAID_W is not defined, then a braided
   calculation is not used, and the associated tables and code are not compiled.
 */
#ifdef ARCH_64BIT
#  define BRAID_W 8
    typedef uint64_t z_word_t;
#  define Z_WORD_FROM_LE(word) Z_U64_FROM_LE(word)
#else
#  define BRAID_W 4
    typedef uint32_t z_word_t;
#  define Z_WORD_FROM_LE(word) Z_U32_FROM_LE(word)
#endif

#if BYTE_ORDER == LITTLE_ENDIAN
#  define BRAID_TABLE crc_braid_table
#elif BYTE_ORDER == BIG_ENDIAN
#  define BRAID_TABLE crc_braid_big_table
#else
#  error "No endian defined"
#endif

/* CRC polynomial. */
#define POLY 0xedb88320         /* p(x) reflected, with x^32 implied */

#endif /* CRC32_BRAID_P_H_ */
