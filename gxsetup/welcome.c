/*
	Glidix Installer

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
#include <unistd.h>
#include <string.h>

#include "welcome.h"
#include "render.h"

#define	OPTION_WIDTH				50

const char *optionLabels[WELCOME_OPT_COUNT] = {
	"Upgrade current installation",
	"Install Glidix",
	"Drop to live shell",
	"Drop to GUI",
};

static int startX, startY;
static int selected = 0;

void drawOptions()
{
	int i;
	for (i=0; i<WELCOME_OPT_COUNT; i++)
	{
		char optionBar[OPTION_WIDTH];
		memset(optionBar, ' ', OPTION_WIDTH);
		memcpy(&optionBar[1], optionLabels[i], strlen(optionLabels[i]));
		
		setCursor((uint8_t)startX, (uint8_t)(startY + i));
		
		if (selected == i)
		{
			setColor(COLOR_SELECTION);
			optionBar[0] = 16;
		}
		else
		{
			setColor(COLOR_WINDOW);
		};
		
		write(1, optionBar, OPTION_WIDTH);
	};
	
	setCursor(79, 24);
};

int displayWelcome()
{
	renderWindow("\30" "\31" " Select option\t<ENTER> Accept selected option",
			"WELCOME",
			OPTION_WIDTH, WELCOME_OPT_COUNT,
			&startX, &startY);
	
	drawOptions();
	
	while (1)
	{
		uint8_t c;
		if (read(0, &c, 1) != 1) continue;
		
		if (c == 0x8B)
		{
			// up arrow
			if (selected != 0)
			{
				selected--;
				drawOptions();
			};
		}
		else if (c == 0x8C)
		{
			// down arrow
			if (selected != (WELCOME_OPT_COUNT-1))
			{
				selected++;
				drawOptions();
			};
		}
		else if (c == '\n')
		{
			break;
		};
	};
	
	return selected;
};
