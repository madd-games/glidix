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
#include <inttypes.h>
#include <stddef.h>
#include <string.h>

/**
 * Represents parsed flags from a conversion specification.
 */
#define	FLAG_LEFTADJ			(1 << 0)			/* - */
#define	FLAG_SIGN			(1 << 1)			/* + */
#define	FLAG_SPACE			(1 << 2)			/* <space> */
#define	FLAG_ALT			(1 << 3)			/* # */
#define	FLAG_ZEROPAD			(1 << 4)			/* 0 */

/**
 * Parsed length modifiers.
 */
enum
{
	_LM_NONE,
	_LM_hh,
	_LM_h,
	_LM_l,
	_LM_ll,
	_LM_L,
	_LM_j,
	_LM_z,
	_LM_t,
};

int fputc(int ch, FILE *fp)
{
	char c = (char) ch;
	return fwrite(&c, 1, 1, fp);
};

int putc(int c, FILE *fp)
{
	return fputc(c, fp);
};

int putchar(int ch)
{
	return fputc(ch, stdout);
};

static int __parse_flag(const char **fmtptr, int *flagptr)
{
	switch (**fmtptr)
	{
	case '-':
		(*flagptr) |= FLAG_LEFTADJ;
		(*fmtptr)++;
		return 1;
	case '+':
		(*flagptr) |= FLAG_SIGN;
		(*fmtptr)++;
		return 1;
	case ' ':
		(*flagptr) |= FLAG_SPACE;
		(*fmtptr)++;
		return 1;
	case '#':
		(*flagptr) |= FLAG_ALT;
		(*fmtptr)++;
		return 1;
	case '0':
		(*flagptr) |= FLAG_ZEROPAD;
		(*fmtptr)++;
		return 1;
	default:
		return 0;
	};
};

static int __printf_putfield(FILE *fp, const char *data, size_t len, int flags, int fieldWidth)
{
	if ((fieldWidth == -1) || (len > fieldWidth))
	{
		int ret = fwrite(data, 1, len, fp);
		return ret;
	};
	
	if (flags & FLAG_LEFTADJ)
	{
		int ret = fwrite(data, 1, len, fp);
		size_t toPad = fieldWidth - len;
		
		while (toPad--)
		{
			fputc(' ', fp);
			ret++;
		};
		
		return ret;
	}
	else
	{
		size_t toPad = fieldWidth - len;
		int ret = 0;
		
		while (toPad--)
		{
			fputc(' ', fp);
			ret++;
		};

		ret += fwrite(data, 1, len, fp);
		return ret;
	};
};

/**
 * The "firstLetter" argument is the letter used to represent 10 if base > 10.
 */
static int __printf_conv_signed(FILE *fp, int flags, int lenmod, int fieldWidth, int precision, int base, char firstLetter, va_list ap)
{
	char convbuf_stack[64];
	char *convbuf = convbuf_stack;
	size_t bufsize = 64;
	if (precision > 60)
	{
		convbuf = (char*) malloc(precision+4);
		bufsize = precision + 4;
	};
	
	memset(convbuf, 0, bufsize);
	
	int64_t num = 0;
	switch (lenmod)
	{
	case _LM_NONE:
		num = (int64_t) va_arg(ap, int);
		break;
	case _LM_hh:
		num = (int64_t) va_arg(ap, int);	// char
		break;
	case _LM_h:
		num = (int64_t) va_arg(ap, int);	// short
		break;
	case _LM_l:
		num = (int64_t) va_arg(ap, long);
		break;
	case _LM_ll:
		num = (int64_t) va_arg(ap, long long);
		break;
	case _LM_j:
		num = (int64_t) va_arg(ap, intmax_t);
		break;
	case _LM_z:
		num = (int64_t) va_arg(ap, ssize_t);
		break;
	case _LM_t:
		num = (int64_t) va_arg(ap, ptrdiff_t);
		break;
	default:
		// standard says it's ok to have undefined behavior here
		num = (int64_t) va_arg(ap, int);
		break;
	};
	
	char sign = 0;
	if (num < 0)
	{
		sign = '-';
		num = -num;
	};
	
	if (precision == -1)
	{
		// "The default precision is 1"
		precision = 1;
	};
	
	char *put = convbuf + bufsize - 1;
	while (num != 0)
	{
		int64_t digit = num % base;
		num /= base;
		
		char c;
		if (digit < 10)
		{
			c = (char) digit + '0';
		}
		else
		{
			c = (char) digit - 10 + firstLetter;
		};
		
		*(--put) = c;
	};
	
	if (sign == 0)
	{
		if (flags & FLAG_SIGN)
		{
			sign = '+';
		}
		else if (flags & FLAG_SPACE)
		{
			sign = ' ';
		};
	};
	
	int zerofill = 0;
	if (strlen(put) < precision)
	{
		zerofill = precision - strlen(put);
	};
	
	if ((flags & FLAG_ZEROPAD) && ((flags & FLAG_LEFTADJ) == 0))
	{
		if ((zerofill+strlen(put)) < fieldWidth)
		{
			zerofill = fieldWidth - strlen(put);
		};
	};
	
	if (sign != 0)
	{
		zerofill--;
	};
	
	if (zerofill < 0) zerofill = 0;
	while (zerofill--)
	{
		(*--put) = '0';
	};
	
	if (sign != 0)
	{
		*(--put) = sign;
	};
	
	int ret = __printf_putfield(fp, put, strlen(put), flags, fieldWidth);
	if (precision > 60)
	{
		free(convbuf);
	};
	
	return ret;
};

