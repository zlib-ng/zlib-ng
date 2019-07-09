/* crc32.c -- output crc32 tables
 * Copyright (C) 1995-2006, 2010, 2011, 2012, 2016, 2018 Mark Adler
 * For conditions of distribution and use, see copyright notice in zlib.h
*/

#include <stdio.h>
#include <inttypes.h>
#include "zbuild.h"
#include "deflate.h"
#include "crc32_p.h"

static uint32_t crc_table[8][256];
static uint32_t crc_comb[GF2_DIM][GF2_DIM];

static void gf2_matrix_square(uint32_t *square, const uint32_t *mat);
static void make_crc_table(void);
static void make_crc_combine_table(void);
static void print_crc_table(void);
static void print_crc_combine_table(void);
static void write_table(const uint32_t *, int);


/* ========================================================================= */
static void gf2_matrix_square(uint32_t *square, const uint32_t *mat) {
    int n;

    for (n = 0; n < GF2_DIM; n++)
        square[n] = gf2_matrix_times(mat, mat[n]);
}

/* =========================================================================
  Generate tables for a byte-wise 32-bit CRC calculation on the polynomial:
  x^32+x^26+x^23+x^22+x^16+x^12+x^11+x^10+x^8+x^7+x^5+x^4+x^2+x+1.

  Polynomials over GF(2) are represented in binary, one bit per coefficient,
  with the lowest powers in the most significant bit.  Then adding polynomials
  is just exclusive-or, and multiplying a polynomial by x is a right shift by
  one.  If we call the above polynomial p, and represent a byte as the
  polynomial q, also with the lowest power in the most significant bit (so the
  byte 0xb1 is the polynomial x^7+x^3+x^2+1), then the CRC is (q*x^32) mod p,
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
static void make_crc_table(void) {
    int n, k;
    uint32_t c;
    uint32_t poly;                       /* polynomial exclusive-or pattern */
    /* terms of polynomial defining this crc (except x^32): */
    static const unsigned char p[] = {0, 1, 2, 4, 5, 7, 8, 10, 11, 12, 16, 22, 23, 26};

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
}

static void make_crc_combine_table(void) {
    int n, k;
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
}

static void write_table(const uint32_t *table, int k) {
    int n;

    for (n = 0; n < k; n++)
        printf("%s0x%08" PRIx32 "%s", n % 5 ? "" : "    ",
                (uint32_t)(table[n]),
                n == k - 1 ? "\n" : (n % 5 == 4 ? ",\n" : ", "));
}

static void print_crc_table(void) {
    int k;
    printf("#ifndef CRC32_TBL_H_\n");
    printf("#define CRC32_TBL_H_\n\n");
    printf("/* crc32_tbl.h -- tables for rapid CRC calculation\n");
    printf(" * Generated automatically by makecrct.c\n */\n\n");

    /* print CRC table */
    printf("static const uint32_t ");
    printf("crc_table[8][256] =\n{\n  {\n");
    write_table(crc_table[0], 256);
    for (k = 1; k < 8; k++) {
        printf("  },\n  {\n");
        write_table(crc_table[k], 256);
    }
    printf("  }\n};\n\n");

    printf("#endif /* CRC32_TBL_H_ */\n");
}

static void print_crc_combine_table(void) {
    int k;
    printf("#ifndef CRC32_COMB_TBL_H_\n");
    printf("#define CRC32_COMB_TBL_H_\n\n");
    printf("/* crc32_comb_tbl.h -- zero operators table for CRC combine\n");
    printf(" * Generated automatically by makecrct.c\n */\n\n");

    /* print zero operator table */
    printf("static const uint32_t ");
    printf("crc_comb[%d][%d] =\n{\n  {\n", GF2_DIM, GF2_DIM);
    write_table(crc_comb[0], GF2_DIM);
    for (k = 1; k < GF2_DIM; k++) {
        printf("  },\n  {\n");
        write_table(crc_comb[k], GF2_DIM);
    }
    printf("  }\n};\n\n");

    printf("#endif /* CRC32_COMB_TBL_H_ */\n");
}

// The output of this application can be piped out to recreate crc32.h
int main(int argc, char *argv[]) {
    if (argc > 1 && strcmp(argv[1], "-c") == 0) {
        make_crc_combine_table();
        print_crc_combine_table();
    } else {
        make_crc_table();
        print_crc_table();
    }
    return 0;
}
