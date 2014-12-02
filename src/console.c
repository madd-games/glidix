/*
	Glidix kernel

	Copyright (c) 2014, Madd Games.
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
#include <stdint.h>

struct
{
	uint64_t curX, curY;
	uint64_t curColor;
} consoleState;

void initConsole()
{
	consoleState.curX = 0;
	consoleState.curY = 0;
	consoleState.curColor = 0x07;
};

static void updateVGACursor()
{
	uint64_t pos = consoleState.curY * 80 + consoleState.curX;
	outb(0x3D4, 0x0F);
	outb(0x3D5, pos & 0xFF);
	outb(0x3D4, 0x0E);
	outb(0x3D5, (pos >> 8) & 0xFF);
};

static void scroll()
{
	uint8_t *vidmem = (uint8_t*) 0xB8000;

	uint64_t i;
	for (i=0; i<2*80*24; i++)
	{
		vidmem[i] = vidmem[i+160];
	};

	for (i=80*24; i<80*25; i++)
	{
		vidmem[2*i+0] = 0;
		vidmem[2*i+1] = 0x07;
	};

	consoleState.curY--;
	updateVGACursor();
};

static void kputch(char c)
{
	if (c == '\n')
	{
		consoleState.curX = 0;
		consoleState.curY++;
		if (consoleState.curY == 25) scroll();
	}
	else if (c == '\r')
	{
		consoleState.curX = 0;
	}
	else
	{
		uint8_t *vidmem = (uint8_t*) (0xB8000 + 2 * (consoleState.curY * 80 + consoleState.curX));
		*vidmem++ = c;
		*vidmem++ = consoleState.curColor;
		consoleState.curX++;

		if (consoleState.curX == 80)
		{
			consoleState.curX = 0;
			consoleState.curY++;
			if (consoleState.curY == 25) scroll();
		};
	};
};

void kputs(const char *str)
{
	while (*str != 0)
	{
		kputch(*str++);
	};

	updateVGACursor();
};

static void put_d(int d)
{
	if (d < 0)
	{
		kputch('-');
		d = -d;
	};

	char buffer[20];
	buffer[19] = 0;

	char *ptr = &buffer[18];
	do
	{
		*ptr = '0' + (d % 10);
		ptr--;
		d /= 10;
	} while (d != 0);

	kputs(ptr+1);
};

void kvprintf(const char *fmt, va_list ap)
{
	while (*fmt != 0)
	{
		char c = *fmt++;
		if (c != '%')
		{
			kputch(c);
		}
		else
		{
			c = *fmt++;
			if (c == '%')
			{
				kputch('%');
			}
			else if (c == 's')
			{
				const char *str = va_arg(ap, char*);
				kputs(str);
			}
			else if (c == 'd')
			{
				int d = va_arg(ap, int);
				put_d(d);
			};
		};
	};

	updateVGACursor();
};

void kprintf(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	kvprintf(fmt, ap);
	va_end(ap);
};
