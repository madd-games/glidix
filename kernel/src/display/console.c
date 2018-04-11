/*
	Glidix kernel

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

#include <glidix/display/console.h>
#include <glidix/hw/port.h>
#include <glidix/thread/ticket.h>
#include <glidix/util/string.h>
#include <stdint.h>
#include <glidix/thread/mutex.h>
#include <glidix/thread/semaphore.h>
#include <glidix/hw/physmem.h>
#include <glidix/hw/pagetab.h>
#include <glidix/fs/vfs.h>
#include <glidix/fs/devfs.h>

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
	_LM_j,
	_LM_z,
	_LM_t,
	_LM_L,
};

ConsoleState consoleState;

static Mutex consoleLock;

/**
 * Initial 4KB buffer for the kernel log.
 */
SECTION(".klog") char initklog[0x1000];

/**
 * Pointer to the next byte to store in the kernel log, and a semaphore counting the bytes stored.
 */
static char *klogput;
static Semaphore semLog;
static Semaphore semLogLock;

/**
 * Console colors (RGB).
 */
unsigned char consoleColors[16*3] = {
	0x00, 0x00, 0x00,		/* 0 */
	0x00, 0x00, 0xAA,		/* 1 */
	0x00, 0xAA, 0x00,		/* 2 */
	0x00, 0xAA, 0xAA,		/* 3 */
	0xAA, 0x00, 0x00,		/* 4 */
	0xAA, 0x00, 0xAA,		/* 5 */
	0xAA, 0x55, 0x00,		/* 6 */
	0xAA, 0xAA, 0xAA,		/* 7 */
	0x55, 0x55, 0x55,		/* 8 */
	0x55, 0x55, 0xFF,		/* 9 */
	0x55, 0xFF, 0x55,		/* A */
	0x55, 0xFF, 0xFF,		/* B */
	0xFF, 0x55, 0x55,		/* C */
	0xFF, 0x55, 0xFF,		/* D */
	0xFF, 0xFF, 0x55,		/* E */
	0xFF, 0xFF, 0xFF,		/* F */
};

/**
 * Console colors converted to framebuffer pixel format.
 */
uint32_t consolePixels[16];

/**
 * Whether the console should be outputting to COM1.
 */
int conOutCom = 0;

/**
 * Console font defined in confont.c.
 */
extern const unsigned char confont[16*256];

void getConsoleSize(unsigned short *width, unsigned short *height)
{
	*width = (unsigned short) consoleState.width;
	*height = (unsigned short) consoleState.height;
};

void enableDebugTerm()
{
	conOutCom = 1;
};

static uint32_t maskToPixel(uint8_t value, uint32_t mask)
{
	uint32_t pixel = (uint32_t) value;
	while ((mask & 1) == 0)
	{
		pixel <<= 1;
		mask >>= 1;
	};
	
	return pixel;
};

static void undrawCursor()
{
	if (consoleState.cursorDrawn)
	{
		const uint32_t *fetch = consoleState.behindCursor;
		uint8_t *put = consoleState.cursorPtr;
		
		int count = 16;
		while (count--)
		{
			*((uint32_t*)put) = *fetch++;
			put += consoleState.pitch;
		};
		
		consoleState.cursorDrawn = 0;
	};
};

void initConsole()
{
	if ((bootInfo->features & KB_FEATURE_VIDEO) == 0)
	{
		// what can we possibly do?
		while (1)
		{
			cli();
			hlt();
		};
	};
	
	// convert console colors to pixel format
	int i;
	for (i=0; i<16; i++)
	{
		uint32_t redPart = maskToPixel(consoleColors[i*3+0], bootInfo->fbFormat.redMask);
		uint32_t greenPart = maskToPixel(consoleColors[i*3+1], bootInfo->fbFormat.greenMask);
		uint32_t bluePart = maskToPixel(consoleColors[i*3+2], bootInfo->fbFormat.blueMask);
		
		consolePixels[i] = redPart | greenPart | bluePart;
	};
	
	mutexInit(&consoleLock);
	consoleState.width = bootInfo->fbWidth / 9;
	consoleState.height = bootInfo->fbHeight / 16;
	consoleState.enabled = 1;
	consoleState.fb = bootInfo->framebuffer;
	consoleState.fbSize = bootInfo->fbHeight * (bootInfo->fbWidth * (bootInfo->fbFormat.bpp + bootInfo->fbFormat.pixelSpacing) + bootInfo->fbFormat.scanlineSpacing);
	consoleState.pitch = bootInfo->fbFormat.bpp * bootInfo->fbWidth + bootInfo->fbFormat.scanlineSpacing;
	memcpy(&consoleState.format, &bootInfo->fbFormat, sizeof(PixelFormat));
	consoleState.cursorDrawn = 0;
	consoleState.pixelWidth = bootInfo->fbWidth;
	consoleState.pixelHeight = bootInfo->fbHeight;
	clearScreen();
	
	// set up kernel log
	klogput = initklog;
	semInit2(&semLog, 0);
	semInit(&semLogLock);
};

