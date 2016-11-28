/* gentrees.c -- output deflated data using Huffman coding
 * Copyright (C) 1995-2012 Jean-loup Gailly
 * detect_data_type() function provided freely by Cosmin Truta, 2006
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "deflate.h"

/* ===========================================================================
 * Reverse the first len bits of a code, using straightforward code (a faster
 * method would use a table)
 * IN assertion: 1 <= len <= 15
 */
static unsigned int bi_reverse(
    unsigned int code, /* the value to invert */
    int len            /* its bit length */)
{
    unsigned int res = 0;
    do {
        res |= code & 1;
        code >>= 1, res <<= 1;
    } while (--len > 0);
    return res >> 1;
}

/* ===========================================================================
 * Generate the codes for a given tree and bit counts (which need not be
 * optimal).
 * IN assertion: the array bl_count contains the bit length statistics for
 * the given tree and the field len is set for all tree elements.
 * OUT assertion: the field code is set for all tree elements of non
 *     zero code length.
 */
static void gen_codes(
    ct_data *tree,             /* the tree to decorate */
    int max_code,              /* largest code with non zero frequency */
    unsigned short *bl_count   /* number of codes at each bit length */)
{
    unsigned short next_code[MAX_BITS+1]; /* next code value for each bit length */
    unsigned short code = 0;   /* running code value */
    int bits;                  /* bit index */
    int n;                     /* code index */

    /* The distribution counts are first used to generate the code values
     * without bit reversal.
     */
    for (bits = 1; bits <= MAX_BITS; bits++) {
        next_code[bits] = code = (code + bl_count[bits-1]) << 1;
    }
    /* Check that the bit counts in bl_count are consistent. The last code
     * must be all ones.
     */
    assert(code + bl_count[MAX_BITS]-1 == (1<<MAX_BITS)-1);

    for (n = 0;  n <= max_code; n++) {
        int len = tree[n].Len;
        if (len == 0) continue;
        /* Now reverse the bits */
        tree[n].Code = bi_reverse(next_code[len]++, len);
    }
}

static const int extra_lbits[LENGTH_CODES] /* extra bits for each length code */
   = {0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0};

static const int extra_dbits[D_CODES] /* extra bits for each distance code */
   = {0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13};


#define DIST_CODE_LEN  512 /* see definition of array dist_code below */

static ct_data static_ltree[L_CODES+2];
/* The static literal tree. Since the bit lengths are imposed, there is no
 * need for the L_CODES extra codes used during heap construction. However
 * The codes 286 and 287 are needed to build a canonical tree (see _tr_init
 * below).
 */

static ct_data static_dtree[D_CODES];
/* The static distance tree. (Actually a trivial tree since all codes use
 * 5 bits.)
 */

static unsigned char dist_code[DIST_CODE_LEN];
/* Distance codes. The first 256 values correspond to the distances
 * 3 .. 258, the last 256 values correspond to the top 8 bits of
 * the 15 bit distances.
 */

static unsigned char length_code[MAX_MATCH-MIN_MATCH+1];
/* length code for each normalized match length (0 == MIN_MATCH) */

static int base_length[LENGTH_CODES];
/* First normalized length for each code (0 = MIN_MATCH) */

static int base_dist[D_CODES];
/* First normalized distance for each code (0 = distance of 1) */

#define SEPARATOR(i, last, width) \
      ((i) == (last)? "\n};\n\n" :    \
       ((i) % (width) == (width)-1 ? ",\n" : ", "))

