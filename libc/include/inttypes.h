/*
	Glidix Runtime

	Copyright (c) 2014-2017, Madd Games.
	All rights reserved.
	
	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions are met:
	
	* Redistributions of source code must retain the above copyright notice, this
	  list of conditions and the following disclaimer.
	
	* Redistributions in binary form must reproduce the above copyright notice,
	  this list of conditions and the following disclaimer in the documentation
	  and/or other materials provided with the distribution.
	
	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
	AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
	IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
	DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
	FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
	DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
	SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
	CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
	OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
	OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef _INTTYPES_H
#define _INTTYPES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <limits.h>
#include <stddef.h>

#ifndef INTMAX_MAX
#define	INTMAX_MAX	9223372036854775807L
#endif

#ifndef INTMAX_MIN
#define	INTMAX_MIN	-9223372036854775807L
#endif

#ifndef UINTMAX_MAX
#define	UINTMAX_MAX	18446744073709551615UL
#endif

#ifndef UINTMAX_MIN
#define	UINTMAX_MIN	0UL
#endif

#if (! defined(__cplusplus)) || defined(__STDC_FORMAT_MACROS)

#define PRId8         "hhd"
#define PRIi8         "hhi"
#define PRIo8         "hho"
#define PRIu8         "hhu"
#define PRIx8         "hhx"
#define PRIX8         "hhX"

#define PRId16        "hd"
#define PRIi16        "hi"
#define PRIo16        "ho"
#define PRIu16        "hu"
#define PRIx16        "hx"
#define PRIX16        "hX"

#define PRId32        "d"
#define PRIi32        "i"
#define PRIo32        "o"
#define PRIu32        "u"
#define PRIx32        "x"
#define PRIX32        "X"

#define PRId64        "ld"
#define PRIi64        "li"
#define PRIo64        "lo"
#define PRIu64        "lu"
#define PRIx64        "lx"
#define PRIX64        "lX"

#define PRIdLEAST8    PRId8
#define PRIiLEAST8    PRIi8
#define PRIoLEAST8    PRIo8
#define PRIuLEAST8    PRIu8
#define PRIxLEAST8    PRIx8
#define PRIXLEAST8    PRIX8

#define PRIdLEAST16   PRId16
#define PRIiLEAST16   PRIi16
#define PRIoLEAST16   PRIo16
#define PRIuLEAST16   PRIu16
#define PRIxLEAST16   PRIx16
#define PRIXLEAST16   PRIX16

#define PRIdLEAST32   PRId32
#define PRIiLEAST32   PRIi32
#define PRIoLEAST32   PRIo32
#define PRIuLEAST32   PRIu32
#define PRIxLEAST32   PRIx32
#define PRIXLEAST32   PRIX32

#define PRIdLEAST64   PRId64
#define PRIiLEAST64   PRIi64
#define PRIoLEAST64   PRIo64
#define PRIuLEAST64   PRIu64
#define PRIxLEAST64   PRIx64
#define PRIXLEAST64   PRIX64

#define PRIdFAST8     PRId32
#define PRIiFAST8     PRIi32
#define PRIoFAST8     PRIo32
#define PRIuFAST8     PRIu32
#define PRIxFAST8     PRIx32
#define PRIXFAST8     PRIX32

#define PRIdFAST16    PRId32
#define PRIiFAST16    PRIi32
#define PRIoFAST16    PRIo32
#define PRIuFAST16    PRIu32
#define PRIxFAST16    PRIx32
#define PRIXFAST16    PRIX32

#define PRIdFAST32    PRId32
#define PRIiFAST32    PRIi32
#define PRIoFAST32    PRIo32
#define PRIuFAST32    PRIu32
#define PRIxFAST32    PRIx32
#define PRIXFAST32    PRIX32

#define PRIdFAST64    PRId64
#define PRIiFAST64    PRIi64
#define PRIoFAST64    PRIo64
#define PRIuFAST64    PRIu64
#define PRIxFAST64    PRIx64
#define PRIXFAST64    PRIX64

#define PRIdPTR       PRId64
#define PRIiPTR       PRIi64
#define PRIoPTR       PRIo64
#define PRIuPTR       PRIu64
#define PRIxPTR       PRIx64
#define PRIXPTR       PRIX64

#define PRIdMAX       PRId64
#define PRIiMAX       PRIi64
#define PRIoMAX       PRIo64
#define PRIuMAX       PRIu64
#define PRIxMAX       PRIx64
#define PRIXMAX       PRIX64

#define SCNd8         "hhd"
#define SCNi8         "hhi"
#define SCNo8         "hho"
#define SCNu8         "hhu"
#define SCNx8         "hhx"

#define SCNd16        "hd"
#define SCNi16        "hi"
#define SCNo16        "ho"
#define SCNu16        "hu"
#define SCNx16        "hx"

#define SCNd32        "d"
#define SCNi32        "i"
#define SCNo32        "o"
#define SCNu32        "u"
#define SCNx32        "x"

#define SCNd64        "ld"
#define SCNi64        "li"
#define SCNo64        "lo"
#define SCNu64        "lu"
#define SCNx64        "lx"

#define SCNdLEAST8  SCNd8
#define SCNiLEAST8  SCNi8
#define SCNoLEAST8  SCNo8
#define SCNuLEAST8  SCNu8
#define SCNxLEAST8  SCNx8

#define SCNdLEAST16   SCNd16
#define SCNiLEAST16   SCNi16
#define SCNoLEAST16   SCNo16
#define SCNuLEAST16   SCNu16
#define SCNxLEAST16   SCNx16

#define SCNdLEAST32   SCNd32
#define SCNiLEAST32   SCNi32
#define SCNoLEAST32   SCNo32
#define SCNuLEAST32   SCNu32
#define SCNxLEAST32   SCNx32

#define SCNdLEAST64 SCNd64
#define SCNiLEAST64 SCNi64
#define SCNoLEAST64 SCNo64
#define SCNuLEAST64 SCNu64
#define SCNxLEAST64 SCNx64

#define SCNdFAST8     SCNd32
#define SCNiFAST8     SCNi32
#define SCNoFAST8     SCNo32
#define SCNuFAST8     SCNu32
#define SCNxFAST8     SCNx32

#define SCNdFAST16    SCNd32
#define SCNiFAST16    SCNi32
#define SCNoFAST16    SCNo32
#define SCNuFAST16    SCNu32
#define SCNxFAST16    SCNx32

#define SCNdFAST32    SCNd32
#define SCNiFAST32    SCNi32
#define SCNoFAST32    SCNo32
#define SCNuFAST32    SCNu32
#define SCNxFAST32    SCNx32

#define SCNdFAST64  SCNd64
#define SCNiFAST64  SCNi64
#define SCNoFAST64  SCNo64
#define SCNuFAST64  SCNu64
#define SCNxFAST64  SCNx64

#define SCNdPTR       SCNd64
#define SCNiPTR       SCNi64
#define SCNoPTR       SCNo64
#define SCNuPTR       SCNu64
#define SCNxPTR       SCNx64

#define SCNdMAX     SCNd64
#define SCNiMAX     SCNi64
#define SCNoMAX     SCNo64
#define SCNuMAX     SCNu64
#define SCNxMAX     SCNx64

#endif /* C++ */

intmax_t		strtoimax(const char *nptr, char **endptr, int base);
uintmax_t		strtoumax(const char *nptr, char **endptr, int base);

#ifdef __cplusplus
};	/* extern "C" */
#endif

#endif
