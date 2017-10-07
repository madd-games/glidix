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
#include <time.h>
#include <assert.h>
#include <fcntl.h>

/**
 * A test for how fast libddi can blut surfaces.
 */
#define	RENDER_COUNT		1000
#define	TEST_DEVICE		"/dev/bga0"
#define	SURFACE_SIZE		500

int main()
{
	if (ddiInit(TEST_DEVICE, O_RDONLY) != 0)
	{
		fprintf(stderr, "blit-test: cannot initialize DDI on %s: %s\n", TEST_DEVICE, strerror(errno));
		return 1;
	};

	DDIPixelFormat format;
	format.bpp = 4;
	format.redMask = 0xFF0000;
	format.greenMask = 0x00FF00;
	format.blueMask = 0x0000FF;
	format.alphaMask = 0xFF000000;
	format.pixelSpacing = 0;
	format.scanlineSpacing = 0;
	
	DDISurface *surfA = ddiCreateSurface(&format, SURFACE_SIZE, SURFACE_SIZE, NULL, 0);
	DDISurface *surfB = ddiCreateSurface(&format, SURFACE_SIZE, SURFACE_SIZE, NULL, 0);
	
	int count = RENDER_COUNT;
	clock_t start = clock();
	
	while (count--)
	{
		ddiBlit(surfA, 0, 0, surfB, 0, 0, SURFACE_SIZE, SURFACE_SIZE);
	};
	
	clock_t end = clock();
	printf("Blitting a %dx%d surface %d times took %ld ms\n", SURFACE_SIZE, SURFACE_SIZE, RENDER_COUNT,
			(end-start)*1000/CLOCKS_PER_SEC);


	count = RENDER_COUNT;
	start = clock();
	
	while (count--)
	{
		ddiOverlay(surfA, 0, 0, surfB, 0, 0, SURFACE_SIZE, SURFACE_SIZE);
	};
	
	end = clock();
	printf("Overlaying a %dx%d surface %d times took %ld ms\n", SURFACE_SIZE, SURFACE_SIZE, RENDER_COUNT,
			(end-start)*1000/CLOCKS_PER_SEC);

	DDIColor color;	
	count = RENDER_COUNT;
	start = clock();

	while (count--)
	{
		ddiFillRect(surfA, 0, 0, SURFACE_SIZE, SURFACE_SIZE, &color);
	};
	
	end = clock();
	printf("Drawing a %dx%d rectangle %d times took %ld ms\n", SURFACE_SIZE, SURFACE_SIZE, RENDER_COUNT,
			(end-start)*1000/CLOCKS_PER_SEC);
	return 0;
};
