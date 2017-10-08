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

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <ctype.h>

struct __instr
{
	const char *_s;
	FILE *_fp;
	int _maxget;
	int _count;
};

static void __instr_inits(struct __instr *_is, const char *_s)
{
	_is->_s = _s;
	_is->_fp = NULL;
	_is->_maxget = -1;
	_is->_count = 0;
};

static void __instr_initf(struct __instr *_is, FILE *_fp)
{
	_is->_fp = _fp;
	_is->_maxget = -1;
	_is->_count = 0;
};

static int __instr_getc(struct __instr *_is)
{
	if (_is->_maxget == 0)
	{
		return EOF;
	}
	else if (_is->_maxget != -1)
	{
		_is->_maxget--;
	};

	if (_is->_fp != NULL)
	{
		int ret = fgetc(_is->_fp);
		if (ret != EOF) _is->_count++;
		return ret;
	}
	else
	{
		if (*_is->_s == 0) return EOF;
		else
		{
			_is->_count++;
			return *_is->_s++;
		};
	};
};

static void __instr_ungetc(struct __instr *_is, int prev)
{
	if (prev == EOF) return;
	
	_is->_count--;
	if (_is->_fp != NULL)
	{
		ungetc(prev, _is->_fp);
	}
	else
	{
		_is->_s--;
	};

	if (_is->_maxget != -1)
	{
		_is->_maxget++;
	};
};

#define	_INTCONVFUNC(_TYPE, _FUNCNAME, _LIMIT) \
int _FUNCNAME(struct __instr *_is, int base, _TYPE *outp)\
{\
	int c;\
	while (isspace((c = __instr_getc(_is))));\
	if (c == EOF) return -1;\
	else __instr_ungetc(_is, c);\
\
	int negative = 0;\
	c = __instr_getc(_is);\
	if (c == '+')\
	{}\
	else if (c == '-')\
	{\
		negative = 1;\
	}\
	else\
	{\
		__instr_ungetc(_is, c);\
	};\
\
	_TYPE out = 0;\
	if (base == 0)\
	{\
		c = __instr_getc(_is);\
		if (c == '0')\
		{\
			base = 8;\
\
			c = __instr_getc(_is);\
			if (c == 'x')\
			{\
				base = 16;\
			}\
			else\
			{\
				__instr_ungetc(_is, c);\
			};\
		}\
		else\
		{\
			base = 10;\
			__instr_ungetc(_is, c);\
		};\
	};\
\
	_TYPE llbase = (_TYPE) base;\
	int matchedOne = 0;\
	while (1)\
	{\
		c = __instr_getc(_is);\
		if (c == EOF)\
		{\
			goto finished;\
		};\
\
		if (((c < '0') || (c > '9')) && ((c < 'a') || (c > 'z')) && ((c < 'A') || (c > 'Z')))\
		{\
			__instr_ungetc(_is, c);\
			goto finished;\
		};\
\
		_TYPE digit = 0;\
		if ((c >= '0') && (c <= '9'))\
		{\
			digit = c - '0';\
		}\
		else if ((c >= 'a') && (c <= 'z'))\
		{\
			digit = 10 + c - 'a';\
		}\
		else if ((c >= 'A') && (c <= 'Z'))\
		{\
			digit = 10 + c - 'A';\
		}\
		else\
		{\
			__instr_ungetc(_is, c);\
			goto finished;\
		};\
\
		if (digit >= llbase)\
		{\
			__instr_ungetc(_is, c);\
			goto finished;\
		};\
\
		if (out > ((_LIMIT-digit)/llbase))\
		{\
			out = _LIMIT;\
			__instr_ungetc(_is, c);\
			goto finished;\
		};\
\
		matchedOne = 1;\
		out = out * llbase + digit;\
	};\
\
	finished:\
	if (negative) out = -out;\
	if (outp != NULL) *outp = out;\
	return !matchedOne;\
}