static void updateVGACursor()
{
	if (!consoleState.enabled) return;
	undrawCursor();
	
	if (consoleState.curY >= consoleState.height || consoleState.curX >= consoleState.width) return;

	consoleState.cursorPtr = consoleState.fb + 16 * consoleState.curY * consoleState.pitch + 4 * 9 * consoleState.curX;
	uint32_t *put = consoleState.behindCursor;
	uint8_t *buf = consoleState.cursorPtr;
	
	int count = 16;
	while (count--)
	{
		*put++ = *((uint32_t*)buf);
		*((uint32_t*)buf) = 0x66666666;
		
		buf += consoleState.pitch;
	};
	
	consoleState.cursorDrawn = 1;
};

void clearScreen()
{
	if (!consoleState.enabled) return;
	
	mutexLock(&consoleLock);
	consoleState.curX = 0;
	consoleState.curY = 0;
	consoleState.curColor = 0x07;
	consoleState.putcon = 1;
	consoleState.cursorDrawn = 0;
	
	memset(consoleState.fb, 0, consoleState.fbSize);
	
	updateVGACursor();
	mutexUnlock(&consoleLock);
};

void setCursorPos(uint8_t x, uint8_t y)
{
	mutexLock(&consoleLock);
	consoleState.curX = x;
	consoleState.curY = y;
	updateVGACursor();
	mutexUnlock(&consoleLock);
};

void setConsoleColor(uint8_t col)
{
	mutexLock(&consoleLock);
	consoleState.curColor = col;
	mutexUnlock(&consoleLock);
};

static void kputch(char c);
static void kputbuf_unlocked(const char *buf, size_t size)
{
	while (size--)
	{
		kputch(*buf++);
	};
};

static void scroll()
{
	if (!consoleState.enabled) return;
	undrawCursor();
	
	int y;
	for (y=0; y<consoleState.height-1; y++)
	{
		uint8_t *thisLine = consoleState.fb + (16*y) * consoleState.pitch;
		uint8_t *nextLine = thisLine + 16 * consoleState.pitch;
		
		memcpy(thisLine, nextLine, 16 * consoleState.pitch);
	};
	
	uint8_t *end = consoleState.fb + 16*(consoleState.height - 1) * consoleState.pitch;
	memset(end, 0, 16 * consoleState.pitch);

	consoleState.curY--;
	updateVGACursor();
};

static void renderChar(int x, int y, char c)
{
	const uint8_t *fetch = &confont[16*(unsigned int)(unsigned char)c];
	uint8_t *put = consoleState.fb + (16*y) * consoleState.pitch + (9*x) * 4;
	
	if (put == consoleState.cursorPtr)
	{
		consoleState.cursorDrawn = 0;
	};
	
	int height = 16;
	while (height--)
	{
		uint32_t *put32 = (uint32_t*) put;
		
		int i;
		for (i=0; i<9; i++)
		{
			// when expanding to 9x16, the last column is repeated. i found this through
			// trial and error. JUST TRUST ME OK ?!
			int index = i;
			if (index == 8) index = 7;
			
			uint8_t masks[8] = {128, 64, 32, 16, 8, 4, 2, 1};
			if ((*fetch) & masks[index])
			{
				*put32++ = consolePixels[consoleState.curColor & 0xF];
			}
			else
			{
				*put32++ = consolePixels[consoleState.curColor >> 4];
			};
		};
		
		fetch++;
		put += consoleState.pitch;
	};
};

