/* dfltcc_deflate.c - IBM Z DEFLATE CONVERSION CALL general support. */

#include "zbuild.h"
#include "dfltcc_common.h"
#include "dfltcc_detail.h"

/*
   Memory management.

   DFLTCC requires parameter blocks and window to be aligned. zlib-ng allows
   users to specify their own allocation functions, so using e.g.
   `posix_memalign' is not an option. Thus, we overallocate and take the
   aligned portion of the buffer.
*/
static inline int is_dfltcc_enabled(void)
{
    uint64_t facilities[(DFLTCC_FACILITY / 64) + 1];
    register uint8_t r0 __asm__("r0");

    memset(facilities, 0, sizeof(facilities));
    r0 = sizeof(facilities) / sizeof(facilities[0]) - 1;
    __asm__ volatile("stfle %[facilities]\n" : [facilities] "=Q" (facilities), [r0] "+r" (r0) :: "cc");
    return is_bit_set((const char *)facilities, DFLTCC_FACILITY);
}

void ZLIB_INTERNAL dfltcc_reset(PREFIX3(streamp) strm, uInt size)
{
    struct dfltcc_state *dfltcc_state = (struct dfltcc_state *)((char *)strm->state + size);
    struct dfltcc_qaf_param *param = (struct dfltcc_qaf_param *)&dfltcc_state->param;

    /* Initialize available functions */
    if (is_dfltcc_enabled()) {
        dfltcc(DFLTCC_QAF, param, NULL, NULL, NULL, NULL, NULL);
        memmove(&dfltcc_state->af, param, sizeof(dfltcc_state->af));
    } else
        memset(&dfltcc_state->af, 0, sizeof(dfltcc_state->af));

    /* Initialize parameter block */
    memset(&dfltcc_state->param, 0, sizeof(dfltcc_state->param));
    dfltcc_state->param.nt = 1;

    /* Initialize tuning parameters */
    dfltcc_state->level_mask = DFLTCC_LEVEL_MASK;
    dfltcc_state->block_size = DFLTCC_BLOCK_SIZE;
    dfltcc_state->block_threshold = DFLTCC_FIRST_FHT_BLOCK_SIZE;
    dfltcc_state->dht_threshold = DFLTCC_DHT_MIN_SAMPLE_SIZE;
    dfltcc_state->param.ribm = DFLTCC_RIBM;
}

void ZLIB_INTERNAL *dfltcc_alloc_state(PREFIX3(streamp) strm, uInt items, uInt size)
{
    Assert((items * size) % 8 == 0,
           "The size of zlib-ng state must be a multiple of 8");
    return ZALLOC(strm, items * size + sizeof(struct dfltcc_state), sizeof(unsigned char));
}

void ZLIB_INTERNAL dfltcc_copy_state(void *dst, const void *src, uInt size)
{
    memcpy(dst, src, size + sizeof(struct dfltcc_state));
}

static const int PAGE_ALIGN = 0x1000;

#define ALIGN_UP(p, size) (__typeof__(p))(((uintptr_t)(p) + ((size) - 1)) & ~((size) - 1))

void ZLIB_INTERNAL *dfltcc_alloc_window(PREFIX3(streamp) strm, uInt items, uInt size)
{
    void *p;
    void *w;

    /* To simplify freeing, we store the pointer to the allocated buffer right
     * before the window.
     */
    p = ZALLOC(strm, sizeof(void *) + items * size + PAGE_ALIGN, sizeof(unsigned char));
    if (p == NULL)
        return NULL;
    w = ALIGN_UP((char *)p + sizeof(void *), PAGE_ALIGN);
    *(void **)((char *)w - sizeof(void *)) = p;
    return w;
}

void ZLIB_INTERNAL dfltcc_free_window(PREFIX3(streamp) strm, void *w)
{
    if (w)
        ZFREE(strm, *(void **)((unsigned char *)w - sizeof(void *)));
}
