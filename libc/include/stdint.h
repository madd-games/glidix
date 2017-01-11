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

#ifndef _STDINT_H
#define _STDINT_H

#define	INT16_C(c)				c
#define	INT32_C(c)				c
#define	INT64_C(c)				c##L
#define	INT8_C(c)				c
#define	INTMAX_C(c)				c##L
#define	UINT16_C(c)				c##U
#define	UINT32_C(c)				c##U
#define	UINT64_C(c)				c##UL
#define	UINT8_C(c)				c##U
#define	UINTMAX_C(c)				c##UL

#define	UINT8_MAX				0xFFU
#define	UINT16_MAX				0xFFFFU
#define	UINT32_MAX				0xFFFFFFFFU
#define	UINT64_MAX				0xFFFFFFFFFFFFFFFFUL

#define	INT8_MAX				0x7F
#define	INT16_MAX				0x7FFF
#define	INT32_MAX				0x7FFFFFFF
#define	INT64_MAX				0x7FFFFFFFFFFFFFFFL

#define	INT8_MIN				-127
#define	INT16_MIN				-32767
#define	INT32_MIN				-2147483647
#define	INT64_MIN				-9223372036854775807L

#define	SIZE_MAX				0xFFFFFFFFFFFFFFFFUL
#define	SSIZE_MAX				0x7FFFFFFFFFFFFFFFL

#define	INT_FAST8_MIN				INT8_MIN
#define	INT_FAST16_MIN				INT16_MIN
#define	INT_FAST32_MIN				INT32_MIN
#define	INT_FAST64_MIN				INT64_MIN

#define	INT_LEAST8_MIN				INT8_MIN
#define	INT_LEAST16_MIN				INT16_MIN
#define	INT_LEAST32_MIN				INT32_MIN
#define	INT_LEAST64_MIN				INT64_MIN

#define	INT_FAST8_MAX				INT8_MAX
#define	INT_FAST16_MAX				INT16_MAX
#define	INT_FAST32_MAX				INT32_MAX
#define	INT_FAST64_MAX				INT64_MAX

#define	INT_LEAST8_MAX				INT8_MAX
#define	INT_LEAST16_MAX				INT16_MAX
#define	INT_LEAST32_MAX				INT32_MAX
#define	INT_LEAST64_MAX				INT64_MAX

#define	INTMAX_MAX				INT64_MAX
#define	INTMAX_MIN				INT64_MIN

#define	UINTMAX_MAX				UINT64_MAX

#define	UINT_FAST8_MAX				UINT8_MAX
#define	UINT_FAST16_MAX				UINT16_MAX
#define	UINT_FAST32_MAX				UINT32_MAX
#define	UINT_FAST64_MAX				UINT64_MAX

#define	UINT_LEAST8_MAX				UINT8_MAX
#define	UINT_LEAST16_MAX			UINT16_MAX
#define	UINT_LEAST32_MAX			UINT32_MAX
#define	UINT_LEAST64_MAX			UINT64_MAX

#define	INTPTR_MIN				INT64_MIN
#define	INTPTR_MAX				INT64_MAX
#define	UINTPTR_MAX				UINT64_MAX

typedef	signed char				int8_t;
typedef	unsigned char				uint8_t;
typedef	signed short				int16_t;
typedef	unsigned short				uint16_t;
typedef	signed int				int32_t;
typedef	unsigned int				uint32_t;

#ifdef __x86_64__
typedef	signed long				int64_t;
typedef	unsigned long				uint64_t;
#else
typedef	signed long long			int64_t;
typedef	unsigned long long			uint64_t;
#endif

typedef	int64_t					intmax_t;
typedef	uint64_t				uintmax_t;
typedef	int64_t					intptr_t;
typedef	uint64_t				uintptr_t;

typedef int8_t					int_least8_t;
typedef uint8_t					uint_least8_t;
typedef int16_t					int_least16_t;
typedef uint16_t				uint_least16_t;
typedef int32_t					int_least32_t;
typedef uint32_t				uint_least32_t;
typedef int64_t					int_least64_t;
typedef uint64_t				uint_least64_t;

typedef int8_t					int_fast8_t;
typedef uint8_t					uint_fast8_t;
typedef int16_t					int_fast16_t;
typedef uint16_t				uint_fast16_t;
typedef int32_t					int_fast32_t;
typedef uint32_t				uint_fast32_t;
typedef int64_t					int_fast64_t;
typedef uint64_t				uint_fast64_t;

#endif