static int __printf_conv_unsigned(FILE *fp, int flags, int lenmod, int fieldWidth, int precision, int base, char firstLetter, va_list ap)
{
	char convbuf_stack[64];
	char *convbuf = convbuf_stack;
	size_t bufsize = 64;
	if (precision > 60)
	{
		convbuf = (char*) malloc(precision+4);
		bufsize = precision + 4;
	};
	
	memset(convbuf, 0, bufsize);
	
	uint64_t num = 0;
	switch (lenmod)
	{
	case _LM_NONE:
		num = (uint64_t) va_arg(ap, unsigned int);
		break;
	case _LM_hh:
		num = (uint64_t) va_arg(ap, unsigned int);	// char
		break;
	case _LM_h:
		num = (uint64_t) va_arg(ap, unsigned int);	// short
		break;
	case _LM_l:
		num = (uint64_t) va_arg(ap, unsigned long);
		break;
	case _LM_ll:
		num = (uint64_t) va_arg(ap, unsigned long long);
		break;
	case _LM_j:
		num = (uint64_t) va_arg(ap, uintmax_t);
		break;
	case _LM_z:
		num = (uint64_t) va_arg(ap, size_t);
		break;
	case _LM_t:
		num = (uint64_t) va_arg(ap, uint64_t);
		break;
	default:
		// standard says it's ok to have undefined behavior here
		num = (uint64_t) va_arg(ap, unsigned int);
		break;
	};
	
	if (precision == -1)
	{
		// "The default precision is 1"
		precision = 1;
	};
	
	char *put = convbuf + bufsize - 1;
	while (num != 0)
	{
		uint64_t digit = num % base;
		num /= base;
		
		char c;
		if (digit < 10)
		{
			c = (char) digit + '0';
		}
		else
		{
			c = (char) digit - 10 + firstLetter;
		};
		
		*(--put) = c;
	};
	
	int zerofill = 0;
	if (strlen(put) < precision)
	{
		zerofill = precision - strlen(put);
	};
	
	if ((flags & FLAG_ZEROPAD) && ((flags & FLAG_LEFTADJ) == 0))
	{
		if ((zerofill+strlen(put)) < fieldWidth)
		{
			zerofill = fieldWidth - strlen(put);
		};
	};
	
	if (zerofill < 0) zerofill = 0;
	while (zerofill--)
	{
		(*--put) = '0';
	};
	
	int ret = __printf_putfield(fp, put, strlen(put), flags, fieldWidth);
	if (precision > 60)
	{
		free(convbuf);
	};
	
	return ret;
};

