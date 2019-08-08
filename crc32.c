/* crc32.c -- compute the CRC-32 of a data stream
 * Copyright (C) 1995-2006, 2010, 2011, 2012, 2016, 2018 Mark Adler
 * For conditions of distribution and use, see copyright notice in zlib.h
 *
 * Thanks to Rodney Brown <rbrown64@csc.com.au> for his contribution of faster
 * CRC methods: exclusive-oring 32 bits of data at a time, and pre-computing
 * tables for updating the shift register in one step with three exclusive-ors
 * instead of four steps with four exclusive-ors.  This results in about a
 * factor of two increase in speed on a Power PC G4 (PPC7455) using gcc -O3.
 */

/* @(#) $Id$ */

# include "zbuild.h"
# include "zendian.h"
# include <inttypes.h>

/*
  Note on the use of DYNAMIC_CRC_TABLE: there is no mutex or semaphore
  protection on the static variables used to control the first-use generation
  of the crc tables.  Therefore, if you #define DYNAMIC_CRC_TABLE, you should
  first call get_crc_table() to initialize the tables before allowing more than
  one thread to use crc32().

  DYNAMIC_CRC_TABLE and MAKECRCH can be #defined to write out crc32.h. A main()
  routine is also produced, so that this one source file can be compiled to an
  executable.
 */

#ifdef MAKECRCH
#  include <stdio.h>
#  ifndef DYNAMIC_CRC_TABLE
#    define DYNAMIC_CRC_TABLE
#  endif /* !DYNAMIC_CRC_TABLE */
#endif /* MAKECRCH */

#include "deflate.h"
#include "functable.h"


/* Local functions for crc concatenation */
#define GF2_DIM 32      /* dimension of GF(2) vectors (length of CRC) */
static uint32_t gf2_matrix_times(const uint32_t *mat, uint32_t vec);
static uint32_t crc32_combine_(uint32_t crc1, uint32_t crc2, z_off64_t len2);
static void crc32_combine_gen_(uint32_t *op, z_off64_t len2);

/* ========================================================================= */
static uint32_t gf2_matrix_times(const uint32_t *mat, uint32_t vec) {
    uint32_t sum = 0;
    while (vec) {
        if (vec & 1)
            sum ^= *mat;
        vec >>= 1;
        mat++;
    }
    return sum;
}

#ifdef DYNAMIC_CRC_TABLE
volatile int crc_table_empty = 1;
static uint32_t crc_table[8][256];
static uint32_t crc_comb[GF2_DIM][GF2_DIM];
void make_crc_table(void);
static void gf2_matrix_square(uint32_t *square, const uint32_t *mat);
#ifdef MAKECRCH
static void write_table(FILE *, const uint32_t *, int);
#endif /* MAKECRCH */

/* ========================================================================= */
static void gf2_matrix_square(uint32_t *square, const uint32_t *mat) {
    int n;

    for (n = 0; n < GF2_DIM; n++)
        square[n] = gf2_matrix_times(mat, mat[n]);
}

