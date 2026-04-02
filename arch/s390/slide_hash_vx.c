/* slide_hash_vx.c - VX version of slide_hash for S390 processors
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#ifdef S390_VX

#include "zbuild.h"
#include "deflate.h"

#include "vx_intrins.h"

static inline void slide_hash_chain(Pos *table, uint32_t entries, uint16_t wsize) {
    const uv8hi vmx_wsize = vec_splats(wsize);
    Pos *p = table;

    do {
        uv8hi value, result;

        value = vec_xl(0, p);
        result = vec_sub(value, vec_min(value, vmx_wsize));
        vec_xst(result, 0, p);

        p += 8;
        entries -= 8;
    } while (entries > 0);
}

Z_INTERNAL void slide_hash_vx(deflate_state *s) {
    Assert(s->w_size <= UINT16_MAX, "w_size should fit in uint16_t");
    uint16_t wsize = (uint16_t)s->w_size;

    slide_hash_chain(s->head, HASH_SIZE, wsize);
    slide_hash_chain(s->prev, wsize, wsize);
}
#endif
