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
 * A test for how fast libddi can render text.
 */
#define	RENDER_COUNT		10
#define	TEST_DEVICE		"/dev/bga0"

const char *sampleText = "Hello world";
int main()
{
	if (ddiInit(TEST_DEVICE, O_RDONLY) != 0)
	{
		fprintf(stderr, "text-render-test: cannot initialize DDI on %s: %s\n", TEST_DEVICE, strerror(errno));
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
	
	DDIFont *font = ddiLoadFont("DejaVu Sans", 20, DDI_STYLE_REGULAR, NULL);
	assert(font != NULL);
	
	int count = RENDER_COUNT;
	clock_t start = clock();
	while (count--)
	{
		ddiRenderText(&format, font, sampleText, NULL);
	};
	
	clock_t end = clock();
	ddiQuit();
	
	printf("I rendered the text %d times in %ld ms\n", RENDER_COUNT, (end-start)*1000/CLOCKS_PER_SEC);
	return 0;
};
