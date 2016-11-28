/* crc32.c -- compute the CRC-32 of a data stream
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

#include <endian.h>
#include <stdint.h>

#include "zutil.h"

/* Definitions for doing the crc four data bytes at a time. */
#if BYTE_ORDER == LITTLE_ENDIAN
static unsigned long crc32_little(unsigned long, const unsigned char *, unsigned int);
#endif
#if BYTE_ORDER == BIG_ENDIAN
static unsigned long crc32_big(unsigned long, const unsigned char *, unsigned int);
#endif
#define TBLS 8

/* Local functions for crc concatenation */
static unsigned long gf2_matrix_times(unsigned long *mat, unsigned long vec);
static void gf2_matrix_square(unsigned long *square, unsigned long *mat);
static unsigned long crc32_combine_(unsigned long crc1, unsigned long crc2, off_t len2);

/* ========================================================================
 * Tables of CRC-32s of all single-byte values.
 */
#include "crc32.h"

/* =========================================================================
 * This function can be used by asm versions of crc32()
 */
const z_crc_t * ZEXPORT get_crc_table(void)
{
    return (const z_crc_t *)crc_table;
}

/* ========================================================================= */
#define DO1 crc = crc_table[0][((int)crc ^ (*buf++)) & 0xff] ^ (crc >> 8)
#define DO8 DO1; DO1; DO1; DO1; DO1; DO1; DO1; DO1

/* ========================================================================= */
unsigned long ZEXPORT crc32(unsigned long crc,
                            const unsigned char *buf,
                            unsigned int len)
{
    if (buf == NULL) return 0UL;

    if (sizeof(void *) == sizeof(ptrdiff_t)) {
#if BYTE_ORDER == LITTLE_ENDIAN
        return crc32_little(crc, buf, len);
#endif
#if BYTE_ORDER == BIG_ENDIAN
        return crc32_big(crc, buf, len);
#endif
    }
    crc = crc ^ 0xffffffffUL;
    while (len >= 8) {
        DO8;
        len -= 8;
    }
    if (len) do {
        DO1;
    } while (--len);
    return crc ^ 0xffffffffUL;
}

#if BYTE_ORDER == LITTLE_ENDIAN
/* ========================================================================= */
#define DOLIT4 c ^= *buf4++; \
        c = crc_table[3][c & 0xff] ^ crc_table[2][(c >> 8) & 0xff] ^ \
            crc_table[1][(c >> 16) & 0xff] ^ crc_table[0][c >> 24]
#define DOLIT32 DOLIT4; DOLIT4; DOLIT4; DOLIT4; DOLIT4; DOLIT4; DOLIT4; DOLIT4

/* ========================================================================= */
static unsigned long crc32_little(unsigned long crc,
                                  const unsigned char *buf,
                                  unsigned int len)
{
    z_crc_t c;
    const z_crc_t *buf4;

    c = (z_crc_t)crc;
    c = ~c;
    while (len && ((ptrdiff_t)buf & 3)) {
        c = crc_table[0][(c ^ *buf++) & 0xff] ^ (c >> 8);
        len--;
    }

    buf4 = (const z_crc_t *)(const void *)buf;
    while (len >= 32) {
        DOLIT32;
        len -= 32;
    }
    while (len >= 4) {
        DOLIT4;
        len -= 4;
    }
    buf = (const unsigned char *)buf4;

    if (len) do {
        c = crc_table[0][(c ^ *buf++) & 0xff] ^ (c >> 8);
    } while (--len);
    c = ~c;
    return (unsigned long)c;
}
#endif

#if BYTE_ORDER == BIG_ENDIAN
/* ========================================================================= */
#define DOBIG4 c ^= *++buf4; \
        c = crc_table[4][c & 0xff] ^ crc_table[5][(c >> 8) & 0xff] ^ \
            crc_table[6][(c >> 16) & 0xff] ^ crc_table[7][c >> 24]
#define DOBIG32 DOBIG4; DOBIG4; DOBIG4; DOBIG4; DOBIG4; DOBIG4; DOBIG4; DOBIG4

/* ========================================================================= */
static unsigned long crc32_big(unsigned long crc,
                               const unsigned char *buf,
                               unsigned int len)
{
    z_crc_t c;
    const z_crc_t *buf4;

    c = ZSWAP32((z_crc_t)crc);
    c = ~c;
    while (len && ((ptrdiff_t)buf & 3)) {
        c = crc_table[4][(c >> 24) ^ *buf++] ^ (c << 8);
        len--;
    }

    buf4 = (const z_crc_t *)(const void *)buf;
    buf4--;
    while (len >= 32) {
        DOBIG32;
        len -= 32;
    }
    while (len >= 4) {
        DOBIG4;
        len -= 4;
    }
    buf4++;
    buf = (const unsigned char *)buf4;

    if (len) do {
        c = crc_table[4][(c >> 24) ^ *buf++] ^ (c << 8);
    } while (--len);
    c = ~c;
    return (unsigned long)(ZSWAP32(c));
}
#endif


#define GF2_DIM 32      /* dimension of GF(2) vectors (length of CRC) */

/* ========================================================================= */
static unsigned long gf2_matrix_times(unsigned long *mat,
                                      unsigned long vec)
{
    unsigned long sum;

    sum = 0;
    while (vec) {
        if (vec & 1)
            sum ^= *mat;
        vec >>= 1;
        mat++;
    }
    return sum;
}

/* ========================================================================= */
static void gf2_matrix_square(unsigned long *square,
                              unsigned long *mat)
{
    int n;

    for (n = 0; n < GF2_DIM; n++)
        square[n] = gf2_matrix_times(mat, mat[n]);
}

/* ========================================================================= */
static unsigned long crc32_combine_(unsigned long crc1,
                                    unsigned long crc2,
                                    off_t len2)
{
    int n;
    unsigned long row;
    unsigned long even[GF2_DIM];    /* even-power-of-two zeros operator */
    unsigned long odd[GF2_DIM];     /* odd-power-of-two zeros operator */

    /* degenerate case (also disallow negative lengths) */
    if (len2 <= 0)
        return crc1;

    /* put operator for one zero bit in odd */
    odd[0] = 0xedb88320UL;          /* CRC-32 polynomial */
    row = 1;
    for (n = 1; n < GF2_DIM; n++) {
        odd[n] = row;
        row <<= 1;
    }

    /* put operator for two zero bits in even */
    gf2_matrix_square(even, odd);

    /* put operator for four zero bits in odd */
    gf2_matrix_square(odd, even);

    /* apply len2 zeros to crc1 (first square will put the operator for one
       zero byte, eight zero bits, in even) */
    do {
        /* apply zeros operator for this bit of len2 */
        gf2_matrix_square(even, odd);
        if (len2 & 1)
            crc1 = gf2_matrix_times(even, crc1);
        len2 >>= 1;

        /* if no more bits set, then done */
        if (len2 == 0)
            break;

        /* another iteration of the loop with odd and even swapped */
        gf2_matrix_square(odd, even);
        if (len2 & 1)
            crc1 = gf2_matrix_times(odd, crc1);
        len2 >>= 1;

        /* if no more bits set, then done */
    } while (len2 != 0);

    /* return combined crc */
    crc1 ^= crc2;
    return crc1;
}

/* ========================================================================= */
unsigned long ZEXPORT crc32_combine(unsigned long crc1, unsigned long crc2, z_default_off_t len2)
{
    return crc32_combine_(crc1, crc2, len2);
}

unsigned long ZEXPORT crc32_combine64(unsigned long crc1, unsigned long crc2, off_t len2)
{
    return crc32_combine_(crc1, crc2, len2);
}
