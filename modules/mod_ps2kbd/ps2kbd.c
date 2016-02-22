/*
	Glidix kernel

	Copyright (c) 2014-2016, Madd Games.
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

#include <glidix/module.h>
#include <glidix/console.h>
#include <glidix/idt.h>
#include <glidix/common.h>
#include <glidix/port.h>
#include <glidix/sched.h>
#include <glidix/ktty.h>
#include <glidix/waitcnt.h>
#include <glidix/term.h>
#include <glidix/string.h>
#include <glidix/humin.h>

#define	MOUSE_BL			(1 << 0)
#define	MOUSE_BR			(1 << 1)
#define	MOUSE_BM			(1 << 2)
#define	MOUSE_XS			(1 << 4)
#define	MOUSE_YS			(1 << 5)
#define	MOUSE_XO			(1 << 6)
#define	MOUSE_YO			(1 << 7)

#define	MOUSE_BUTTONS			(MOUSE_BL | MOUSE_BR | MOUSE_BM)

static IRQHandler oldHandler;
static IRQHandler oldHandler12;
static HuminDevice *hups;

// US keyboard layout
static unsigned char keymap[128] =
{
		0,	0, '1', '2', '3', '4', '5', '6', '7', '8',	/* 9 */
	'9', '0', '-', '=', '\b',	/* Backspace */
	'\t',			/* Tab */
	'q', 'w', 'e', 'r',	/* 19 */
	't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',	/* Enter key */
		0,			/* 29	 - Control */
	'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',	/* 39 */
 '\'', '`',	 0x80,		/* Left shift */
 '\\', 'z', 'x', 'c', 'v', 'b', 'n',			/* 49 */
	'm', ',', '.', '/',	 0x80,				/* Right shift */
	'*',
		0,	/* Alt */
	' ',	/* Space bar */
		0,	/* Caps lock */
		0,	/* 59 - F1 key ... > */
		0,	 0,	 0,	 0,	 0,	 0,	 0,	 0,
		0,	/* < ... F10 */
		0,	/* 69 - Num lock*/
		0,	/* Scroll Lock */
		0,	/* Home key */
		CC_ARROW_UP,	/* Up Arrow */
		0,	/* Page Up */
	'-',
		CC_ARROW_LEFT,	/* Left Arrow */
		0,
		CC_ARROW_RIGHT,	/* Right Arrow */
	'+',
		0,	/* 79 - End key*/
		CC_ARROW_DOWN,	/* Down Arrow */
		0,	/* Page Down */
		0,	/* Insert Key */
		0,	/* Delete Key */
		0,	 0,	 0,
		0,	/* F11 Key */
		0,	/* F12 Key */
		0,	/* All other keys are undefined */
};

// when shift is pressed
static unsigned char keymapShift[128] =
{
		0,	0, '!', '@', '#', '$', '%', '^', '&', '*',	/* 9 */
	'(', ')', '_', '+', '\b',	/* Backspace */
	'\t',			/* Tab */
	'Q', 'W', 'E', 'R',	/* 19 */
	'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',	/* Enter key */
		0,			/* 29	 - Control */
	'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':',	/* 39 */
 '\"', '`',	 0x80,		/* Left shift */
 '\\', 'Z', 'X', 'C', 'V', 'B', 'N',			/* 49 */
	'M', '<', '>', '?',	 0x80,				/* Right shift */
	'*',
		0,	/* Alt */
	' ',	/* Space bar */
		0,	/* Caps lock */
		0,	/* 59 - F1 key ... > */
		0,	 0,	 0,	 0,	 0,	 0,	 0,	 0,
		0,	/* < ... F10 */
		0,	/* 69 - Num lock*/
		0,	/* Scroll Lock */
		0,	/* Home key */
		0,	/* Up Arrow */
		0,	/* Page Up */
	'-',
		0,	/* Left Arrow */
		0,
		0,	/* Right Arrow */
	'+',
		0,	/* 79 - End key*/
		0,	/* Down Arrow */
		0,	/* Page Down */
		0,	/* Insert Key */
		0,	/* Delete Key */
		0,	 0,	 0,
		0,	/* F11 Key */
		0,	/* F12 Key */
		0,	/* All other keys are undefined */
};

static volatile ATOMIC(uint8_t)	kbdbuf[64];
static volatile ATOMIC(int)	kbdput = 0;
static volatile ATOMIC(int)	kbdread = 0;

static WaitCounter wcKeyboard = WC_INIT;

static void onKeyboardIRQ(int irq)
{
	wcUp(&wcKeyboard);
};

static void onMouseIRQ(int irq)
{
	wcUp(&wcKeyboard);
};

static uint8_t mouseStateWas = 0;

