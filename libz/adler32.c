/* adler32.c -- compute the Adler-32 checksum of a data stream
 * Copyright (C) 1995-2011 Mark Adler
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

/* @(#) $Id$ */

#include <stddef.h>
#include <stdint.h>

#include "zutil.h"

#define BASE 65521      /* largest prime smaller than 65536 */
#define BASE_X2 131042  /* twice BASE */
#define NMAX 5552
/* NMAX is the largest n such that 255n(n+1)/2 + (n+1)(BASE-1) <= 2^32-1 */

/* ========================================================================= */
unsigned long ZEXPORT adler32(unsigned long adler,
                              const unsigned char *buf,
                              unsigned int len)
{
    /* initial Adler-32 value */
    if (buf == NULL)
        return UINT32_C(1) << 0 | UINT32_C(0) << 16;

    /* split Adler-32 into component sums */
    uint32_t a = adler >>  0 & 0xFFFF; /* (sum of all bytes) % BASE */
    uint32_t b = adler >> 16 & 0xFFFF; /* (sum of all a) % BASE */

    while (len) {
        size_t i;
        for (i = 0; i < len && i < NMAX; i++) {
            a += buf[i];
            b += a;
        }
        a %= BASE;
        b %= BASE;
        buf += i;
        len -= i;
    }

    return a << 0 | b << 16;
}

/* ========================================================================= */
static unsigned long adler32_combine_(unsigned long adler1,
                                      unsigned long adler2,
                                      off_t len2)
{
    /* for negative len, return invalid adler32 as a clue for debugging */
    if (len2 < 0)
        return UINT32_C(0xFFFF) << 0 | UINT32_C(0xFFFF) << 16;

    /* the derivation of this formula is left as an exercise for the reader */
    uint32_t adler1_a = adler1 >>  0 & 0xFFFF;
    uint32_t adler1_b = adler1 >> 16 & 0xFFFF;
    uint32_t adler2_a = adler2 >>  0 & 0xFFFF;
    uint32_t adler2_b = adler2 >> 16 & 0xFFFF;
    uint32_t rem = len2 % BASE;
    uint32_t sum1 = adler1_a;
    uint32_t sum2 = (rem * sum1) % BASE;
    sum1 += adler2_a + BASE - 1;
    sum2 += adler1_b + adler2_b + BASE - rem;
    /* sum1 %= BASE; */
    if (sum1 >= BASE) sum1 -= BASE;
    if (sum1 >= BASE) sum1 -= BASE;
    /* sum2 %= BASE; */
    if (sum2 >= BASE_X2) sum2 -= BASE_X2;
    if (sum2 >= BASE) sum2 -= BASE;
    return sum1 << 0 | sum2 << 16;
}

/* ========================================================================= */
unsigned long ZEXPORT adler32_combine(unsigned long adler1,
                                      unsigned long adler2,
                                      z_default_off_t len2)
{
    return adler32_combine_(adler1, adler2, len2);
}

unsigned long ZEXPORT adler32_combine64(unsigned long adler1,
                                        unsigned long adler2,
                                        off_t len2)
{
    return adler32_combine_(adler1, adler2, len2);
}
