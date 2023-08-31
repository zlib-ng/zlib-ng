/* chunkset_rvv.c - General version of chunkset
 * Copyright (C) 2023 SiFive, Inc. All rights reserved.
 * Contributed by Alex Chiang <alex.chiang@sifive.com>
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#include "zbuild.h"

/*
 * It's not a optimized implemantation using RISC-V RVV, but a general optimized one.
 *
 * RISC-V glibc would enable RVV optimized memcpy at runtime by IFUNC,
 * so we prefer using large size chunk and copy memory as much as possible.
 */
#define CHUNK_SIZE 32

/* We don't have a 32-byte datatype for RISC-V arch. */
typedef struct chunk_s {
    uint8_t data[CHUNK_SIZE];
} chunk_t;

static inline void loadchunk(uint8_t const *s, chunk_t *chunk) {
    memcpy(chunk->data, s, CHUNK_SIZE);
}

static inline void storechunk(uint8_t *out, chunk_t *chunk) {
    memcpy(out, chunk->data, CHUNK_SIZE);
}

#define CHUNKSIZE        chunksize_rvv
#define CHUNKCOPY        chunkcopy_rvv
#define CHUNKUNROLL      chunkunroll_rvv
#define CHUNKMEMSET      chunkmemset_rvv
#define CHUNKMEMSET_SAFE chunkmemset_safe_rvv

#define HAVE_CHUNKCOPY

/*
 * Assuming that the length is non-zero, and that `from` lags `out` by at least
 * sizeof chunk_t bytes, please see the comments in chunkset_tpl.h.
 *
 * We load/store a single chunk once in the `CHUNKCOPY`.
 * However, RISC-V glibc would enable RVV optimized memcpy at runtime by IFUNC,
 * such that, we prefer copy large memory size once to make good use of the the RVV advance.
 * 
 * To be aligned to the other platforms, we did't modify `CHUNKCOPY` method a lot,
 * but we still copy as much memory as possible for some conditions.
 * 
 * case 1: out - from >= len (no overlap)
 *         We can use memcpy to copy `len` size once
 *         because the memory layout would be the same.
 *
 * case 2: overlap
 *         We copy N chunks using memcpy at once, aiming to achieve our goal: 
 *         to copy as much memory as possible.
 * 
 *         After using a single memcpy to copy N chunks, we have to use series of
 *         loadchunk and storechunk to ensure the result is correct.
 */
static inline uint8_t* CHUNKCOPY(uint8_t *out, uint8_t const *from, unsigned len) {
    Assert(len > 0, "chunkcopy should never have a length 0");
    int32_t align = ((len - 1) % sizeof(chunk_t)) + 1;
    chunk_t chunk;
    memcpy(out, from, sizeof(chunk));
    out += align;
    from += align;
    len -= align;
    ptrdiff_t dist = out - from;
    if (dist >= len) {
        memcpy(out, from, len);
        out += len;
        from += len;
        return out;
    }
    if (dist >= sizeof(chunk_t)) {
        dist = (dist / sizeof(chunk_t)) * sizeof(chunk_t);
        memcpy(out, from, dist);
        out += dist;
        from += dist;
        len -= dist;
    }
    while (len > 0) {
        memcpy(out, from, sizeof(chunk));
        out += sizeof(chunk_t);
        from += sizeof(chunk_t);
        len -= sizeof(chunk_t);
    }
    return out;
}

#include "chunkset_tpl.h"

#define INFLATE_FAST     inflate_fast_rvv

#include "inffast_tpl.h"
