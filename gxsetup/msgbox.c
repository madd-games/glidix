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

#include "msgbox.h"
#include "render.h"

#define	MSGBOX_WIDTH				50
#define	MSGBOX_HEIGHT				5

void msgbox(const char *caption, const char *text)
{
	int startX, startY;
	renderWindow("<ENTER> Accept", caption, MSGBOX_WIDTH, MSGBOX_HEIGHT, &startX, &startY);
	
	setCursor((uint8_t)((MSGBOX_WIDTH-strlen(text))/2 + startX), (uint8_t)(startY+1));
	setColor(COLOR_WINDOW);
	printf("%s", text);
	
	setCursor((uint8_t)((MSGBOX_WIDTH-4)/2 + startX), (uint8_t)(startY + 3));
	setColor(COLOR_SELECTION);
	printf("<OK>");
	
	setCursor(79, 24);
	
	while (1)
	{
		char c;
		if (read(0, &c, 1) != 1) continue;
		
		if (c == '\n') break;
	};
};