static void kbdThread(void *data)
{
	int ctrl = 0;
	int shift = 0;
	while (1)
	{
		wcDown(&wcKeyboard);
		uint8_t status = inb(0x64);
		if ((status & 1) == 0)
		{
			// sporious interrupt?
			continue;
		};
		
		uint8_t code = inb(0x60);
		if (status & 0x20)
		{
			uint8_t flags = code;
			uint8_t xmot = inb(0x60);
			uint8_t ymot = inb(0x60);
			
			if ((xmot != 0) || (ymot != 0))
			{
				// motion
				int relx = (int) xmot;
				int rely = (int) ymot;
				
				if (flags & MOUSE_XS)
				{
					relx = relx - 256;
				};
				
				if (flags & MOUSE_YS)
				{
					rely = rely - 256;
				};
				
				rely = -rely;
				HuminEvent ev;
				memset(&ev, 0, sizeof(HuminEvent));
				ev.motion.type = HUMIN_EV_MOTION_RELATIVE;
				ev.motion.x = relx;
				ev.motion.y = rely;
				huminPostEvent(hups, &ev);
			};
			
			// check each of the buttons
			HuminEvent ev;
			memset(&ev, 0, sizeof(HuminEvent));
			
			uint8_t buttons = flags & MOUSE_BUTTONS;
			if (mouseStateWas != buttons)
			{
				if ((buttons & MOUSE_BL) != (mouseStateWas & MOUSE_BL))
				{
					if (buttons & MOUSE_BL) ev.button.type = HUMIN_EV_BUTTON_DOWN;
					else ev.button.type = HUMIN_EV_BUTTON_UP;
					ev.button.scancode = HUMIN_SC_MOUSE_LEFT;
					huminPostEvent(hups, &ev);
				};

				if ((buttons & MOUSE_BM) != (mouseStateWas & MOUSE_BM))
				{
					if (buttons & MOUSE_BM) ev.button.type = HUMIN_EV_BUTTON_DOWN;
					else ev.button.type = HUMIN_EV_BUTTON_UP;
					ev.button.scancode = HUMIN_SC_MOUSE_MIDDLE;
					huminPostEvent(hups, &ev);
				};

				if ((buttons & MOUSE_BR) != (mouseStateWas & MOUSE_BR))
				{
					if (buttons & MOUSE_BR) ev.button.type = HUMIN_EV_BUTTON_DOWN;
					else ev.button.type = HUMIN_EV_BUTTON_UP;
					ev.button.scancode = HUMIN_SC_MOUSE_RIGHT;
					huminPostEvent(hups, &ev);
				};
			};
			mouseStateWas = buttons;
			
			continue;
		};
		
		// first report it as an event for the humin interface.
		HuminEvent ev;
		memset(&ev, 0, sizeof(HuminEvent));
		ev.button.type = HUMIN_EV_BUTTON_DOWN;
		if (code & 0x80) ev.button.type = HUMIN_EV_BUTTON_UP;
		ev.button.scancode = code & 0x7F;
		huminPostEvent(hups, &ev);
		
		if (code == 0x1D)
		{
			ctrl = 1;
		}
		else if (code == 0x9D)
		{
			ctrl = 0;
		}
		else if ((code & 0x80) == 0)
		{
			unsigned char key = keymap[code];
			if (shift) key = keymapShift[code];
			if (key == 0x80)
			{
				shift = 1;
				continue;
			};
			if (key == 0) continue;

			if (ctrl)
			{
				//termPutChar('^');
				//termPutChar(keymapShift[code]);

				if (key == 'c')
				{
					//kprintf("PUTTING INT IN BUFFER\n");
					termPutChar(CC_VINTR);
				};
			}
			else
			{
				termPutChar(key);
			};
		}
		else if (code & 0x80)
		{
			unsigned char key = keymap[code & 0x7F];
			if (key == 0x80) shift = 0;
		};
	};
};

inline void mouse_wait(uint8_t a_type) //unsigned char
{
	uint32_t _time_out=100000; //unsigned int
	if(a_type==0)
	{
		while(_time_out--) //Data
		{
			if((inb(0x64) & 1)==1)
			{
				return;
			}
		}
		return;
	}
	else
	{
		while(_time_out--) //Signal
		{
			if((inb(0x64) & 2)==0)
			{
				return;
			}
		}
		return;
	}
}

inline void mouse_write(uint8_t a_write) //unsigned char
{
	//Wait to be able to send a command
	mouse_wait(1);
	//Tell the mouse we are sending a command
	outb(0x64, 0xD4);
	//Wait for the final part
	mouse_wait(1);
	//Finally write
	outb(0x60, a_write);
}

uint8_t mouse_read()
{
	//Get's response from mouse
	mouse_wait(0); 
	return inb(0x60);
}

void mouse_install()
{
	uint8_t _status;	//unsigned char

	//Enable the auxiliary mouse device
	mouse_wait(1);
	outb(0x64, 0xA8);
	
	//Enable the interrupts
	mouse_wait(1);
	outb(0x64, 0x20);
	mouse_wait(0);
	_status=(inb(0x60) | 2);
	mouse_wait(1);
	outb(0x64, 0x60);
	mouse_wait(1);
	outb(0x60, _status);
	
	//Tell the mouse to use default settings
	mouse_write(0xF6);
	mouse_read();	//Acknowledge
	
	//Enable the mouse
	mouse_write(0xF4);
	mouse_read();	//Acknowledge
};

MODULE_INIT()
{
	kprintf("Initializing the PS/2 keyboard\n");
	oldHandler = registerIRQHandler(1, onKeyboardIRQ);
	oldHandler12 = registerIRQHandler(12, onMouseIRQ);
	hups = huminCreateDevice("PS/2 Keyboard and Mouse");
	
	// initialize the mouse
	mouse_install();

	// init mouse
	mouse_write(0xF6);
	mouse_read();
	mouse_write(0xF4);
	mouse_read();
	
	while (inb(0x64) & 1) inb(0x60);
	
	KernelThreadParams kbdPars;
	memset(&kbdPars, 0, sizeof(KernelThreadParams));
	kbdPars.stackSize = 0x4000;
	kbdPars.name = "PS/2 keyboard driver";
	CreateKernelThread(kbdThread, &kbdPars, NULL);
	return 0;
};

MODULE_FINI()
{
	kprintf("Shutting down PS/2 keyboard\n");
	registerIRQHandler(1, oldHandler);
	registerIRQHandler(12, oldHandler12);
	huminDeleteDevice(hups);
	return 0;
};