#define	_UINTCONVFUNC(_TYPE, _FUNCNAME, _LIMIT) \
int _FUNCNAME(struct __instr *_is, int base, _TYPE *outp)\
{\
	int c;\
	while (isspace((c = __instr_getc(_is))));\
	if (c == EOF) return -1;\
	else __instr_ungetc(_is, c);\
\
	_TYPE out = 0;\
	if (base == 0)\
	{\
		c = __instr_getc(_is);\
		if (c == '0')\
		{\
			base = 8;\
\
			c = __instr_getc(_is);\
			if (c == 'x')\
			{\
				base = 16;\
			}\
			else\
			{\
				__instr_ungetc(_is, c);\
			};\
		}\
		else\
		{\
			base = 10;\
			__instr_ungetc(_is, c);\
		};\
	};\
\
	_TYPE llbase = (_TYPE) base;\
	int matchedOne = 0;\
	while (1)\
	{\
		c = __instr_getc(_is);\
		if (c == EOF)\
		{\
			goto finished;\
		};\
\
		if (((c < '0') || (c > '9')) && ((c < 'a') || (c > 'z')) && ((c < 'A') || (c > 'Z')))\
		{\
			__instr_ungetc(_is, c);\
			goto finished;\
		};\
\
		_TYPE digit = 0;\
		if ((c >= '0') && (c <= '9'))\
		{\
			digit = c - '0';\
		}\
		else if ((c >= 'a') && (c <= 'z'))\
		{\
			digit = 10 + c - 'a';\
		}\
		else if ((c >= 'A') && (c <= 'Z'))\
		{\
			digit = 10 + c - 'A';\
		}\
		else\
		{\
			__instr_ungetc(_is, c);\
			goto finished;\
		};\
\
		if (digit >= llbase)\
		{\
			__instr_ungetc(_is, c);\
			goto finished;\
		};\
\
		if (out > ((_LIMIT-digit)/llbase))\
		{\
			out = _LIMIT;\
			__instr_ungetc(_is, c);\
			goto finished;\
		};\
\
		matchedOne = 1;\
		out = out * llbase + digit;\
	};\
\
	finished:\
	if (outp != NULL) *outp = out;\
	return !matchedOne;\
}

_INTCONVFUNC(int, __scanf_conv_noned, 2147483647);
_INTCONVFUNC(char, __scanf_conv_hhd, 127);
_INTCONVFUNC(short, __scanf_conv_hd, 32767);
_INTCONVFUNC(long, __scanf_conv_ld, 9223372036854775807L);
_INTCONVFUNC(long long, __scanf_conv_lld, 9223372036854775807L);
_INTCONVFUNC(intmax_t, __scanf_conv_jd, 9223372036854775807L);
_INTCONVFUNC(int64_t, __scanf_conv_64d, 9223372036854775807L);
_INTCONVFUNC(ptrdiff_t, __scanf_conv_td, 0x7FFFFFFFFFFFFFFF);

_UINTCONVFUNC(unsigned, __scanf_conv_noneu, 4294967295U);
_UINTCONVFUNC(unsigned char, __scanf_conv_hhu, 255);
_UINTCONVFUNC(unsigned short, __scanf_conv_hu, 65535U);
_UINTCONVFUNC(unsigned long, __scanf_conv_lu, 18446744073709551615UL);
_UINTCONVFUNC(unsigned long long, __scanf_conv_llu, 18446744073709551615UL);
_UINTCONVFUNC(uintmax_t, __scanf_conv_ju, 18446744073709551615UL);
_UINTCONVFUNC(uint64_t, __scanf_conv_64u, 18446744073709551615UL);
_UINTCONVFUNC(uint64_t, __scanf_conv_tu, 0xFFFFFFFFFFFFFFFF);

/* length modifiers */
enum
{
	_LM_NONE,
	_LM_hh,
	_LM_h,
	_LM_l,
	_LM_ll,
	_LM_j,
	_LM_z,
	_LM_t,
	_LM_L,
};

int __scanf_conv_sint(struct __instr *_is, int lenmod, int base, void *outp)
{
	switch (lenmod)
	{
	case _LM_NONE:
		return __scanf_conv_noned(_is, base, (int*)outp);
	case _LM_hh:
		return __scanf_conv_hhd(_is, base, (char*)outp);
	case _LM_h:
		return __scanf_conv_hd(_is, base, (short*)outp);
	case _LM_l:
		return __scanf_conv_ld(_is, base, (long*)outp);
	case _LM_ll:
		return __scanf_conv_lld(_is, base, (long long*)outp);
	case _LM_j:
		return __scanf_conv_jd(_is, base, (intmax_t*)outp);
	case _LM_z:
		return __scanf_conv_64d(_is, base, (int64_t*)outp);
	case _LM_t:
		return __scanf_conv_td(_is, base, (ptrdiff_t*)outp);
	default:
		return -1;
	};
};

