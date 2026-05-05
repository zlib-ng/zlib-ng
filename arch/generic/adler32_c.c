/* adler32.c -- compute the Adler-32 checksum of a data stream
 * Copyright (C) 1995-2011, 2016 Mark Adler
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#include "zbuild.h"
#include "arch_functions.h"

#ifdef ADLER32_FALLBACK

#include "functable.h"
#include "adler32_p.h"

Z_INTERNAL uint32_t adler32_c(uint32_t adler, const uint8_t *buf, size_t len) {
    uint32_t sum2;
    size_t n;

    /* split Adler-32 into component sums */
    sum2 = (adler >> 16) & 0xffff;
    adler &= 0xffff;

    /* in case user likes doing a byte at a time, keep it fast */
    if (UNLIKELY(len == 1))
        return adler32_copy_tail(adler, NULL, buf, 1, sum2, 1, 1, 0);

    /* in case short lengths are provided, keep it somewhat fast */
    if (UNLIKELY(len < 16))
        return adler32_copy_tail(adler, NULL, buf, len, sum2, 1, 15, 0);

    /* Align source to 8 bytes so SWAR loads are naturally aligned */
    size_t align_diff = ALIGN_DIFF(buf, 8);
    if (align_diff) {
        adler32_copy_align(&adler, NULL, buf, align_diff, &sum2, 7, 0);
        buf += align_diff;
        len -= align_diff;
    }

    /* do length NMAX blocks -- requires just one modulo operation */
    while (len >= NMAX) {
        len -= NMAX;
        n = NMAX;

        do {
            size_t chunk = MIN(ALIGN_DOWN(n, (size_t)8), (size_t)ADLER32_SWAR_MAX_BYTES);
            adler32_swar(&adler, NULL, buf, chunk, &sum2, 0);
            buf += chunk;
            n -= chunk;
        } while (n >= 8);

        adler %= BASE;
        sum2 %= BASE;
    }

    /* do remaining bytes (less than NMAX, still just one modulo) */
    return adler32_copy_tail(adler, NULL, buf, len, sum2, len != 0, NMAX - 1, 0);
}

Z_INTERNAL uint32_t adler32_copy_c(uint32_t adler, uint8_t *dst, const uint8_t *src, size_t len) {
    adler = adler32_c(adler, src, len);
    memcpy(dst, src, len);
    return adler;
}

#endif /* ADLER32_FALLBACK */
