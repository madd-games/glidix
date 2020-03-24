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

#include <stdio.h>
#include <libgwm.h>
#include <time.h>
#include <sys/time.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <dirent.h>
#include <pthread.h>
#include <poll.h>
#include <sys/utsname.h>
#include <sys/systat.h>
#include <sys/call.h>
#include <errno.h>
#include <libddi.h>

GWMWindow* newGeneralTab(GWMWindow *notebook)
{
	struct utsname info;
	uname(&info);

	struct system_state sst;
	__syscall(__SYS_systat, &sst, sizeof(struct system_state));
	
	char *dispdev = realpath("/run/gwmdisp", NULL);
	if (dispdev == NULL)
	{
		dispdev = strdup("<cannot detect>");
	};
	
	GWMWindow *tab = gwmNewTab(notebook);
	gwmSetWindowCaption(tab, "General");
	
	GWMLayout *box = gwmCreateBoxLayout(GWM_BOX_VERTICAL);
	gwmSetWindowLayout(tab, box);
	
	GWMLayout *grid = gwmCreateGridLayout(2);
	gwmBoxLayoutAddLayout(box, grid, 0, 2, GWM_BOX_FILL);
	
	gwmGridLayoutAddWindow(grid, gwmCreateLabel(tab, "Glidix release:", 0), 1, 1, GWM_GRID_FILL, GWM_GRID_CENTER);
	gwmGridLayoutAddWindow(grid, gwmCreateLabel(tab, info.release, 0), 1, 1, GWM_GRID_FILL, GWM_GRID_CENTER);
	gwmGridLayoutAddWindow(grid, gwmCreateLabel(tab, "Glidix version:", 0), 1, 1, GWM_GRID_FILL, GWM_GRID_CENTER);
	gwmGridLayoutAddWindow(grid, gwmCreateLabel(tab, info.version, 0), 1, 1, GWM_GRID_FILL, GWM_GRID_CENTER);
	gwmGridLayoutAddWindow(grid, gwmCreateLabel(tab, "DDI driver in use:", 0), 1, 1, GWM_GRID_FILL, GWM_GRID_CENTER);
	gwmGridLayoutAddWindow(grid, gwmCreateLabel(tab, ddiDisplayInfo.renderer, 0), 1, 1, GWM_GRID_FILL, GWM_GRID_CENTER);
	gwmGridLayoutAddWindow(grid, gwmCreateLabel(tab, "DDI renderer string:", 0), 1, 1, GWM_GRID_FILL, GWM_GRID_CENTER);
	gwmGridLayoutAddWindow(grid, gwmCreateLabel(tab, ddiDriver->renderString, 0), 1, 1, GWM_GRID_FILL, GWM_GRID_CENTER);
	gwmGridLayoutAddWindow(grid, gwmCreateLabel(tab, "GWM display device:", 0), 1, 1, GWM_GRID_FILL, GWM_GRID_CENTER);
	gwmGridLayoutAddWindow(grid, gwmCreateLabel(tab, dispdev, 0), 1, 1, GWM_GRID_FILL, GWM_GRID_CENTER);
	gwmGridLayoutAddWindow(grid, gwmCreateLabel(tab, "RAM use:", 0), 1, 1, GWM_GRID_FILL, GWM_GRID_CENTER);
	
	GWMProgressBar *progMemUsage = gwmNewProgressBar(tab);
	gwmGridLayoutAddWindow(grid, progMemUsage, 1, 1, GWM_GRID_FILL, GWM_GRID_CENTER);
	gwmSetScaleValue(progMemUsage, (double) (sst.sst_frames_used - sst.sst_frames_cached) / (double) sst.sst_frames_total);

	free(dispdev);
	return tab;
};
