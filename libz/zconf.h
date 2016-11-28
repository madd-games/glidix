/* zconf.h -- configuration of the zlib compression library
 * Copyright (C) 1995-2013 Jean-loup Gailly.
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

/* @(#) $Id$ */

#ifndef ZCONF_H
#define ZCONF_H

#include <sys/types.h>

#include <limits.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

/* Transition the public interface to const if requested by the application.
   libz itself is always built with const. */
#if (defined(ZLIB_CONST) || defined(Z_INSIDE_LIBZ)) && !defined(z_const)
#  define z_const const
#else
#  define z_const
#endif

/* Maximum value for memLevel in deflateInit2 */
#define MAX_MEM_LEVEL 9

/* Maximum value for windowBits in deflateInit2 and inflateInit2. */
#define MAX_WBITS   15 /* 32K LZ77 window */

/* The memory requirements for deflate are (in bytes):
            (1 << (windowBits+2)) +  (1 << (memLevel+9))
 that is: 128K for windowBits=15  +  128K for memLevel = 8  (default values)
 plus a few kilobytes for small objects. For example, if you want to reduce
 the default memory requirements from 256K to 128K, define MAX_WBITS to 14 and
 define MAX_MEM_LEVEL to 7. Of course this will generally degrade compression
 (there's no free lunch).

   The memory requirements for inflate are (in bytes) 1 << windowBits
 that is, 32K for windowBits=15 (default value) plus a few kilobytes
 for small objects.
*/

                        /* Type declarations */

#define ZEXTERN extern
#define ZEXPORT
#define ZEXPORTVA

typedef uint32_t z_crc_t;

/* Large File Support brain damage support, kept only because important
   contemporary Unix systems buy into this snake oil and to preserve
   compatibility with zlib. Detect if this is the case. */
#if defined(_LFS64_LARGEFILE) && _LFS64_LARGEFILE-0
#  define Z_LFS64
#  if LONG_MAX <= 0x7FFFFFFF /* 32-bit LFS-capable system */
#    if defined(_FILE_OFFSET_BITS) && _FILE_OFFSET_BITS-0 == 64
#      define Z_WANT64 /* Replace with *64 variants. */
#    endif
#  endif
#  if defined(_LARGEFILE64_SOURCE)
#    define Z_LARGE64 /* Provide *64 variants */
#  endif
#endif

/* Internally used default off_t size. */
#ifdef Z_INSIDE_LIBZ
#  if defined(Z_LFS64) && LONG_MAX <= 0x7FFFFFFF /* 32-bit LFS-capable system */
typedef long z_default_off_t;
#  else /* System without LFS. */
typedef off_t z_default_off_t;
#  endif
#endif

/* These declarations were traditionally used in the public API and inside the
   library itself. This is no longer the case as they have been replaced by the
   real types they obviously wrap. Programs still rely on these types being
   provided by this header, however, so we continue to provide them unless the
   program request they are omitted. #define Z_OMIT_JUNK if you don't want them.
   Z_INSIDE_LIBZ is an internal macro, don't define it. */
#if !(defined(Z_OMIT_JUNK) || defined(Z_INSIDE_LIBZ))
#  define Z_OMITTED_JUNK 1
typedef unsigned char  Byte;
typedef unsigned int   uInt;
typedef unsigned long  uLong;
typedef Byte           Bytef;
typedef char           charf;
typedef int            intf;
typedef uInt           uIntf;
typedef uLong          uLongf;
typedef const void    *voidpc;
typedef void          *voidpf;
typedef void          *voidp;
#  define z_off_t        off_t
#  if defined(Z_LFS64) && LONG_MAX <= 0x7FFFFFFF /* 32-bit LFS-capable system */
#    define z_off64_t      int64_t
#  else /* System without LFS. */
#     define z_off_t        off_t
#  endif
#endif

#endif /* ZCONF_H */