int __scanf_conv_uint(struct __instr *_is, int lenmod, int base, void *outp)
{
	switch (lenmod)
	{
	case _LM_NONE:
		return __scanf_conv_noneu(_is, base, (unsigned*)outp);
	case _LM_hh:
		return __scanf_conv_hhu(_is, base, (unsigned char*)outp);
	case _LM_h:
		return __scanf_conv_hu(_is, base, (unsigned short*)outp);
	case _LM_l:
		return __scanf_conv_lu(_is, base, (unsigned long*)outp);
	case _LM_ll:
		return __scanf_conv_llu(_is, base, (unsigned long long*)outp);
	case _LM_j:
		return __scanf_conv_ju(_is, base, (uintmax_t*)outp);
	case _LM_z:
		return __scanf_conv_64u(_is, base, (uint64_t*)outp);
	case _LM_t:
		return __scanf_conv_tu(_is, base, (uint64_t*)outp);
	default:
		return -1;
	};
};

int __scanf_conv_str(struct __instr *_is, char *out)
{
	while (1)
	{
		int c = __instr_getc(_is);
		if (c == EOF)
		{
			break;
		};

		if (isspace(c))
		{
			__instr_ungetc(_is, c);
			break;
		};

		if (out != NULL) *out++ = c;
	};

	if (out != NULL) *out = 0;
	return 0;
};

int __scanf_conv_chars(struct __instr *_is, int count, char *outp)
{
	while (count--)
	{
		int c = __instr_getc(_is);
		if (c == EOF) return -1;
		if (outp != NULL) *outp++ = c;
	};

	return 0;
};

int __scanf_conv_float(struct __instr *_is, int lenmod, void *outp)
{
	uint64_t result = 0;
	int sign = 1;
	uint64_t div = 1;
	int scannedYet = 0;
	int pointSeen = 0;
	
	int c = __instr_getc(_is);
	if (c == EOF)
	{
		return -1;
	};
	
	if (c == '+')
	{
		sign = 1;
	}
	else if (c == '-')
	{
		sign = -1;
	}
	else
	{
		__instr_ungetc(_is, c);
	};
	
	while (1)
	{
		c = __instr_getc(_is);
		if (c == EOF)
		{
			break;
		};
		
		if (c == '.')
		{
			if (pointSeen)
			{
				__instr_ungetc(_is, c);
				break;
			};
			
			pointSeen = 1;
			continue;
		};
		
		if ((c < '0') || (c > '9'))
		{
			__instr_ungetc(_is, c);
			break;
		};
		
		result = (result*10) + (c-'0');
		if (pointSeen) div *= 10;
		scannedYet = 1;
	};

	if (!scannedYet)
	{
		return -1;
	};

	c = __instr_getc(_is);
	if (c == EOF)
	{
		// NOP
	}
	else if (c == 'e')
	{
		char expsign = __instr_getc(_is);
		if (expsign == '-' || expsign == '+')
		{
			int exp = 0;
			
			while (1)
			{
				c = __instr_getc(_is);
				if (c < '0' || c > '9')
				{
					__instr_ungetc(_is, c);
					break;
				};
				
				exp = exp * 10 + (c - '0');
			};
			
			if (expsign == '+')
			{
				while (exp--) result *= 10;
			}
			else
			{
				while (exp--)
				{
					if (result % 10 == 0)
					{
						result /= 10;
					}
					else
					{
						div *= 10;
					};
				};
			};
		};
	}
	else
	{
		__instr_ungetc(_is, c);
	};
	
	if (outp != NULL)
	{
		switch (lenmod)
		{
		case _LM_NONE:
			*((float*)outp) = (float) sign * (float) result / (float) div;
			break;
		case _LM_l:
			*((double*)outp) = (double) sign * (double) result / (double) div;
			break;
		case _LM_L:
			*((long double*)outp) = (long double) sign * (long double) result / (long double) div;
			break;
		default:
			return -1;
		};
	};
	
	return 0;
};

int __scanf_conv_n(struct __instr *_is, int lenmod, int count, void *outp)
{
	switch (lenmod)
	{
	case _LM_NONE:
		*((unsigned*)outp) = count;
		break;
	case _LM_hh:
		*((unsigned char*)outp) = count;
		break;
	case _LM_h:
		*((unsigned short*)outp) = count;
		break;
	case _LM_l:
		*((unsigned long*)outp) = count;
		break;
	case _LM_ll:
		*((unsigned long long*)outp) = count;
		break;
	case _LM_j:
		*((uintmax_t*)outp) = count;
		break;
	case _LM_z:
		*((uint64_t*)outp) = count;
		break;
	case _LM_t:
		*((uint64_t*)outp) = count;
		break;
	default:
		return -1;
	};

	return 0;
};

