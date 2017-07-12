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

#include <glidix/console.h>
#include <glidix/port.h>
#include <glidix/ticket.h>
#include <glidix/string.h>
#include <stdint.h>
#include <glidix/mutex.h>

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

static struct
{
	uint64_t curX, curY;
	uint64_t curColor;
	uint8_t putcon;
	unsigned char *buffer;
	int usingSoftware;
	int width, height;
	int gfxterm;
} consoleState;

static Mutex consoleLock;
SECTION(".videoram") unsigned char initVideoRAM[2*80*25];

/**
 * Whether the console should be outputting to COM1.
 */
int conOutCom = 0;

void initConsole()
{
	mutexInit(&consoleLock);
	consoleState.buffer = initVideoRAM;
	consoleState.usingSoftware = 0;
	consoleState.width = 80;
	consoleState.height = 25;
	consoleState.gfxterm = 0;
	clearScreen();
};

static void updateVGACursor()
{
	if (consoleState.gfxterm)
	{
		//lgiRenderConsole(consoleState.buffer, consoleState.width, consoleState.height);
	};
	
	if (consoleState.usingSoftware) return;
	uint64_t pos = consoleState.curY * consoleState.width + consoleState.curX;
	outb(0x3D4, 0x0F);
	outb(0x3D5, pos & 0xFF);
	outb(0x3D4, 0x0E);
	outb(0x3D5, (pos >> 8) & 0xFF);
};

void clearScreen()
{
	mutexLock(&consoleLock);
	consoleState.curX = 0;
	consoleState.curY = 0;
	consoleState.curColor = 0x07;
	consoleState.putcon = 1;
	
	uint8_t *videoram = consoleState.buffer;
	uint64_t i;
	
	for (i=0; i<consoleState.width*consoleState.height; i++)
	{
		videoram[2*i+0] = 0;
		videoram[2*i+1] = 0x07;
	};
	
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
	uint8_t *vidmem = consoleState.buffer;

	uint64_t i;
	for (i=0; i<2*consoleState.width*(consoleState.height-1); i++)
	{
		vidmem[i] = vidmem[i+consoleState.width*2];
	};

	for (i=consoleState.width*(consoleState.height-1); i<consoleState.width*consoleState.height; i++)
	{
		vidmem[2*i+0] = 0;
		vidmem[2*i+1] = 0x07;
	};

	consoleState.curY--;
	updateVGACursor();
};

static void kputch(char c)
{
	//outb(0xE9, c);
	if (conOutCom)
	{
		while ((inb(0x3F8 + 5) & 0x20) == 0);
		outb(0x3F8, c);
	};
	
	if (!consoleState.putcon) return;

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
		uint8_t *vidmem = &consoleState.buffer[2 * (consoleState.curY * consoleState.width + consoleState.curX)];
		*vidmem++ = 0;
		*vidmem++ = consoleState.curColor;
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
		uint8_t *vidmem = &consoleState.buffer[2 * (consoleState.curY * consoleState.width + consoleState.curX)];
		*vidmem++ = c;
		*vidmem++ = consoleState.curColor;
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
	/*if (putcon)*/ mutexLock(&consoleLock);
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

	/*if (putcon)*/ mutexUnlock(&consoleLock);
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

void switchConsoleToSoftwareBuffer(unsigned char *buffer, int width, int height)
{
	mutexLock(&consoleLock);
	consoleState.usingSoftware = 1;
	consoleState.buffer = buffer;
	consoleState.width = width;
	consoleState.height = height;
	mutexUnlock(&consoleLock);
	clearScreen();
};

void renderConsoleToScreen()
{
	mutexLock(&consoleLock);
	if (consoleState.usingSoftware)
	{
		//lgiRenderConsole(consoleState.buffer, consoleState.width, consoleState.height);
	}
	mutexUnlock(&consoleLock);
};

void setGfxTerm(int value)
{
	consoleState.gfxterm = value;
};
