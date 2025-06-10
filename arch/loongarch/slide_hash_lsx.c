/*
 * LSX optimized hash slide, based on Intel SSE implementation
 *
 * Copyright (C) 2017 Intel Corporation
 * Copyright (C) 2025 Vladislav Shchapov <vladislav@shchapov.ru>
 * Authors:
 *   Arjan van de Ven   <arjan@linux.intel.com>
 *   Jim Kukunas        <james.t.kukunas@linux.intel.com>
 *
 * For conditions of distribution and use, see copyright notice in zlib.h
 */
#include "zbuild.h"
#include "deflate.h"

#include <lsxintrin.h>
#include <assert.h>

static inline void slide_hash_chain(Pos *table0, Pos *table1, uint32_t entries0,
                                    uint32_t entries1, const __m128i wsize) {
    uint32_t entries;
    Pos *table;
    __m128i value0, value1, result0, result1;

    int on_chain = 0;

next_chain:
    table = (on_chain) ? table1 : table0;
    entries = (on_chain) ? entries1 : entries0;

    table += entries;
    table -= 16;

    /* ZALLOC allocates this pointer unless the user chose a custom allocator.
     * Our alloc function is aligned to 64 byte boundaries */
    do {
        value0 = __lsx_vld(table, 0);
        value1 = __lsx_vld(table, 16);
        result0 = __lsx_vssub_hu(value0, wsize);
        result1 = __lsx_vssub_hu(value1, wsize);
        __lsx_vst(result0, table, 0);
        __lsx_vst(result1, table, 16);

        table -= 16;
        entries -= 16;
    } while (entries > 0);

    ++on_chain;
    if (on_chain > 1) {
        return;
    } else {
        goto next_chain;
    }
}

Z_INTERNAL void slide_hash_lsx(deflate_state *s) {
    Assert(s->w_size <= UINT16_MAX, "w_size should fit in uint16_t");
    uint16_t wsize = (uint16_t)s->w_size;
    const __m128i xmm_wsize = __lsx_vreplgr2vr_h((short)wsize);

    assert(((uintptr_t)s->head & 15) == 0);
    assert(((uintptr_t)s->prev & 15) == 0);

    slide_hash_chain(s->head, s->prev, HASH_SIZE, wsize, xmm_wsize);
}
