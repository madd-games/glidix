/* uncompr.c -- decompress a memory buffer
 * Copyright (C) 1995-2003, 2010, 2014 Jean-loup Gailly, Mark Adler
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

/* @(#) $Id$ */

#include <limits.h>

#include "zlib.h"

/* ===========================================================================
     Decompresses the source buffer into the destination buffer.  sourceLen is
   the byte length of the source buffer. Upon entry, destLen is the total
   size of the destination buffer, which must be large enough to hold the
   entire uncompressed data. (The size of the uncompressed data must have
   been saved previously by the compressor and transmitted to the decompressor
   by some mechanism outside the scope of this compression library.)
   Upon exit, destLen is the actual size of the compressed buffer.

     uncompress returns Z_OK if success, Z_MEM_ERROR if there was not
   enough memory, Z_BUF_ERROR if there was not enough room in the output
   buffer, or Z_DATA_ERROR if the input data was corrupted, including if the
   input data is an incomplete zlib stream.
*/
int ZEXPORT uncompress(unsigned char *dest,
                       unsigned long *destLenPtr,
                       const unsigned char *source,
                       unsigned long sourceLen)
{
    z_stream stream;
    int err;
    unsigned long destLen = *destLenPtr;
    /* for detection of incomplete stream when *destLen == 0 */
    unsigned char buf[1];

    if (destLen == 0) {
        dest = buf;
        destLen = 1;
    }

    stream.zalloc = (alloc_func)0;
    stream.zfree = (free_func)0;
    stream.opaque = (void *)0;

    err = inflateInit(&stream);
    if (err != Z_OK) return err;

    stream.next_in = source;
    stream.next_out = dest;

    do {
        unsigned long avail_in = sourceLen - stream.total_in;
        unsigned long avail_out = destLen - stream.total_out;

        stream.avail_in = avail_in < UINT_MAX ? avail_in : UINT_MAX;
        stream.avail_out = avail_out < UINT_MAX ? avail_out : UINT_MAX;
        err = inflate(&stream, Z_NO_FLUSH);
    } while (err == Z_OK);

    destLen -= stream.total_out; /* Remaining space for output */
    if (dest != buf)
        *destLenPtr = stream.total_out;
    else if (stream.total_out && err == Z_BUF_ERROR)
        destLen = 1;

    inflateEnd(&stream);
    return err == Z_STREAM_END ? Z_OK :
           err == Z_NEED_DICT ? Z_DATA_ERROR  :
           err == Z_BUF_ERROR && destLen ? Z_DATA_ERROR :
           err;
}
