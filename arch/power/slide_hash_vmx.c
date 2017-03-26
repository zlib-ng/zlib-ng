/* Optimized slide_hash for PowerPC processors with VMX instructions
 * Copyright (C) 2017-2021 Mika T. Lindqvist <postmaster@raasu.org>
 * For conditions of distribution and use, see copyright notice in zlib.h
 */
#ifdef PPC_VMX_SLIDEHASH

#include <altivec.h>
#include "zbuild.h"
#include "deflate.h"

static inline void slide_hash_chain(Pos *table, uint32_t entries, uint16_t wsize) {
    const vector unsigned short vmx_wsize = vec_splats(wsize);
    Pos *p = table;

    do {
        vector unsigned short value, result;

        value = vec_ld(0, p);
        result = vec_subs(value, vmx_wsize);
        vec_st(result, 0, p);

        p += 8;
        entries -= 8;
   } while (entries > 0);
}

void Z_INTERNAL slide_hash_vmx(deflate_state *s) {
    uint16_t wsize = s->w_size;

    slide_hash_chain(s->head, HASH_SIZE, wsize);
    slide_hash_chain(s->prev, wsize, wsize);
}

#endif /* PPC_VMX_SLIDEHASH */