static void kputch(char c)
{
	outb(0xE9, c);
	if (conOutCom)
	{
		while ((inb(0x3F8 + 5) & 0x20) == 0);
		outb(0x3F8, c);
	};
	
	*klogput++ = c;
	semSignal2(&semLog, 1);		// DO NOT use semSignal(); see semaphore.c as to why
	if ((((uint64_t)klogput) & 0xFFF) == 0)
	{
		// crossed a page boundary, ensure page allocated
		PDPTe *pdpte = VIRT_TO_PDPTE(klogput);
		PDe *pde = VIRT_TO_PDE(klogput);
		PTe *pte = VIRT_TO_PTE(klogput);
		
		if (!pdpte->present)
		{
			pdpte->present = 1;
			pdpte->pdPhysAddr = phmAllocZeroFrame();
			pdpte->xd = 1;
			pdpte->rw = 1;
			refreshAddrSpace();
		};
		
		if (!pde->present)
		{
			pde->present = 1;
			pde->ptPhysAddr = phmAllocZeroFrame();
			pde->xd = 1;
			pde->rw = 1;
			refreshAddrSpace();
		};
		
		if (!pte->present)
		{
			pte->present = 1;
			pte->framePhysAddr = phmAllocFrame();
			pte->xd = 1;
			pte->rw = 1;
			refreshAddrSpace();
		};
	};
	
	if (!consoleState.putcon) return;
	if (!consoleState.enabled) return;
	
	if (consoleState.curY == consoleState.height) scroll();

	if (c == '\n')
	{
		consoleState.curX = 0;
		consoleState.curY++;
		if (consoleState.curY == consoleState.height) scroll();
	}
	else if (c == '\r')
	{
		consoleState.curX = 0;
	}
	else if (c == '\b')
	{
		if (consoleState.curX == 0)
		{
			if (consoleState.curY == 0) return;
			consoleState.curY--;
			consoleState.curX = consoleState.width;
		};
		consoleState.curX--;
		renderChar(consoleState.curX, consoleState.curY, 0);
	}
	else if (c == '\t')
	{
		consoleState.curX = (consoleState.curX/8+1)*8;
		if (consoleState.curX >= consoleState.width)
		{
			consoleState.curY++;
			consoleState.curX -= consoleState.width;
			if (consoleState.curY == consoleState.height) scroll();
		};
	}
	else
	{
		renderChar(consoleState.curX, consoleState.curY, c);
		consoleState.curX++;

		if (consoleState.curX == consoleState.width)
		{
			consoleState.curX = 0;
			consoleState.curY++;
		};
	};
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

static void __printf_putfield(const char *data, size_t len, int flags, int fieldWidth)
{
	if ((fieldWidth == -1) || (len > fieldWidth))
	{
		kputbuf_unlocked(data, len);
		return;
	};
	
	if (flags & FLAG_LEFTADJ)
	{
		kputbuf_unlocked(data, len);
		size_t toPad = fieldWidth - len;
		
		while (toPad--)
		{
			kputch(' ');
		};
	}
	else
	{
		size_t toPad = fieldWidth - len;
		
		while (toPad--)
		{
			kputch(' ');
		};

		kputbuf_unlocked(data, len);
	};
};

/**
 * The "firstLetter" argument is the letter used to represent 10 if base > 10.
 */
static void __printf_conv_signed(int flags, int lenmod, int fieldWidth, int precision, int base, char firstLetter, va_list ap)
{
	size_t bufsize = 64;
	char *convbuf;
	if (precision > 60)
	{
		convbuf = (char*) kalloca(precision+4);
		bufsize = precision + 4;
	}
	else
	{
		convbuf = (char*) kalloca(64);
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
	
	__printf_putfield(put, strlen(put), flags, fieldWidth);
};

static void __printf_conv_unsigned(int flags, int lenmod, int fieldWidth, int precision, int base, char firstLetter, va_list ap)
{
	size_t bufsize = 64;
	char *convbuf;
	if (precision > 60)
	{
		convbuf = (char*) kalloca(precision+4);
		bufsize = precision + 4;
	}
	else
	{
		convbuf = (char*) kalloca(64);
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
	
	__printf_putfield(put, strlen(put), flags, fieldWidth);
};

static void __printf_conv_c(va_list ap)
{
	kputch(va_arg(ap, int));
};

static void __printf_conv_s(int flags, int lenmod, int fieldWidth, int precision, va_list ap)
{
	char *str = va_arg(ap, char*);
	
	size_t len = strlen(str);
	if ((size_t)precision < len)
	{
		len = (size_t) precision;
	};
	
	__printf_putfield(str, len, flags, fieldWidth);
};

void kvprintf_gen(uint8_t putcon, const char *fmt, va_list ap)
{
	mutexLock(&consoleLock);
	consoleState.putcon = putcon;

	while ((*fmt) != 0)
	{
		if (*fmt != '%')
		{
			kputch(*fmt);
			fmt++;
		}
		else
		{
			fmt++;
			if (*fmt == '%')
			{
				kputch('%');
				fmt++;
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
			fieldWidth = (int) strtoul(fmt, &endptr, 10);
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
				precision = (int) strtoul(fmt, &endptr, 10);
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
			case 'L':
				lenmod = _LM_L;
				fmt++;
				break;
			};
			
			// finally the conversion specification part!
			// NOTE: we increase the fmt pointer at the very start, inthe switch statement.
			// do not forget this!
			char spec = *fmt++;
			switch (spec)
			{
			case 'd':
			case 'i':
				__printf_conv_signed(flags, lenmod, fieldWidth, precision, 10, 'a', ap);
				break;
			case 'o':
				__printf_conv_unsigned(flags, lenmod, fieldWidth, precision, 8, 'a', ap);
				break;
			case 'u':
				__printf_conv_unsigned(flags, lenmod, fieldWidth, precision, 10, 'a', ap);
				break;
			case 'x':
				__printf_conv_unsigned(flags, lenmod, fieldWidth, precision, 16, 'a', ap);
				break;
			case 'X':
				__printf_conv_unsigned(flags, lenmod, fieldWidth, precision, 16, 'A', ap);
				break;
			case 'c':
				__printf_conv_c(ap);
				break;
			case 's':
				__printf_conv_s(flags, lenmod, fieldWidth, precision, ap);
				break;
			case 'p':
				__printf_conv_unsigned(flags, _LM_l, fieldWidth, precision, 16, 'a', ap);
				break;
			};
		};
	};

	updateVGACursor();

	mutexUnlock(&consoleLock);
};

void kvprintf(const char *fmt, va_list ap)
{
	kvprintf_gen(1, fmt, ap);
};

void kprintf(const char *fmt, ...)
{
	if (strlen(fmt) >= 4)
	{
		if (memcmp(fmt, "ACPI", 4) == 0)
		{
			return;
		};
	};
	
	va_list ap;
	va_start(ap, fmt);
	kvprintf(fmt, ap);
	va_end(ap);
};

void kvprintf_debug(const char *fmt, va_list ap)
{
	kvprintf_gen(0, fmt, ap);
};

void kprintf_debug(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	kvprintf_debug(fmt, ap);
	va_end(ap);
};

void kputbuf(const char *buf, size_t size)
{
	mutexLock(&consoleLock);
	consoleState.putcon = 1;
	while (size--)
	{
		if (*buf == '\e')
		{
			buf++;
			
			if (size == 0) break;
			
			char cmd = *buf++;
			size--;
			
			if (cmd == 33)
			{
				clearScreen();
			}
			else if (cmd == 34)
			{
				setConsoleColor((uint8_t) *buf++);
				size--;
			}
			else if (cmd == 35)
			{
				uint8_t x = (uint8_t) *buf++;
				uint8_t y = (uint8_t) *buf++;
				if (x >= consoleState.width) x = 0;
				if (y >= consoleState.height) y = 0;
				
				size -= 2;

				setCursorPos(x, y);
			};
		}
		else
		{
			kputch(*buf++);
		};
	};
	updateVGACursor();
	mutexUnlock(&consoleLock);
};

void kputbuf_debug(const char *buf, size_t size)
{
	mutexLock(&consoleLock);
	consoleState.putcon = 0;
	while (size--)
	{
		kputch(*buf++);
	};
	updateVGACursor();
	mutexUnlock(&consoleLock);
};

typedef struct
{
	char on;
	char off;
	uint64_t mask;
} Flag;

static Flag flagList[11] = {
	{'C', 'c', (1 << 0)},
	{'P', 'p', (1 << 2)},
	{'A', 'a', (1 << 4)},
	{'Z', 'z', (1 << 6)},
	{'S', 's', (1 << 7)},
	{'T', 't', (1 << 8)},
	{'I', 'i', (1 << 9)},
	{'D', 'd', (1 << 10)},
	{'O', 'o', (1 << 11)},
	{'N', 'n', (1 << 14)},
	{'R', 'r', (1 << 16)}
};

static void printFlags(uint64_t flags)
{
	int i;
	for (i=0; i<11; i++)
	{
		if (flags & flagList[i].mask)
		{
			kprintf("%c", flagList[i].on);
		}
		else
		{
			kprintf("%c", flagList[i].off);
		};
	};
};

void kdumpregs(Regs *regs)
{
	kprintf("FS:  0x%016lX\tGS:  0x%016lX\n", regs->fsbase, regs->gsbase);
	kprintf("DS:  0x%016lX\tSS:  0x%016lX\n", regs->ds, regs->ss);
	kprintf("RDI: 0x%016lX\tRSI: 0x%016lX\n", regs->rdi, regs->rsi);
	kprintf("RBP: 0x%016lX\tRSP: 0x%016lX\n", regs->rbp, regs->rsp);
	kprintf("RAX: 0x%016lX\tRBX: 0x%016lX\n", regs->rax, regs->rbx);
	kprintf("RCX: 0x%016lX\tRDX: 0x%016lX\n", regs->rcx, regs->rdx);
	kprintf("R8:  0x%016lX\tR9:  0x%016lX\n", regs->r8, regs->r9);
	kprintf("R10: 0x%016lX\tR11: 0x%016lX\n", regs->r10, regs->r11);
	kprintf("R12: 0x%016lX\tR13: 0x%016lX\n", regs->r12, regs->r13);
	kprintf("R14: 0x%016lX\tR15: 0x%016lX\n", regs->r14, regs->r15);
	kprintf("INO: 0x%016lX\tERR: 0x%016lX\n", regs->intNo, regs->errCode);
	kprintf("CS:  0x%016lX\tRIP: 0x%016lX\n", regs->cs, regs->rip);
	kprintf("RFLAGS: 0x%016lX (", regs->rflags); printFlags(regs->rflags); kprintf(")\n");
};

void unlockConsole()
{
	mutexUnlock(&consoleLock);
};

void disableConsole()
{
	mutexLock(&consoleLock);
	consoleState.enabled = 0;
	mutexUnlock(&consoleLock);
};

void enableConsole()
{
	mutexLock(&consoleLock);
	consoleState.enabled = 1;
	mutexUnlock(&consoleLock);
	
	clearScreen();
};

void setGfxTerm(int value)
{
};

void klog_pollinfo(Inode *inode, File *fp, Semaphore **sems)
{
	sems[PEI_READ] = &semLog;
};

ssize_t klog_pread(Inode *inode, File *fp, void *buffer, size_t size, off_t offset)
{
	static const char *klogfetch = initklog;
	
	int count = semWaitGen(&semLog, (int) size, SEM_W_FILE(fp->oflags), 0);
	if (count < 0)
	{
		ERRNO = -count;
		return -1;
	};
	
	semWait(&semLogLock);
	memcpy(buffer, klogfetch, count);
	klogfetch += count;
	semSignal(&semLogLock);
	
	return (ssize_t) count;
};

void initKlogFile()
{
	kprintf("Creating /dev/klog... ");
	
	Inode *inode = vfsCreateInode(NULL, VFS_MODE_CHARDEV | 0400);
	inode->pollinfo = klog_pollinfo;
	inode->pread = klog_pread;
	
	devfsAdd("klog", inode);
};
