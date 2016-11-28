/* genfixed.c -- table for decoding fixed codes
 * Copyright (C) 1995-2012 Mark Adler
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <zlib.h>

#include "zutil.h"
#include "inftrees.h"
#include "inflate.h"

static void fixedtables(struct inflate_state *state)
{
    static code *lenfix, *distfix;
    static code fixed[544];

    unsigned int sym, bits;
    static code *next;

    /* literal/length table */
    sym = 0;
    while (sym < 144) state->lens[sym++] = 8;
    while (sym < 256) state->lens[sym++] = 9;
    while (sym < 280) state->lens[sym++] = 7;
    while (sym < 288) state->lens[sym++] = 8;
    next = fixed;
    lenfix = next;
    bits = 9;
    inflate_table(LENS, state->lens, 288, &next, &bits, state->work);

    /* distance table */
    sym = 0;
    while (sym < 32) state->lens[sym++] = 5;
    distfix = next;
    bits = 5;
    inflate_table(DISTS, state->lens, 32, &next, &bits, state->work);

    state->lencode = lenfix;
    state->lenbits = 9;
    state->distcode = distfix;
    state->distbits = 5;
}

int main(int argc, char *argv[])
{
    const char *out_path = NULL;
    if (2 <= argc) {
        if (!(freopen(out_path = argv[1], "w", stdout))) {
            fprintf(stderr, "%s: %s: %s\n", argv[0], out_path, strerror(errno));
            return 1;
        }
    }

    unsigned int low, size;
    struct inflate_state state;

    fixedtables(&state);
    if (out_path)
        printf("    /* %s -- table for decoding fixed codes\n", out_path);
    else
        printf("    /* table for decoding fixed codes\n");
    printf("     * Generated automatically by %s.\n", argv[0]);
    printf("     */\n");
    printf("\n");
    printf("    /* WARNING: this file should *not* be used by applications.\n");
    printf("       It is part of the implementation of this library and is\n");
    printf("       subject to change. Applications should only use zlib.h.\n");
    printf("     */\n");
    printf("\n");
    size = 1U << 9;
    printf("    static const code lenfix[%u] = {", size);
    low = 0;
    for (;;) {
        if ((low % 7) == 0)
              printf("\n        ");
        printf("{%u,%u,%d}", (low & 127) == 99 ? 64 : state.lencode[low].op,
               state.lencode[low].bits, state.lencode[low].val);
        if (++low == size)
            break;
        putchar(',');
    }
    printf("\n    };\n");
    size = 1U << 5;
    printf("\n    static const code distfix[%u] = {", size);
    low = 0;
    for (;;) {
        if ((low % 6) == 0)
            printf("\n        ");
        printf("{%u,%u,%d}", state.distcode[low].op, state.distcode[low].bits,
               state.distcode[low].val);
        if (++low == size)
            break;
        putchar(',');
    }
    printf("\n    };\n");

    if (ferror(stdout) || fflush(stdout) == EOF) {
        if (!out_path)
            out_path = "<stdout>";
        fprintf(stderr, "%s: %s: %s\n", argv[0], out_path, strerror(errno));
        return 1;
    }

    return 0;
}