/*
  Generate tables for a byte-wise 32-bit CRC calculation on the polynomial:
  x^32+x^26+x^23+x^22+x^16+x^12+x^11+x^10+x^8+x^7+x^5+x^4+x^2+x+1.

  Polynomials over GF(2) are represented in binary, one bit per coefficient,
  with the lowest powers in the most significant bit.  Then adding polynomials
  is just exclusive-or, and multiplying a polynomial by x is a right shift by
  one.  If we call the above polynomial p, and represent a byte as the
  polynomial q, also with the lowest power in the most significant bit (so the
  byte 0xb1 is the polynomial x^7+x^3+x+1), then the CRC is (q*x^32) mod p,
  where a mod b means the remainder after dividing a by b.

  This calculation is done using the shift-register method of multiplying and
  taking the remainder.  The register is initialized to zero, and for each
  incoming bit, x^32 is added mod p to the register if the bit is a one (where
  x^32 mod p is p+x^32 = x^26+...+1), and the register is multiplied mod p by
  x (which is shifting right by one and adding x^32 mod p if the bit shifted
  out is a one).  We start with the highest power (least significant bit) of
  q and repeat for all eight bits of q.

  The first table is simply the CRC of all possible eight bit values.  This is
  all the information needed to generate CRCs on data a byte at a time for all
  combinations of CRC register values and incoming bytes.  The remaining tables
  allow for word-at-a-time CRC calculation for both big-endian and little-
  endian machines, where a word is four bytes.
*/
void make_crc_table() {
    uint32_t c;
    int n, k;
    uint32_t poly;                       /* polynomial exclusive-or pattern */
    /* terms of polynomial defining this crc (except x^32): */
    static volatile int first = 1;      /* flag to limit concurrent making */
    static const unsigned char p[] = {0, 1, 2, 4, 5, 7, 8, 10, 11, 12, 16, 22, 23, 26};

    /* See if another task is already doing this (not thread-safe, but better
       than nothing -- significantly reduces duration of vulnerability in
       case the advice about DYNAMIC_CRC_TABLE is ignored) */
    if (first) {
        first = 0;

        /* make exclusive-or pattern from polynomial (0xedb88320) */
        poly = 0;
        for (n = 0; n < (int)(sizeof(p)/sizeof(unsigned char)); n++)
            poly |= (uint32_t)1 << (31 - p[n]);

        /* generate a crc for every 8-bit value */
        for (n = 0; n < 256; n++) {
            c = (uint32_t)n;
            for (k = 0; k < 8; k++)
                c = c & 1 ? poly ^ (c >> 1) : c >> 1;
            crc_table[0][n] = c;
        }

        /* generate crc for each value followed by one, two, and three zeros,
           and then the byte reversal of those as well as the first table */
        for (n = 0; n < 256; n++) {
            c = crc_table[0][n];
            crc_table[4][n] = ZSWAP32(c);
            for (k = 1; k < 4; k++) {
                c = crc_table[0][c & 0xff] ^ (c >> 8);
                crc_table[k][n] = c;
                crc_table[k + 4][n] = ZSWAP32(c);
            }
        }

        /* generate zero operators table for crc32_combine() */

        /* generate the operator to apply a single zero bit to a CRC -- the
           first row adds the polynomial if the low bit is a 1, and the
           remaining rows shift the CRC right one bit */
        k = GF2_DIM - 3;
        crc_comb[k][0] = 0xedb88320UL;      /* CRC-32 polynomial */
        uint32_t row = 1;
        for (n = 1; n < GF2_DIM; n++) {
            crc_comb[k][n] = row;
            row <<= 1;
        }

        /* generate operators that apply 2, 4, and 8 zeros to a CRC, putting
           the last one, the operator for one zero byte, at the 0 position */
        gf2_matrix_square(crc_comb[k + 1], crc_comb[k]);
        gf2_matrix_square(crc_comb[k + 2], crc_comb[k + 1]);
        gf2_matrix_square(crc_comb[0], crc_comb[k + 2]);

        /* generate operators for applying 2^n zero bytes to a CRC, filling out
           the remainder of the table -- the operators repeat after GF2_DIM
           values of n, so the table only needs GF2_DIM entries, regardless of
           the size of the length being processed */
        for (n = 1; n < k; n++)
            gf2_matrix_square(crc_comb[n], crc_comb[n - 1]);

        /* mark tables as complete, in case someone else is waiting */
        crc_table_empty = 0;
    } else {      /* not first */
        /* wait for the other guy to finish (not efficient, but rare) */
        while (crc_table_empty)
            {}
    }
#ifdef MAKECRCH
    {
        FILE *out;

        out = fopen("crc32.h", "w");
        if (out == NULL) return;

        /* write out CRC table to crc32.h */
        fprintf(out, "/* crc32.h -- tables for rapid CRC calculation\n");
        fprintf(out, " * Generated automatically by crc32.c\n */\n\n");
        fprintf(out, "static const uint32_t ");
        fprintf(out, "crc_table[8][256] =\n{\n  {\n");
        write_table(out, crc_table[0], 256);
        for (k = 1; k < 8; k++) {
            fprintf(out, "  },\n  {\n");
            write_table(out, crc_table[k], 256);
        }
        fprintf(out, "  }\n};\n");

        /* write out zero operator table to crc32.h */
        fprintf(out, "\nstatic const uint32_t ");
        fprintf(out, "crc_comb[%d][%d] =\n{\n  {\n", GF2_DIM, GF2_DIM);
        write_table(out, crc_comb[0], GF2_DIM);
        for (k = 1; k < GF2_DIM; k++) {
            fprintf(out, "  },\n  {\n");
            write_table(out, crc_comb[k], GF2_DIM);
        }
        fprintf(out, "  }\n};\n");
        fclose(out);
    }
#endif /* MAKECRCH */
}

#ifdef MAKECRCH
static void write_table(FILE *out, const uint32_t *table, int k) {
    int n;

    for (n = 0; n < k; n++)
        fprintf(out, "%s0x%08" PRIx32 "%s", n % 5 ? "" : "    ",
                (uint32_t)(table[n]),
                n == k - 1 ? "\n" : (n % 5 == 4 ? ",\n" : ", "));
}

int main()
{
    make_crc_table();
    return 0;
}
#endif /* MAKECRCH */

#else /* !DYNAMIC_CRC_TABLE */
/* ========================================================================
 * Tables of CRC-32s of all single-byte values, made by make_crc_table(),
 * and tables of zero operator matrices for crc32_combine().
 */