int __scanf_gen(struct __instr *_is, const char *format, va_list ap)
{
	int suppress=0, maxwidth=-1, lenmod=_LM_NONE, matchcount=0, c;

	while (*format != 0)
	{
		if (isspace(*format))
		{
			while (1)
			{
				c = __instr_getc(_is);
				if (c == EOF) goto finish;
				if (!isspace(c))
				{
					__instr_ungetc(_is, c);
					break;
				};
			};
			
			format++;
		}
		else if (*format != '%')
		{
			c = __instr_getc(_is);
			if (c == EOF)
			{
				goto finish;
			};

			if ((char)c != *format)
			{
				__instr_ungetc(_is, c);
				goto finish;
			};
			
			format++;
		}
		else
		{
			format++;
			if (*format == '%')
			{
				c = __instr_getc(_is);
				if (c == EOF)
				{
					goto finish;
				};
				
				if (c != '%')
				{
					__instr_ungetc(_is, c);
					goto finish;
				};
				
				format++;
				continue;
			};
			
			suppress = 0;
			maxwidth = -1;
			lenmod = _LM_NONE;

			if (*format == '*')
			{
				suppress = 1;
				format++;
			};

			char *endptr;
			maxwidth = (int) strtol(format, (char**)&endptr, 10);
			if (endptr == format)
			{
				maxwidth = -1;
			}
			else
			{
				format = endptr;
			};
			
			// read optional length modifier if any.
			switch (*format)
			{
			case 'h':
				format++;
				if ((*format) == 'h')
				{
					lenmod = _LM_hh;
					format++;
				}
				else
				{
					lenmod = _LM_h;
				};
				break;
			case 'l':
				format++;
				if ((*format) == 'l')
				{
					lenmod = _LM_ll;
					format++;
				}
				else
				{
					lenmod = _LM_l;
				};
				break;
			case 'j':
				format++;
				lenmod = _LM_j;
				break;
			case 'z':
				format++;
				lenmod = _LM_z;
				break;
			case 't':
				format++;
				lenmod = _LM_t;
				break;
			case 'L':
				format++;
				lenmod = _LM_L;
				break;
			};

			_is->_maxget = maxwidth;

			void *outp = NULL;
			if (!suppress)
			{
				outp = va_arg(ap, void*);
			};

			// read the convertion specifier
			char convspec = *format++;
			switch (convspec)
			{
			case 'd':
				if (__scanf_conv_sint(_is, lenmod, 10, outp) != 0) goto finish;
				break;
			case 'i':
				if (__scanf_conv_sint(_is, lenmod, 0, outp) != 0) goto finish;
				break;
			case 'o':
				if (__scanf_conv_uint(_is, lenmod, 8, outp) != 0) goto finish;
				break;
			case 'u':
				if (__scanf_conv_uint(_is, lenmod, 10, outp) != 0) goto finish;
				break;
			case 'x':
				if (__scanf_conv_uint(_is, lenmod, 16, outp) != 0) goto finish;
				break;
			case 'a':
			case 'e':
			case 'f':
			case 'g':
			case 'A':
			case 'E':
			case 'F':
			case 'G':
				if (__scanf_conv_float(_is, lenmod, outp) != 0) goto finish;
				break;
			case 'S':
			case 's':
				__scanf_conv_str(_is, (char*)outp);
				break;
			case 'C':
			case 'c':
				if (maxwidth == 0) maxwidth = 1;
				if (__scanf_conv_chars(_is, maxwidth, (char*)outp) != 0) goto finish;
				break;
			case 'n':
				__scanf_conv_n(_is, lenmod, _is->_count, outp);
				break;
			default:
				goto finish;
			};

			matchcount++;
			_is->_maxget = -1;
		};
	};

	finish:
	return matchcount;
};

int vfscanf(FILE *stream, const char *format, va_list arg)
{
	struct __instr _is;
	__instr_initf(&_is, stream);
	return __scanf_gen(&_is, format, arg);
};

int vscanf(const char *format, va_list arg)
{
	return vfscanf(stdin, format, arg);
};

int vsscanf(const char *s, const char *format, va_list arg)
{
	struct __instr _is;
	__instr_inits(&_is, s);
	return __scanf_gen(&_is, format, arg);
};

int fscanf(FILE *stream, const char *format, ...)
{
	va_list ap;
	va_start(ap, format);
	int out = vfscanf(stream, format, ap);
	va_end(ap);
	return out;
};

int scanf(const char *format, ...)
{
	va_list ap;
	va_start(ap, format);
	int out = vscanf(format, ap);
	va_end(ap);
	return out;
};

int sscanf(const char *s, const char *format, ...)
{
	va_list ap;
	va_start(ap, format);
	int out = vsscanf(s, format, ap);
	va_end(ap);
	return out;
};
