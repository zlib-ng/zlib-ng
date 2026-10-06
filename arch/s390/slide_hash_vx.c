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
    Assert(s->slide_len <= UINT16_MAX, "slide_len should fit in uint16_t");
    uint16_t slide = (uint16_t)s->slide_len;

    slide_hash_chain(s->head, HASH_SIZE, slide);
    slide_hash_chain(s->prev, s->w_size, slide);
}

Z_INTERNAL void slide_hash_head_vx(deflate_state *s) {
    Assert(s->slide_len <= UINT16_MAX, "slide_len should fit in uint16_t");
    uint16_t slide = (uint16_t)s->slide_len;

    slide_hash_chain(s->head, HASH_SIZE, slide);
}
#endif
