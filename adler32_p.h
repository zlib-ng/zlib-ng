/* adler32_p.h -- Private inline functions and macros shared with
 *                different computation of the Adler-32 checksum
 *                of a data stream.
 * Copyright (C) 1995-2011, 2016 Mark Adler
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#ifndef ADLER32_P_H
#define ADLER32_P_H

#define BASE 65521U     /* largest prime smaller than 65536 */
#define NMAX 5552
/* NMAX is the largest n such that 255n(n+1)/2 + (n+1)(BASE-1) <= 2^32-1 */

#define ADLER_DO1(sum1, sum2, buf, i)  {(sum1) += buf[(i)]; (sum2) += (sum1);}
#define ADLER_DO2(sum1, sum2, buf, i)  {ADLER_DO1(sum1, sum2, buf, i); ADLER_DO1(sum1, sum2, buf, i+1);}
#define ADLER_DO4(sum1, sum2, buf, i)  {ADLER_DO2(sum1, sum2, buf, i); ADLER_DO2(sum1, sum2, buf, i+2);}
#define ADLER_DO8(sum1, sum2, buf, i)  {ADLER_DO4(sum1, sum2, buf, i); ADLER_DO4(sum1, sum2, buf, i+4);}
#define ADLER_DO16(sum1, sum2, buf)    {ADLER_DO8(sum1, sum2, buf, 0); ADLER_DO8(sum1, sum2, buf, 8);}

Z_FORCEINLINE static uint32_t adler32_copy_small(uint32_t adler, uint8_t *dst, const uint8_t *buf, size_t len, uint32_t sum2, const int MAX_LEN, const int COPY) {
    if (COPY) {
        memcpy(dst, buf, len);
    }
    if (MAX_LEN > 16) {
        while (len >= 16) {
            len -= 16;
            ADLER_DO16(adler, sum2, buf);
            buf += 16;
        }
    }
    while (len >= 8) {
        len -= 8;
        ADLER_DO8(adler, sum2, buf, 0);
        buf += 8;
    }
    while (len--) {
        ADLER_DO1(adler, sum2, buf, 0);
        buf++;
    }
    /* D = B * 65536 + A, see: https://en.wikipedia.org/wiki/Adler-32. */
    return (adler % BASE) | ((sum2 % BASE) << 16);
}

Z_FORCEINLINE static uint32_t adler32_copy_small_pair(uint32_t *pair, uint8_t *dst, const uint8_t *buf, size_t len, const int MAX_LEN, const int COPY) {
    if (COPY) {
        memcpy(dst, buf, len);
    }
    if (MAX_LEN > 16) {
        while (len >= 16) {
            len -= 16;
            ADLER_DO16(pair[0], pair[1], buf);
            buf += 16;
        }
    }
    while (len >= 8) {
        len -= 8;
        ADLER_DO8(pair[0], pair[1], buf, 0);
        buf += 8;
    }
    while (len--) {
        ADLER_DO1(pair[0], pair[1], buf, 0);
        buf++;
    }
    /* D = B * 65536 + A, see: https://en.wikipedia.org/wiki/Adler-32. */
    return (pair[0] % BASE) | ((pair[1] % BASE) << 16);
}

#endif /* ADLER32_P_H */
