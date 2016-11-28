/* zutil.c -- target dependent utility functions for the compression library
 * Copyright (C) 1995-2005, 2010, 2011, 2012 Jean-loup Gailly.
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

/* @(#) $Id$ */

#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "zutil.h"
#include "gzguts.h"

ZLIB_INTERNAL
const char * const z_errmsg[10] = {
"need dictionary",     /* Z_NEED_DICT       2  */
"stream end",          /* Z_STREAM_END      1  */
"",                    /* Z_OK              0  */
"file error",          /* Z_ERRNO         (-1) */
"stream error",        /* Z_STREAM_ERROR  (-2) */
"data error",          /* Z_DATA_ERROR    (-3) */
"insufficient memory", /* Z_MEM_ERROR     (-4) */
"buffer error",        /* Z_BUF_ERROR     (-5) */
"incompatible version",/* Z_VERSION_ERROR (-6) */
""};


const char * ZEXPORT zlibVersion(void)
{
    return ZLIB_VERSION;
}

unsigned long ZEXPORT zlibCompileFlags(void)
{
    unsigned long flags;

    flags = 0;
    switch ((int)(sizeof(unsigned int))) {
    case 2:     break;
    case 4:     flags += 1 << 0;        break;
    case 8:     flags += 2 << 0;        break;
    default:    flags += 3 << 0;
    }
    switch ((int)(sizeof(unsigned long))) {
    case 2:     break;
    case 4:     flags += 1 << 2;        break;
    case 8:     flags += 2 << 2;        break;
    default:    flags += 3 << 2;
    }
    switch ((int)(sizeof(void *))) {
    case 2:     break;
    case 4:     flags += 1 << 4;        break;
    case 8:     flags += 2 << 4;        break;
    default:    flags += 3 << 4;
    }
    switch ((int)(sizeof(off_t))) {
    case 2:     break;
    case 4:     flags += 1 << 6;        break;
    case 8:     flags += 2 << 6;        break;
    default:    flags += 3 << 6;
    }
#ifdef ZLIB_DEBUG
    flags += 1 << 8;
#endif
    /* The operating system must have modern and secure printf functions. */
    flags += 0L << 24;
    flags += 0L << 25;
    flags += 0L << 26;
    return flags;
}

#ifdef ZLIB_DEBUG

#  ifndef verbose
#    define verbose 0
#  endif
int ZLIB_INTERNAL z_verbose = verbose;

/* TODO. This should be a const char *. */
void ZLIB_INTERNAL z_error(char *m)
{
    fprintf(stderr, "%s\n", m);
    exit(1);
}
#endif

/* exported to allow conversion of error code to string for compress() and
 * uncompress()
 */
const char * ZEXPORT zError(int err)
{
    return ERR_MSG(err);
}

/* This function is only for outward appearances in case an application looks
   too closely at strm->zalloc. The internal allocator (z_stream_alloc) notices
   if this one is used and uses the system allocator instead. */
ZLIB_INTERNAL void *zcalloc(void *opaque, unsigned int items, unsigned int size)
{
    (void) opaque;
    return calloc((size_t) items, (size_t) size);
}

/* This function is only for outward appearances in case an application looks
   too closely at strm->zfree. The internal deallocator (z_stream_free) notices
   if this one is used and uses the system deallocator instead. */
ZLIB_INTERNAL void zcfree(void *opaque, void *ptr)
{
    (void) opaque;
    free(ptr);
}

ZLIB_INTERNAL void *z_stream_alloc(z_stream *strm, size_t size)
{
    if (size == 0)
        size = 1;
    if (strm->zalloc == (alloc_func)0 || strm->zalloc == zcalloc)
        return malloc(size);
#if UINT_MAX < SIZE_MAX
    /* Don't trust the application's allocation function that has unsigned int
       parameters to handle multiplication-check-and-promotion-to-size_t
       properly. */
    if (UINT_MAX < size)
        return NULL;
#endif
    return strm->zalloc(strm->opaque, (unsigned int) size, 1);
}

ZLIB_INTERNAL void *z_stream_allocarray(z_stream *strm, size_t nmemb, size_t size)
{
    if (size && nmemb && SIZE_MAX / size < nmemb)
        return errno = ENOMEM, (void *) NULL;
    return z_stream_alloc(strm, nmemb * size);
}

ZLIB_INTERNAL void z_stream_free(z_stream *strm, void *ptr)
{
    if (strm->zfree == (free_func)0 || strm->zfree == zcfree) {
        free(ptr);
        return;
    }
    /* Don't trust the application to do null-checks in the deallocator. */
    if (!ptr)
        return;
    strm->zfree(strm->opaque, ptr);
}
