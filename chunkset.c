/* chunkset.c -- inline functions to copy small data chunks.
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#include "zbuild.h"
#include "zutil.h"

/* Define 8 byte chunks differently depending on unaligned support */
#if defined(UNALIGNED64_OK)
typedef uint64_t chunk_t;
#elif defined(UNALIGNED_OK)
typedef struct chunk_t { uint32_t u32[2]; } chunk_t;
#else
typedef struct chunk_t { uint8_t u8[8]; } chunk_t;
#endif

#define CHUNK_SIZE 8

#define HAVE_CHUNKMEMSET_1
#define HAVE_CHUNKMEMSET_4
#define HAVE_CHUNKMEMSET_8

static inline void chunkmemset_1(uint8_t *from, chunk_t *chunk) {
#if defined(UNALIGNED64_OK)
    *chunk = 0x0101010101010101 * (uint8_t)*from;
#elif defined(UNALIGNED_OK)
    chunk->u32[0] = 0x01010101 * (uint8_t)*from;
    chunk->u32[1] = chunk->u32[0];
#else
    memset(chunk, *from, sizeof(chunk_t));
#endif
}

static inline void chunkmemset_4(uint8_t *from, chunk_t *chunk) {
#if defined(UNALIGNED64_OK)
    uint32_t half_chunk;
    memcpy(&half_chunk, from, sizeof(half_chunk));
    *chunk = 0x0000000100000001 * (uint64_t)half_chunk;
#elif defined(UNALIGNED_OK)
    memcpy(&chunk->u32[0], from, sizeof(chunk->u32[0]));
    chunk->u32[1] = chunk->u32[0];
#else
    uint8_t *chunkptr = (uint8_t *)chunk;
    memcpy(chunkptr, from, sizeof(uint32_t));
    memcpy(chunkptr+4, from, sizeof(uint32_t));
#endif
}

static inline void chunkmemset_8(uint8_t *from, chunk_t *chunk) {
#if defined(UNALIGNED_OK) && !defined(UNALIGNED64_OK)
    memcpy(&chunk->u32[0], from, sizeof(chunk->u32[0]));
    memcpy(&chunk->u32[1], from+4, sizeof(chunk->u32[1]));
#else
    memcpy(chunk, from, sizeof(uint64_t));
#endif
}

static inline void loadchunk(uint8_t const *s, chunk_t *chunk) {
    chunkmemset_8((uint8_t *)s, chunk);
}

static inline void storechunk(uint8_t *out, chunk_t *chunk) {
    memcpy(out, chunk, sizeof(uint64_t));
}

#define CHUNKSIZE        chunksize_c
#define CHUNKCOPY        chunkcopy_c
#define CHUNKCOPY_SAFE   chunkcopy_safe_c
#define CHUNKUNROLL      chunkunroll_c
#define CHUNKMEMSET      chunkmemset_c
#define CHUNKMEMSET_SAFE chunkmemset_safe_c

#include "chunkset_tpl.h"
