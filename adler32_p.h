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

/* use NO_DIVIDE if your processor does not do division in hardware --
   try it both ways to see which is faster */
#ifdef NO_DIVIDE
/* note that this assumes BASE is 65521, where 65536 % 65521 == 15
   (thank you to John Reiser for pointing this out) */
#  define CHOP(a) \
    do { \
        uint32_t tmp = a >> 16; \
        a &= 0xffff; \
        a += (tmp << 4) - tmp; \
    } while (0)
#  define MOD28(a) \
    do { \
        CHOP(a); \
        if (a >= BASE) a -= BASE; \
    } while (0)
#  define MOD(a) \
    do { \
        CHOP(a); \
        MOD28(a); \
    } while (0)
#  define MOD63(a) \
    do { /* this assumes a is not negative */ \
        z_off64_t tmp = a >> 32; \
        a &= 0xffffffffL; \
        a += (tmp << 8) - (tmp << 5) + tmp; \
        tmp = a >> 16; \
        a &= 0xffffL; \
        a += (tmp << 4) - tmp; \
        tmp = a >> 16; \
        a &= 0xffffL; \
        a += (tmp << 4) - tmp; \
        if (a >= BASE) a -= BASE; \
    } while (0)
#else
#  define MOD(a) a %= BASE
#  define MOD28(a) a %= BASE
#  define MOD63(a) a %= BASE
#endif

static inline uint32_t adler32_len_1(uint32_t adler, const unsigned char *buf, uint32_t sum2) {
    adler += buf[0];
    if (adler >= BASE)
        adler -= BASE;
    sum2 += adler;
    if (sum2 >= BASE)
        sum2 -= BASE;
    return adler | (sum2 << 16);
}

static inline uint32_t adler32_len_16(uint32_t adler, const unsigned char *buf, size_t len, uint32_t sum2) {
    while (len) {
        --len;
        adler += *buf++;
        sum2 += adler;
    }
    if (adler >= BASE)
        adler -= BASE;
    MOD28(sum2);            /* only added so many BASE's */
    return adler | (sum2 << 16);
}

#endif /* ADLER32_P_H */
