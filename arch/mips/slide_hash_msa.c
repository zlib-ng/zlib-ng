/* Optimized slide_hash for MIPS processors with MSA instructions
 * Copyright (C) 2026 Mika T. Lindqvist <postmaster@raasu.org>
 * For conditions of distribution and use, see copyright notice in zlib.h
 */
#ifdef MIPS_MSA

#include "zbuild.h"
#include "deflate.h"

#include <msa.h>

static inline void slide_hash_chain(Pos *table, uint32_t entries, v8u16 msa_wsize) {
    Pos *p = table;

    do {
        /* Do the pointer arithmetic early to hopefully overlap the vector unit */
        Pos *q = p;
        p += 32;
        v8u16 value0, value1, value2, value3;
        v8u16 result0, result1, result2, result3;

        value0 = (v8u16) __msa_ld_h(q, 0);
        value1 = (v8u16) __msa_ld_h(q, 16);
        value2 = (v8u16) __msa_ld_h(q, 32);
        value3 = (v8u16) __msa_ld_h(q, 48);
        result0 = __msa_subs_u_h(value0, msa_wsize);
        result1 = __msa_subs_u_h(value1, msa_wsize);
        result2 = __msa_subs_u_h(value2, msa_wsize);
        result3 = __msa_subs_u_h(value3, msa_wsize);
        __msa_st_h((v8i16) result0, q, 0);
        __msa_st_h((v8i16) result1, q, 16);
        __msa_st_h((v8i16) result2, q, 32);
        __msa_st_h((v8i16) result3, q, 48);

        entries -= 32;
   } while (entries);
}

void Z_INTERNAL slide_hash_msa(deflate_state *s) {
    Assert(s->w_size <= UINT16_MAX, "w_size should fit in uint16_t");
    uint16_t wsize = (uint16_t) s->w_size;
    v8u16 msa_wsize = (v8u16) __msa_fill_h(wsize);

    slide_hash_chain(s->head, HASH_SIZE, msa_wsize);
    slide_hash_chain(s->prev, wsize, msa_wsize);
}
#endif /* MIPS_MSA */
