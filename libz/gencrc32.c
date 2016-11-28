/* gencrc32.c -- compute the CRC-32 of a data stream
 * Copyright (C) 1995-2006, 2010, 2011, 2012 Mark Adler
 * For conditions of distribution and use, see copyright notice in zlib.h
 *
 * Thanks to Rodney Brown <rbrown64@csc.com.au> for his contribution of faster
 * CRC methods: exclusive-oring 32 bits of data at a time, and pre-computing
 * tables for updating the shift register in one step with three exclusive-ors
 * instead of four steps with four exclusive-ors.  This results in about a
 * factor of two increase in speed on a Power PC G4 (PPC7455) using gcc -O3.
 */

/* @(#) $Id$ */

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "zutil.h"

#define TBLS 8

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
int main(int argc, char *argv[])
{
    const char *out_path = NULL;
    if (2 <= argc) {
        if (!(freopen(out_path = argv[1], "w", stdout))) {
            fprintf(stderr, "%s: %s: %s\n", argv[0], out_path, strerror(errno));
            return 1;
        }
    }

    z_crc_t crc_table[TBLS][256];
    z_crc_t c;
    int n, k;
    z_crc_t poly;                       /* polynomial exclusive-or pattern */
    /* terms of polynomial defining this crc (except x^32): */
    const unsigned char p[] = {0,1,2,4,5,7,8,10,11,12,16,22,23,26};

    /* make exclusive-or pattern from polynomial (0xedb88320UL) */
    poly = 0;
    for (n = 0; n < (int)(sizeof(p)/sizeof(unsigned char)); n++)
        poly |= (z_crc_t)1 << (31 - p[n]);

    /* generate a crc for every 8-bit value */
    for (n = 0; n < 256; n++) {
        c = (z_crc_t)n;
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

    /* write out CRC tables */
    if (out_path)
        printf("/* %s -- tables for rapid CRC calculation\n", out_path);
    else
        printf("/* tables for rapid CRC calculation\n");
    printf(" * Generated automatically by %s\n */\n\n", argv[0]);
    printf("static const z_crc_t ");
    printf("crc_table[TBLS][256] =\n");
    printf("{\n");
    printf("  {\n");
    for (k = 0; k < TBLS; k++) {
        if (0 < k) {
            printf("  },\n");
            printf("  {\n");
        }
        for (int n = 0; n < 256; n++) {
            printf("%s0x%08lxUL%s", n % 5 ? "" : "    ",
                    (unsigned long)(crc_table[k][n]),
                    n == 255 ? "\n" : (n % 5 == 4 ? ",\n" : ", "));
        }
    }
    printf("  }\n");
    printf("};\n");

    if (ferror(stdout) || fflush(stdout) == EOF) {
        if (!out_path)
            out_path = "<stdout>";
        fprintf(stderr, "%s: %s: %s\n", argv[0], out_path, strerror(errno));
        return 1;
    }

    return 0;
}
