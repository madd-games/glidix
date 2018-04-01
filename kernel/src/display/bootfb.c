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

#include <glidix/util/common.h>
#include <glidix/util/string.h>
#include <glidix/display/video.h>
#include <glidix/display/bootfb.h>
#include <glidix/display/console.h>
#include <glidix/hw/pagetab.h>

static int bootfbSetMode(VideoDisplay *disp, VideoModeRequest *req)
{
	if (req->res != VIDEO_RES_AUTO)
	{
		return -1;
	};
	
	disableConsole();
	memcpy(&req->format, &consoleState.format, sizeof(PixelFormat));
	req->res = VIDEO_RES_SPECIFIC(consoleState.pixelWidth, consoleState.pixelHeight);
	return 0;
};

static void bootfbGetInfo(VideoDisplay *disp, VideoInfo *info)
{
	strcpy(info->renderer, "softrender");
	info->numPresentable = 1;
	info->features = 0;
};

static uint64_t bootfbGetPage(VideoDisplay *disp, off_t off)
{
	if (off >= consoleState.fbSize)
	{
		return 0;
	};
	
	uint64_t base = VIRT_TO_FRAME(consoleState.fb);
	return base + (off >> 12);
};

static VideoOps bootfbOps = {
	.setmode = bootfbSetMode,
	.getinfo = bootfbGetInfo,
	.getpage = bootfbGetPage,
};

void bootfbInit()
{
	VideoDriver *drv = videoCreateDriver("bootfb");
	assert(drv != NULL);
	VideoDisplay *disp = videoCreateDisplay(drv, NULL, &bootfbOps);
	assert(disp != NULL);
};
