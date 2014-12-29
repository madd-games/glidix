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

#include <glidix/module.h>
#include <glidix/console.h>
#include <glidix/sched.h>
#include <glidix/storage.h>
#include <glidix/string.h>

StorageDevice *sdTest;

static void sdThread(void *data)
{
	while (1)
	{
		SDCommand *cmd = sdPop(sdTest);

		switch (cmd->type)
		{
		case SD_CMD_READ:
			kprintf_debug("sd: read sector %a\n", cmd->index);
			memset(cmd->block, (char) (cmd->index & 0xFF), 512);
			break;
		case SD_CMD_WRITE:
			kprintf_debug("sd: write sector %a\n", cmd->index);
			break;
		};

		sdPostComplete(cmd);
	};
};

MODULE_INIT()
{
	kprintf("Test, setting up fake device.\n");
	SDParams params;
	params.flags = 0;
	params.blockSize = 512;
	params.totalSize = 1024 * 1024 * 2;		// 2MB
	sdTest = sdCreate(&params);
	CreateKernelThread(sdThread, NULL, NULL);
};

MODULE_FINI()
{
};