static int __printf_conv_fixedfloat(FILE *fp, int flags, int lenmod, int fieldWidth, int precision, int base, char spec, va_list ap)
{
	double x;
	if (lenmod == _LM_L)
	{
		// loss of precision will occur perhaps in the future we can modify this to work on
		// long doubles instead of normal doubles.
		x = (double) va_arg(ap, long double);
	}
	else
	{
		x = va_arg(ap, double);
	};
	uint64_t val64 = *((uint64_t*)(&x));
	
	// check for infinity and NaN
	// (is this the most terrible code i ever wrote?)
	if (((val64 >> 52) & 0x7FF) == 0x7FF)
	{
		if (spec == 'f')
		{
			if (val64 & 0x8000000000000000)
			{
				if (val64 & 0xfffffffffffff)
				{
					return __printf_putfield(fp, "-nan", 4, flags, fieldWidth);
				}
				else
				{
					return __printf_putfield(fp, "-inf", 4, flags, fieldWidth);
				};
			}
			else
			{
				if (val64 & 0xfffffffffffff)
				{
					return __printf_putfield(fp, "nan", 3, flags, fieldWidth);
				}
				else
				{
					return __printf_putfield(fp, "inf", 3, flags, fieldWidth);
				};
			};
		}
		else
		{
			if (val64 & 0x8000000000000000)
			{
				if (val64 & 0xfffffffffffff)
				{
					return __printf_putfield(fp, "-NAN", 4, flags, fieldWidth);
				}
				else
				{
					return __printf_putfield(fp, "-INF", 4, flags, fieldWidth);
				};
			}
			else
			{
				if (val64 & 0xfffffffffffff)
				{
					return __printf_putfield(fp, "NAN", 3, flags, fieldWidth);
				}
				else
				{
					return __printf_putfield(fp, "INF", 3, flags, fieldWidth);
				};
			};
		};
	};
	
	if (precision == -1)
	{
		precision = 6;
	};

	if (precision == 0)
	{
		precision = 1;
	};

	int negative = 0;
	if (x < 0)
	{
		negative = 1;
		x = -x;
	};
	
	// do the integer part
	char intconvbuf[30];
	uint64_t intPart = (uint64_t) x;
	char *put = &intconvbuf[29];
	*put = 0;
	
	do
	{
		uint64_t digit = intPart % base;
		intPart /= base;
		
		char c;
		if (digit < 10)
		{
			c = (char) digit + '0';
		}
		else
		{
			c = (char) digit - 10 + spec - 5;
		};
		
		*(--put) = c;
	} while (intPart != 0);
	
	if (negative) *(--put) = '-';
	
	char *convbuf = (char*) malloc(strlen(put) + 1 + precision + 1);
	strcpy(convbuf, put);
	put = &convbuf[strlen(convbuf)];
	*put++ = '.';
	memset(put, 0, precision-1);
	
	put[precision] = 0;
	put += precision - 1;
	
	uint64_t exp = 1;
	while (precision--)
	{
		exp *= base;
	};
	
	uint64_t fracPart = (uint64_t) (x * exp);
	while (*put != '.')
	{
		uint64_t digit = fracPart % base;
		fracPart /= base;
		
		char c;
		if (digit < 10)
		{
			c = (char) digit + '0';
		}
		else
		{
			c = (char) digit - 10 + spec - 5;
		};
		
		*(put--) = c;
	};
	
	int result = __printf_putfield(fp, convbuf, strlen(convbuf), flags, fieldWidth);
	free(convbuf);
	return result;
};