#include "crc32.h"
#endif /* DYNAMIC_CRC_TABLE */

/* =========================================================================
 * This function can be used by asm versions of crc32()
 */
const uint32_t * ZEXPORT PREFIX(get_crc_table)(void) {
#ifdef DYNAMIC_CRC_TABLE
    if (crc_table_empty)
        make_crc_table();
#endif /* DYNAMIC_CRC_TABLE */
    return (const uint32_t *)crc_table;
}

uint32_t ZEXPORT PREFIX(crc32_z)(uint32_t crc, const unsigned char *buf, size_t len) {
    if (buf == NULL) return 0;

    return functable.crc32(crc, buf, len);
}
/* ========================================================================= */
#define DO1 crc = crc_table[0][((int)crc ^ (*buf++)) & 0xff] ^ (crc >> 8)
#define DO8 DO1; DO1; DO1; DO1; DO1; DO1; DO1; DO1
#define DO4 DO1; DO1; DO1; DO1

/* ========================================================================= */
ZLIB_INTERNAL uint32_t crc32_generic(uint32_t crc, const unsigned char *buf, uint64_t len)
{
    crc = crc ^ 0xffffffff;

#ifdef UNROLL_MORE
    while (len >= 8) {
        DO8;
        len -= 8;
    }
#else
    while (len >= 4) {
        DO4;
        len -= 4;
    }
#endif

    if (len) do {
        DO1;
    } while (--len);
    return crc ^ 0xffffffff;
}

uint32_t ZEXPORT PREFIX(crc32)(uint32_t crc, const unsigned char *buf, uint32_t len) {
    return PREFIX(crc32_z)(crc, buf, len);
}

/*
   This BYFOUR code accesses the passed unsigned char * buffer with a 32-bit
   integer pointer type. This violates the strict aliasing rule, where a
   compiler can assume, for optimization purposes, that two pointers to
   fundamentally different types won't ever point to the same memory. This can
   manifest as a problem only if one of the pointers is written to. This code
   only reads from those pointers. So long as this code remains isolated in
   this compilation unit, there won't be a problem. For this reason, this code
   should not be copied and pasted into a compilation unit in which other code
   writes to the buffer that is passed to these routines.
 */

/* ========================================================================= */
#if BYTE_ORDER == LITTLE_ENDIAN
#define DOLIT4 c ^= *buf4++; \
        c = crc_table[3][c & 0xff] ^ crc_table[2][(c >> 8) & 0xff] ^ \
            crc_table[1][(c >> 16) & 0xff] ^ crc_table[0][c >> 24]
#define DOLIT32 DOLIT4; DOLIT4; DOLIT4; DOLIT4; DOLIT4; DOLIT4; DOLIT4; DOLIT4

/* ========================================================================= */
ZLIB_INTERNAL uint32_t crc32_little(uint32_t crc, const unsigned char *buf, uint64_t len) {
    register uint32_t c;
    register const uint32_t *buf4;

    c = crc;
    c = ~c;
    while (len && ((ptrdiff_t)buf & 3)) {
        c = crc_table[0][(c ^ *buf++) & 0xff] ^ (c >> 8);
        len--;
    }

    buf4 = (const uint32_t *)(const void *)buf;

#ifdef UNROLL_MORE
    while (len >= 32) {
        DOLIT32;
        len -= 32;
    }
#endif

    while (len >= 4) {
        DOLIT4;
        len -= 4;
    }
    buf = (const unsigned char *)buf4;

    if (len) do {
        c = crc_table[0][(c ^ *buf++) & 0xff] ^ (c >> 8);
    } while (--len);
    c = ~c;
    return c;
}
#endif /* BYTE_ORDER == LITTLE_ENDIAN */

/* ========================================================================= */
#if BYTE_ORDER == BIG_ENDIAN
#define DOBIG4 c ^= *buf4++; \
        c = crc_table[4][c & 0xff] ^ crc_table[5][(c >> 8) & 0xff] ^ \
            crc_table[6][(c >> 16) & 0xff] ^ crc_table[7][c >> 24]
#define DOBIG32 DOBIG4; DOBIG4; DOBIG4; DOBIG4; DOBIG4; DOBIG4; DOBIG4; DOBIG4

