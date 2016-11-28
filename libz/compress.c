/* compress.c -- compress a memory buffer
 * Copyright (C) 1995-2005, 2014 Jean-loup Gailly, Mark Adler
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

/* @(#) $Id$ */

#include <limits.h>

#include "zlib.h"

/* ===========================================================================
     Compresses the source buffer into the destination buffer. The level
   parameter has the same meaning as in deflateInit.  sourceLen is the byte
   length of the source buffer. Upon entry, destLen is the total size of the
   destination buffer, which must be at least 0.1% larger than sourceLen plus
   12 bytes. Upon exit, destLen is the actual size of the compressed buffer.

     compress2 returns Z_OK if success, Z_MEM_ERROR if there was not enough
   memory, Z_BUF_ERROR if there was not enough room in the output buffer,
   Z_STREAM_ERROR if the level parameter is invalid.
*/
int ZEXPORT compress2(unsigned char *dest,
                      unsigned long *destLenPtr,
                      const unsigned char *source,
                      unsigned long sourceLen,
                      int level)
{
    z_stream stream;
    int err;
    unsigned long destLen = *destLenPtr;

    stream.zalloc = (alloc_func)0;
    stream.zfree = (free_func)0;
    stream.opaque = (void *)0;

    err = deflateInit(&stream, level);
    if (err != Z_OK) return err;

    stream.next_in = source;
    stream.next_out = dest;

    do {
        unsigned long avail_in = sourceLen - stream.total_in;
        unsigned long avail_out = destLen - stream.total_out;
        int flush;

        stream.avail_in = avail_in < UINT_MAX ? avail_in : UINT_MAX;
        stream.avail_out = avail_out < UINT_MAX ? avail_out : UINT_MAX;
        flush = stream.avail_in == avail_in ? Z_FINISH : Z_NO_FLUSH;
        err = deflate(&stream, flush);
    } while (err == Z_OK);

    *destLenPtr = stream.total_out;
    deflateEnd(&stream);
    return err == Z_STREAM_END ? Z_OK : err;
}

/* ===========================================================================
 */
int ZEXPORT compress(unsigned char *dest,
                     unsigned long *destLen,
                     const unsigned char *source,
                     unsigned long sourceLen)
{
    return compress2(dest, destLen, source, sourceLen, Z_DEFAULT_COMPRESSION);
}

/* ========================================================================= */
static unsigned long saturateAddBound(unsigned long a, unsigned long b)
{
    return ULONG_MAX - a < b ? ULONG_MAX : a + b;
}

/* ===========================================================================
     If the default memLevel or windowBits for deflateInit() is changed, then
   this function needs to be updated.
 */
unsigned long ZEXPORT compressBound(unsigned long sourceLen)
{
    unsigned long complen = sourceLen;
    complen = saturateAddBound(complen, sourceLen >> 12);
    complen = saturateAddBound(complen, sourceLen >> 14);
    complen = saturateAddBound(complen, sourceLen >> 25);
    complen = saturateAddBound(complen, 13);
    return complen;
}
