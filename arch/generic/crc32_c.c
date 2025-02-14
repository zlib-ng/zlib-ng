#include "zbuild.h"
#include "crc32_braid_p.h"
#include "crc32_c.h"

/* Size thresholds for Chorba algorithm variants */
#define CHORBA_LARGE_THRESHOLD (sizeof(z_word_t) * 64 * 1024)
#define CHORBA_MEDIUM_UPPER_THRESHOLD 32768
#define CHORBA_MEDIUM_LOWER_THRESHOLD 8192
#define CHORBA_SMALL_THRESHOLD_64BIT 72
#define CHORBA_SMALL_THRESHOLD_32BIT 80

uint32_t PREFIX(crc32_c)(uint32_t crc, const uint8_t *buf, size_t len) {
    uint32_t c;
    uint64_t* aligned_buf;
    size_t aligned_len;

    c = (~crc) & 0xffffffff;
#ifndef WITHOUT_CHORBA
    unsigned long algn_diff = ((uintptr_t)8 - ((uintptr_t)buf & 0xF)) & 0xF;
    if (algn_diff < len) {
        if (algn_diff) {
            c = crc32_braid_internal(c, buf, algn_diff);
        }
        aligned_buf = (uint64_t*) (buf + algn_diff);
        aligned_len = len - algn_diff;
        if(aligned_len > CHORBA_LARGE_THRESHOLD)
            c = crc32_chorba_118960_nondestructive(c, (z_word_t*) aligned_buf, aligned_len);
#  if W == 8
        else if (aligned_len > CHORBA_MEDIUM_LOWER_THRESHOLD && aligned_len <= CHORBA_MEDIUM_UPPER_THRESHOLD)
            c = crc32_chorba_32768_nondestructive(c, (uint64_t*) aligned_buf, aligned_len);
        else if (aligned_len > CHORBA_SMALL_THRESHOLD_64BIT)
            c = crc32_chorba_small_nondestructive(c, (uint64_t*) aligned_buf, aligned_len);
#  else
        else if (aligned_len > CHORBA_SMALL_THRESHOLD_32BIT)
            c = crc32_chorba_small_nondestructive_32bit(c, (uint32_t*) aligned_buf, aligned_len);
#  endif
        else
            c = crc32_braid_internal(c, (uint8_t*) aligned_buf, aligned_len);
    }
    else {
        c = crc32_braid_internal(c, buf, len);
    }
#else
    c = crc32_braid_internal(c, buf, len);
#endif /* WITHOUT_CHORBA */

    /* Return the CRC, post-conditioned. */
    return c ^ 0xffffffff;
}