/* ========================================================================= */
ZLIB_INTERNAL uint32_t crc32_big(uint32_t crc, const unsigned char *buf, uint64_t len) {
    register uint32_t c;
    register const uint32_t *buf4;

    c = ZSWAP32(crc);
    c = ~c;
    while (len && ((ptrdiff_t)buf & 3)) {
        c = crc_table[4][(c >> 24) ^ *buf++] ^ (c << 8);
        len--;
    }

    buf4 = (const uint32_t *)(const void *)buf;

#ifdef UNROLL_MORE
    while (len >= 32) {
        DOBIG32;
        len -= 32;
    }
#endif

    while (len >= 4) {
        DOBIG4;
        len -= 4;
    }
    buf = (const unsigned char *)buf4;

    if (len) do {
        c = crc_table[4][(c >> 24) ^ *buf++] ^ (c << 8);
    } while (--len);
    c = ~c;
    return ZSWAP32(c);
}
#endif /* BYTE_ORDER == BIG_ENDIAN */


/* ========================================================================= */
static uint32_t crc32_combine_(uint32_t crc1, uint32_t crc2, z_off64_t len2) {
    int n;

#ifdef DYNAMIC_CRC_TABLE
    if (crc_table_empty)
        make_crc_table();
#endif /* DYNAMIC_CRC_TABLE */

    if (len2 > 0)
        /* operator for 2^n zeros repeats every GF2_DIM n values */
        for (n = 0; len2; n = (n + 1) % GF2_DIM, len2 >>= 1)
            if (len2 & 1)
                crc1 = gf2_matrix_times(crc_comb[n], crc1);
    return crc1 ^ crc2;
}

/* ========================================================================= */
uint32_t ZEXPORT PREFIX(crc32_combine)(uint32_t crc1, uint32_t crc2, z_off_t len2) {
    return crc32_combine_(crc1, crc2, len2);
}

uint32_t ZEXPORT PREFIX(crc32_combine64)(uint32_t crc1, uint32_t crc2, z_off64_t len2) {
    return crc32_combine_(crc1, crc2, len2);
}

#ifdef X86_PCLMULQDQ_CRC
#include "arch/x86/x86.h"
#include "arch/x86/crc_folding.h"

ZLIB_INTERNAL void crc_finalize(deflate_state *const s) {
    if (x86_cpu_has_pclmulqdq)
        s->strm->adler = crc_fold_512to32(s);
}
#endif

ZLIB_INTERNAL void crc_reset(deflate_state *const s) {
#ifdef X86_PCLMULQDQ_CRC
    if (x86_cpu_has_pclmulqdq) {
        crc_fold_init(s);
        return;
    }
#endif
    s->strm->adler = PREFIX(crc32)(0L, NULL, 0);
}

ZLIB_INTERNAL void copy_with_crc(PREFIX3(stream) *strm, unsigned char *dst, unsigned long size) {
#ifdef X86_PCLMULQDQ_CRC
    if (x86_cpu_has_pclmulqdq) {
        crc_fold_copy(strm->state, dst, strm->next_in, size);
        return;
    }
#endif
    memcpy(dst, strm->next_in, size);
    strm->adler = PREFIX(crc32)(strm->adler, dst, size);
}

/* ========================================================================= */
static void crc32_combine_gen_(uint32_t *op, z_off64_t len2)
{
    uint32_t row;
    int j;
    unsigned i;

#ifdef DYNAMIC_CRC_TABLE
    if (crc_table_empty)
        make_crc_table();
#endif /* DYNAMIC_CRC_TABLE */

    /* if len2 is zero or negative, return the identity matrix */
    if (len2 <= 0) {
        row = 1;
        for (j = 0; j < GF2_DIM; j++) {
            op[j] = row;
            row <<= 1;
        }
        return;
    }

    /* at least one bit in len2 is set -- find it, and copy the operator
       corresponding to that position into op */
    i = 0;
    for (;;) {
        if (len2 & 1) {
            for (j = 0; j < GF2_DIM; j++)
                op[j] = crc_comb[i][j];
            break;
        }
        len2 >>= 1;
        i = (i + 1) % GF2_DIM;
    }

    /* for each remaining bit set in len2 (if any), multiply op by the operator
       corresponding to that position */
    for (;;) {
        len2 >>= 1;
        i = (i + 1) % GF2_DIM;
        if (len2 == 0)
            break;
        if (len2 & 1)
            for (j = 0; j < GF2_DIM; j++)
                op[j] = gf2_matrix_times(crc_comb[i], op[j]);
    }
}

/* ========================================================================= */
void ZEXPORT PREFIX(crc32_combine_gen)(uint32_t *op, z_off_t len2)
{
    crc32_combine_gen_(op, len2);
}

void ZEXPORT PREFIX(crc32_combine_gen64)(uint32_t *op, z_off64_t len2)
{
    crc32_combine_gen_(op, len2);
}

/* ========================================================================= */
uint32_t ZEXPORT PREFIX(crc32_combine_op)(uint32_t crc1, uint32_t crc2, const uint32_t *op)
{
    return gf2_matrix_times(op, crc1) ^ crc2;
}