static int __printf_conv_expfloat(FILE *fp, int flags, int lenmod, int fieldWidth, int precision, int base, char spec, va_list ap)
{
	double x;
	if (lenmod == _LM_L)
	{
		// loss of precision will occur perhaps in the future we can modify this to work on
		// long doubles instead of normal doubles.
		x = (double) va_arg(ap, long double);
	}
	else
	{
		x = va_arg(ap, double);
	};
	uint64_t val64 = *((uint64_t*)(&x));
	
	// check for infinity and NaN
	// (is this the most terrible code i ever wrote?)
	if (((val64 >> 52) & 0x7FF) == 0x7FF)
	{
		if (spec == 'e')
		{
			if (val64 & 0x8000000000000000)
			{
				if (val64 & 0xfffffffffffff)
				{
					return __printf_putfield(fp, "-nan", 4, flags, fieldWidth);
				}
				else
				{
					return __printf_putfield(fp, "-inf", 4, flags, fieldWidth);
				};
			}
			else
			{
				if (val64 & 0xfffffffffffff)
				{
					return __printf_putfield(fp, "nan", 3, flags, fieldWidth);
				}
				else
				{
					return __printf_putfield(fp, "inf", 3, flags, fieldWidth);
				};
			};
		}
		else
		{
			if (val64 & 0x8000000000000000)
			{
				if (val64 & 0xfffffffffffff)
				{
					return __printf_putfield(fp, "-NAN", 4, flags, fieldWidth);
				}
				else
				{
					return __printf_putfield(fp, "-INF", 4, flags, fieldWidth);
				};
			}
			else
			{
				if (val64 & 0xfffffffffffff)
				{
					return __printf_putfield(fp, "NAN", 3, flags, fieldWidth);
				}
				else
				{
					return __printf_putfield(fp, "INF", 3, flags, fieldWidth);
				};
			};
		};
	};
	
	uint64_t absval64 = val64 & ~0x8000000000000000;
	double absval = *((double*)(&absval64));

	if (precision == -1)
	{
		precision = 6;
	};

	if (precision == 0)
	{
		precision = 1;
	};

	int expval = 0;
	while ((uint64_t)absval >= 10)
	{
		absval /= 10.0;
		x /= 10.0;
		expval++;
	};
	
	while (absval < 1 && expval > -precision)
	{
		absval *= 10.0;
		x *= 10.0;
		expval--;
	};
	
	// do the integer part
	char intconvbuf[30];
	uint64_t intPart = (uint64_t) x;
	char *put = &intconvbuf[29];
	*put = 0;
	
	do
	{
		uint64_t digit = intPart % base;
		intPart /= base;
		
		char c;
		if (digit < 10)
		{
			c = (char) digit + '0';
		}
		else
		{
			c = (char) digit - 10 + spec - 5;
		};
		
		*(--put) = c;
	} while (intPart != 0);
	
	char *convbuf = (char*) malloc(strlen(put) + 1 + precision + 1 + 5);
	strcpy(convbuf, put);
	put = &convbuf[strlen(convbuf)];
	*put++ = '.';
	memset(put, 0, precision-1);
	
	put[precision] = 0;
	put += precision - 1;
	
	uint64_t exp = 1;
	while (precision--)
	{
		exp *= base;
	};
	
	uint64_t fracPart = (uint64_t) (x * exp);
	while (*put != '.')
	{
		uint64_t digit = fracPart % base;
		fracPart /= base;
		
		char c;
		if (digit < 10)
		{
			c = (char) digit + '0';
		}
		else
		{
			c = (char) digit - 10 + spec - 4;
		};
		
		*(put--) = c;
	};
	
	put = &convbuf[strlen(convbuf)];
	*put++ = 'e';
	if (expval < 0) {*put++ = '-'; expval = -expval;}
	else *put++ = '+';
	if (expval >= 100) *put++ = '0'+((expval / 100) % 10);
	*put++ = '0'+((expval / 10) % 10);
	*put++ = '0'+(expval % 10);
	*put = 0;
	
	int result = __printf_putfield(fp, convbuf, strlen(convbuf), flags, fieldWidth);
	free(convbuf);
	return result;
};

