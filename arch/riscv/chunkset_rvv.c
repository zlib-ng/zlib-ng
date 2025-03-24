/* chunkset_rvv.c - RVV version of chunkset
 * Copyright (C) 2023 SiFive, Inc. All rights reserved.
 * Contributed by Alex Chiang <alex.chiang@sifive.com>
 * For conditions of distribution and use, see copyright notice in zlib.h
 */
#include <riscv_vector.h>
#include "zbuild.h"

/*
 * RISC-V glibc would enable RVV optimized memcpy at runtime by IFUNC,
 * so we prefer using large size chunk and copy memory as much as possible.
 */
#define CHUNK_SIZE 32

#define HAVE_CHUNKMEMSET_2
#define HAVE_CHUNKMEMSET_4
#define HAVE_CHUNKMEMSET_8

#define CHUNK_MEMSET_RVV_IMPL(elen)                                     \
do {                                                                    \
    size_t vl, len = CHUNK_SIZE / sizeof(uint##elen##_t);               \
    uint##elen##_t val = *(uint##elen##_t*)from;                        \
    uint##elen##_t* chunk_p = (uint##elen##_t*)chunk;                   \
    do {                                                                \
        vl = __riscv_vsetvl_e##elen##m4(len);                           \
        vuint##elen##m4_t v_val = __riscv_vmv_v_x_u##elen##m4(val, vl); \
        __riscv_vse##elen##_v_u##elen##m4(chunk_p, v_val, vl);          \
        len -= vl; chunk_p += vl;                                       \
    } while (len > 0);                                                  \
} while (0)

/* We don't have a 32-byte datatype for RISC-V arch. */
typedef struct chunk_s {
    uint64_t data[4];
} chunk_t;

static inline void chunkmemset_2(uint8_t *from, chunk_t *chunk) {
    CHUNK_MEMSET_RVV_IMPL(16);
}

static inline void chunkmemset_4(uint8_t *from, chunk_t *chunk) {
    CHUNK_MEMSET_RVV_IMPL(32);
}

static inline void chunkmemset_8(uint8_t *from, chunk_t *chunk) {
    CHUNK_MEMSET_RVV_IMPL(64);
}

static inline void loadchunk(uint8_t const *s, chunk_t *chunk) {
    memcpy(chunk->data, (uint8_t *)s, CHUNK_SIZE);
}

static inline void storechunk(uint8_t *out, chunk_t *chunk) {
    memcpy(out, chunk->data, CHUNK_SIZE);
}

#define CHUNKSIZE        chunksize_rvv
#define CHUNKCOPY        chunkcopy_rvv
#define CHUNKUNROLL      chunkunroll_rvv
#define CHUNKMEMSET      chunkmemset_rvv
#define CHUNKMEMSET_SAFE chunkmemset_safe_rvv

#include "chunkset_tpl.h"

#define INFLATE_FAST     inflate_fast_rvv

#include "inffast_tpl.h"
