/*
	Glidix GUI

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

#include <libddi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#define	VMGPU_CMD_UPDATE				DDI_COMMAND_ARG(VMGPU_CmdUpdate, 1)
typedef struct
{
	uint32_t					x;
	uint32_t					y;
	uint32_t					width;
	uint32_t					height;
} VMGPU_CmdUpdate;

int main(int argc, char *argv[])
{
	if (argc != 2)
	{
		fprintf(stderr, "USAGE:\t%s <display-device>\n", argv[0]);
		fprintf(stderr, "\tTests the display device.\n");
		return 1;
	};
	
	if (ddiInit(argv[1], O_RDWR) != 0)
	{
		fprintf(stderr, "%s: failed to open display device %s: %s\n", argv[0], argv[1], strerror(errno));
		return 1;
	};
	
	DDISurface *fbuf = ddiSetVideoMode(DDI_RES_AUTO);
	DDIColor color = {0xFF, 0xFF, 0x00, 0xFF};
	ddiFillRect(fbuf, 0, 0, fbuf->width, fbuf->height, &color);
	
	sleep(5);
	
	DDIColor pink = {0xFF, 0x00, 0xFF, 0xFF};
	ddiFillRect(fbuf, 0, 0, fbuf->width, fbuf->height, &pink);

	VMGPU_CmdUpdate update;
	update.x = 0;
	update.y = 0;
	update.width = fbuf->width;
	update.height = fbuf->height;
	ddiCommand(VMGPU_CMD_UPDATE, &update);

	while (1) pause();
	
	ddiQuit();
	return 0;
};