static int __printf_conv_g(FILE *fp, int flags, int lenmod, int fieldWidth, int precision, int base, char spec, va_list ap)
{
	double x;
	if (lenmod == _LM_L)
	{
		// loss of precision will occur perhaps in the future we can modify this to work on
		// long doubles instead of normal doubles.
		x = (double) va_arg(ap, long double);
	}
	else
	{
		x = va_arg(ap, double);
	};
	double y = x;
	uint64_t val64 = *((uint64_t*)(&x));
	
	// check for infinity and NaN
	// (is this the most terrible code i ever wrote?)
	if (((val64 >> 52) & 0x7FF) == 0x7FF)
	{
		if (spec == 'e')
		{
			if (val64 & 0x8000000000000000)
			{
				if (val64 & 0xfffffffffffff)
				{
					return __printf_putfield(fp, "-nan", 4, flags, fieldWidth);
				}
				else
				{
					return __printf_putfield(fp, "-inf", 4, flags, fieldWidth);
				};
			}
			else
			{
				if (val64 & 0xfffffffffffff)
				{
					return __printf_putfield(fp, "nan", 3, flags, fieldWidth);
				}
				else
				{
					return __printf_putfield(fp, "inf", 3, flags, fieldWidth);
				};
			};
		}
		else
		{
			if (val64 & 0x8000000000000000)
			{
				if (val64 & 0xfffffffffffff)
				{
					return __printf_putfield(fp, "-NAN", 4, flags, fieldWidth);
				}
				else
				{
					return __printf_putfield(fp, "-INF", 4, flags, fieldWidth);
				};
			}
			else
			{
				if (val64 & 0xfffffffffffff)
				{
					return __printf_putfield(fp, "NAN", 3, flags, fieldWidth);
				}
				else
				{
					return __printf_putfield(fp, "INF", 3, flags, fieldWidth);
				};
			};
		};
	};
	
	uint64_t absval64 = val64 & ~0x8000000000000000;
	double absval = *((double*)(&absval64));
	
	if (precision == -1)
	{
		precision = 6;
	};
	
	if (precision == 0)
	{
		precision = 1;
	};

	int expval = 0;
	while ((uint64_t)absval >= 10)
	{
		absval /= 10.0;
		x /= 10.0;
		expval++;
	};
	
	while (absval < 1 && expval > -precision)
	{
		absval *= 10.0;
		x *= 10.0;
		expval--;
	};
	
	if (precision > expval && expval >= -4)
	{
		expval = 0;
		x = y;
	};
	
	// do the integer part
	char intconvbuf[30];
	uint64_t intPart = (uint64_t) x;
	char *put = &intconvbuf[29];
	*put = 0;
	
	do
	{
		uint64_t digit = intPart % base;
		intPart /= base;
		
		char c;
		if (digit < 10)
		{
			c = (char) digit + '0';
		}
		else
		{
			c = (char) digit - 10 + spec - 5;
		};
		
		*(--put) = c;
	} while (intPart != 0);
	
	char *convbuf = (char*) malloc(strlen(put) + 1 + precision + 1 + 5);
	strcpy(convbuf, put);
	put = &convbuf[strlen(convbuf)];
	*put++ = '.';
	memset(put, 0, precision-1);
	
	put[precision] = 0;
	put += precision - 1;
	
	uint64_t exp = 1;
	while (precision--)
	{
		exp *= base;
	};
	
	uint64_t fracPart = (uint64_t) (x * exp);
	while (*put != '.')
	{
		uint64_t digit = fracPart % base;
		fracPart /= base;
		
		char c;
		if (digit < 10)
		{
			c = (char) digit + '0';
		}
		else
		{
			c = (char) digit - 10 + spec - 4;
		};
		
		*(put--) = c;
	};
	
	if (expval != 0)
	{
		put = &convbuf[strlen(convbuf)];
		*put++ = 'e';
		if (expval < 0) {*put++ = '-'; expval = -expval;}
		else *put++ = '+';
		if (expval >= 100) *put++ = '0'+((expval / 100) % 10);
		*put++ = '0'+((expval / 10) % 10);
		*put++ = '0'+(expval % 10);
		*put = 0;
	};
	
	int result = __printf_putfield(fp, convbuf, strlen(convbuf), flags, fieldWidth);
	free(convbuf);
	return result;
};

static int __printf_conv_c(FILE *fp, va_list ap)
{
	fputc(va_arg(ap, int), fp);
	return 1;
};

static int __printf_conv_s(FILE *fp, int flags, int lenmod, int fieldWidth, int precision, va_list ap)
{
	char *str = va_arg(ap, char*);
	
	size_t len = strlen(str);
	if ((size_t)precision < len)
	{
		len = (size_t) precision;
	};
	
	return __printf_putfield(fp, str, len, flags, fieldWidth);
};

