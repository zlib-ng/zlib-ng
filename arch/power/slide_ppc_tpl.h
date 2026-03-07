/* Optimized slide_hash for PowerPC processors
 * Copyright (C) 2017-2021 Mika T. Lindqvist <postmaster@raasu.org>
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#include <altivec.h>
#include "zbuild.h"
#include "deflate.h"

static inline void slide_hash_chain(Pos *table, uint32_t entries, uint16_t wsize) {
    const vector unsigned short vmx_wsize = vec_splats(wsize);
    Pos *p = table;

    do {
        /* Do the pointer arithmetic early to hopefully overlap the vector unit */
        Pos *q = p;
        p += 32;
        vector unsigned short value0, value1, value2, value3;
        vector unsigned short result0, result1, result2, result3;

        value0 = vec_ld(0, q);
        value1 = vec_ld(16, q);
        value2 = vec_ld(32, q);
        value3 = vec_ld(48, q);
        result0 = vec_subs(value0, vmx_wsize);
        result1 = vec_subs(value1, vmx_wsize);
        result2 = vec_subs(value2, vmx_wsize);
        result3 = vec_subs(value3, vmx_wsize);
        vec_st(result0, 0, q);
        vec_st(result1, 16, q);
        vec_st(result2, 32, q);
        vec_st(result3, 48, q);

        entries -= 32;
   } while (entries);
}

void Z_INTERNAL SLIDE_PPC(deflate_state *s) {
    Assert(s->w_size <= UINT16_MAX, "w_size should fit in uint16_t");
    uint16_t wsize = (uint16_t)s->w_size;

    slide_hash_chain(s->head, HASH_SIZE, wsize);
    slide_hash_chain(s->prev, wsize, wsize);
}