int main(int argc, char *argv[])
{
    const char *out_path = NULL;
    if (2 <= argc) {
        if (!(freopen(out_path = argv[1], "w", stdout))) {
            fprintf(stderr, "%s: %s: %s\n", argv[0], out_path, strerror(errno));
            return 1;
        }
    }

    int n;        /* iterates over tree elements */
    int bits;     /* bit counter */
    int length;   /* length value */
    int code;     /* code value */
    int dist;     /* distance index */
    unsigned short bl_count[MAX_BITS+1];
    /* number of codes at each bit length for an optimal tree */

    /* Initialize the mapping length (0..255) -> length code (0..28) */
    length = 0;
    for (code = 0; code < LENGTH_CODES-1; code++) {
        base_length[code] = length;
        for (n = 0; n < (1<<extra_lbits[code]); n++) {
            length_code[length++] = (unsigned char)code;
        }
    }
    assert(length == 256);
    /* Note that the length 255 (match length 258) can be represented
     * in two different ways: code 284 + 5 bits or code 285, so we
     * overwrite length_code[255] to use the best encoding:
     */
    length_code[length-1] = (unsigned char)code;

    /* Initialize the mapping dist (0..32K) -> dist code (0..29) */
    dist = 0;
    for (code = 0 ; code < 16; code++) {
        base_dist[code] = dist;
        for (n = 0; n < (1<<extra_dbits[code]); n++) {
            dist_code[dist++] = (unsigned char)code;
        }
    }
    assert(dist == 256);
    dist >>= 7; /* from now on, all distances are divided by 128 */
    for ( ; code < D_CODES; code++) {
        base_dist[code] = dist << 7;
        for (n = 0; n < (1<<(extra_dbits[code]-7)); n++) {
            dist_code[256 + dist++] = (unsigned char)code;
        }
    }
    assert(dist == 256);

    /* Construct the codes of the static literal tree */
    for (bits = 0; bits <= MAX_BITS; bits++) bl_count[bits] = 0;
    n = 0;
    while (n <= 143) static_ltree[n++].Len = 8, bl_count[8]++;
    while (n <= 255) static_ltree[n++].Len = 9, bl_count[9]++;
    while (n <= 279) static_ltree[n++].Len = 7, bl_count[7]++;
    while (n <= 287) static_ltree[n++].Len = 8, bl_count[8]++;
    /* Codes 286 and 287 do not exist, but we must include them in the
     * tree construction to get a canonical Huffman tree (longest code
     * all ones)
     */
    gen_codes((ct_data *)static_ltree, L_CODES+1, bl_count);

    /* The static distance tree is trivial: */
    for (n = 0; n < D_CODES; n++) {
        static_dtree[n].Len = 5;
        static_dtree[n].Code = bi_reverse((unsigned int)n, 5);
    }

    printf("/* header created automatically with %s */\n\n", argv[0]);

    printf("static const ct_data static_ltree[L_CODES+2] = {\n");
    for (int i = 0; i < L_CODES+2; i++) {
        printf("{{%3u},{%3u}}%s", static_ltree[i].Code,
               static_ltree[i].Len, SEPARATOR(i, L_CODES+1, 5));
    }

    printf("static const ct_data static_dtree[D_CODES] = {\n");
    for (int i = 0; i < D_CODES; i++) {
        printf("{{%2u},{%2u}}%s", static_dtree[i].Code,
               static_dtree[i].Len, SEPARATOR(i, D_CODES-1, 5));
    }

    printf("const unsigned char ZLIB_INTERNAL _dist_code[DIST_CODE_LEN] = {\n");
    for (int i = 0; i < DIST_CODE_LEN; i++) {
        printf("%2u%s", dist_code[i], SEPARATOR(i, DIST_CODE_LEN-1, 20));
    }

    printf("const unsigned char ZLIB_INTERNAL _length_code[MAX_MATCH-MIN_MATCH+1]= {\n");
    for (int i = 0; i < MAX_MATCH-MIN_MATCH+1; i++) {
        printf("%2u%s", length_code[i], SEPARATOR(i, MAX_MATCH-MIN_MATCH, 20));
    }

    printf("static const int base_length[LENGTH_CODES] = {\n");
    for (int i = 0; i < LENGTH_CODES; i++) {
        printf("%1u%s", base_length[i], SEPARATOR(i, LENGTH_CODES-1, 20));
    }

    printf("static const int base_dist[D_CODES] = {\n");
    for (int i = 0; i < D_CODES; i++) {
        printf("%5u%s", base_dist[i], SEPARATOR(i, D_CODES-1, 10));
    }

    if (ferror(stdout) || fflush(stdout) == EOF) {
        if (!out_path)
            out_path = "<stdout>";
        fprintf(stderr, "%s: %s: %s\n", argv[0], out_path, strerror(errno));
        return 1;
    }

    return 0;
}