int vfprintf(FILE *fp, const char *fmt, va_list ap)
{
	int out = 0;
	while ((*fmt) != 0)
	{
		if (*fmt != '%')
		{
			fputc((int) *fmt, fp);
			out++;
			fmt++;
		}
		else
		{
			fmt++;
			if (*fmt == '%')
			{
				fputc('%', fp);
				fmt++;
				out++;
				continue;
			};
			
			int flags = 0;
			int lenmod = _LM_NONE;
			int fieldWidth = -1;
			int precision = -1;
			
			// parse all flags (this returns 0 when a non-flag character is seen)
			while (__parse_flag(&fmt, &flags));
			
			// field width and precision
			char *endptr;
			fieldWidth = (int) strtol(fmt, &endptr, 10);
			if (fmt == endptr)
			{
				fieldWidth = -1;
			}
			else
			{
				fmt = endptr;
			};
			
			if ((*fmt) == '.')
			{
				fmt++;
				precision = (int) strtol(fmt, &endptr, 10);
				if (fmt == endptr)
				{
					precision = -1;
				}
				else
				{
					fmt = endptr;
				};
			};
			
			// parse length modifiers if any
			// (only advance the fmt pointer if a modifier was parsed).
			switch (*fmt)
			{
			case 'h':
				lenmod = _LM_h;
				fmt++;
				if ((*fmt) == 'h')
				{
					lenmod = _LM_hh;
					fmt++;
				};
				break;
			case 'l':
				lenmod = _LM_l;
				fmt++;
				if ((*fmt) == 'l')
				{
					lenmod = _LM_ll;
					fmt++;
				};
				break;
			case 'L':
				lenmod = _LM_L;
				fmt++;
				break;
			case 'j':
				lenmod = _LM_j;
				fmt++;
				break;
			case 'z':
				lenmod = _LM_z;
				fmt++;
				break;
			case 't':
				lenmod = _LM_t;
				fmt++;
				break;
			};
			
			// finally the conversion specification part!
			// NOTE: we increase the fmt pointer at the very start, in the switch statement.
			// do not forget this!
			char spec = *fmt++;
			switch (spec)
			{
			case 'd':
			case 'i':
				out += __printf_conv_signed(fp, flags, lenmod, fieldWidth, precision, 10, 'a', ap);
				break;
			case 'o':
				out += __printf_conv_unsigned(fp, flags, lenmod, fieldWidth, precision, 8, 'a', ap);
				break;
			case 'u':
				out += __printf_conv_unsigned(fp, flags, lenmod, fieldWidth, precision, 10, 'a', ap);
				break;
			case 'x':
				out += __printf_conv_unsigned(fp, flags, lenmod, fieldWidth, precision, 16, 'a', ap);
				break;
			case 'X':
				out += __printf_conv_unsigned(fp, flags, lenmod, fieldWidth, precision, 16, 'A', ap);
				break;
			case 'f':
			case 'F':
				out += __printf_conv_fixedfloat(fp, flags, lenmod, fieldWidth, precision, 10, spec, ap);
				break;
			case 'a':
			case 'A':
				out += __printf_conv_fixedfloat(fp, flags, lenmod, fieldWidth, precision, 16, spec+5, ap);
				break;
			case 'e':
			case 'E':
				out += __printf_conv_expfloat(fp, flags, lenmod, fieldWidth, precision, 10, spec, ap);
				break;
			case 'g':
			case 'G':
				out += __printf_conv_g(fp, flags, lenmod, fieldWidth, precision, 10, spec, ap);
				break;
			case 'c':
				out += __printf_conv_c(fp, ap);
				break;
			case 's':
				out += __printf_conv_s(fp, flags, lenmod, fieldWidth, precision, ap);
				break;
			case 'p':
				out += __printf_conv_unsigned(fp, flags, _LM_l, fieldWidth, precision, 16, 'a', ap);
				break;
			case 'n':
				*(va_arg(ap, int*)) = out;
				break;
			};
		};
	};
	
	fflush(fp);
	return out;
};

int vprintf(const char *fmt, va_list ap)
{
	return vfprintf(stdout, fmt, ap);
};

int fprintf(FILE *fp, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	int ret = vfprintf(fp, fmt, ap);
	va_end(ap);
	return ret;
};

int printf(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	int ret = vprintf(fmt, ap);
	va_end(ap);
	return ret;
};
