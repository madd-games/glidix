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

const char *sampleText = "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Aenean est elit, condimentum vitae venenatis et, tincidunt vitae tortor. Nulla auctor sapien id nibh dictum efficitur. Suspendisse mollis at lacus id consequat. Donec cursus leo lorem, at egestas dui gravida non. Vivamus sit amet lorem quis quam venenatis ultrices. Ut nec velit vel est pulvinar dignissim non vitae nibh. Praesent id quam eu arcu tincidunt sodales vitae id neque. Ut bibendum orci nibh, auctor tempor eros ullamcorper id. Nunc maximus aliquam tellus at consectetur. Nam congue egestas ante, sed venenatis eros consectetur vel. Curabitur porttitor lectus ac semper pulvinar. Aliquam rutrum nulla id ex accumsan, in ullamcorper lectus aliquam. Interdum et malesuada fames ac ante ipsum primis in faucibus.\n\
Curabitur vehicula, ex vel placerat placerat, orci dolor tincidunt libero, in blandit nulla nunc venenatis diam. Vestibulum tincidunt vel ex congue volutpat. Sed nulla velit, euismod in neque non, condimentum accumsan lacus. Orci varius natoque penatibus et magnis dis parturient montes, nascetur ridiculus mus. Suspendisse et efficitur mauris. Maecenas nulla felis, posuere eget egestas eget, consequat quis nisl. Integer tempor ultricies odio vitae egestas. Nullam eleifend justo non tellus volutpat, sed suscipit metus congue. Integer egestas egestas risus, ac sodales velit hendrerit eget. Nulla et ex eu risus viverra pretium id vitae ante. Fusce molestie nibh in orci luctus volutpat. Nam quis laoreet ex. Praesent ac vehicula libero, ac semper augue. Fusce eget condimentum eros. Curabitur consectetur lectus ac purus semper, sit amet aliquet ipsum vehicula.\n\
Aenean molestie risus quis neque sodales, vel dictum metus ornare. Phasellus non tincidunt justo. Praesent vestibulum neque sed velit pulvinar aliquet. Duis orci ipsum, scelerisque commodo odio et, vestibulum sagittis risus. Duis non neque congue, mattis ex eget, consectetur eros. Aenean convallis feugiat lacinia. Mauris maximus quis ex vitae feugiat. Nullam facilisis vulputate dui, sit amet bibendum nisi luctus at. Nulla molestie purus lacus, non egestas leo gravida vel. Pellentesque id hendrerit velit, nec sodales nulla. Mauris rhoncus arcu non felis congue, at efficitur magna lacinia. Curabitur semper, enim a euismod auctor, ligula magna scelerisque eros, vel elementum velit sem ac urna. Vestibulum diam ante, pretium ac porttitor ac, faucibus vel libero.\n\
Nullam pulvinar posuere elit, vitae efficitur metus convallis ac. Fusce vitae elementum purus. Aliquam ac egestas nisl. Cras ac libero enim. Maecenas vel scelerisque ex. Nulla pulvinar congue posuere. Vivamus eu urna lacinia, pulvinar neque in, malesuada nisi. Donec eget nulla nec ligula scelerisque gravida feugiat ac nunc. Nam sit amet lacus eget mi bibendum semper. Quisque condimentum tristique semper. Aenean at nisi sit amet nisi egestas facilisis sed at metus. Mauris eget risus turpis.\n\
Morbi sit amet lorem turpis. Sed eget justo vulputate, ultricies tellus eget, mattis purus. Cras congue volutpat quam vel bibendum. In vel dapibus turpis. Aenean mauris augue, convallis ac eleifend vel, tincidunt vel tortor. Nam euismod interdum pellentesque. Duis eleifend ac est vitae tincidunt. Nam ultricies nibh ante, eget maximus tellus finibus vel. Sed condimentum odio libero, id ullamcorper nibh rhoncus ut. Aliquam non semper mauris. Nam vel tempor urna. In nisl erat, tincidunt id nisi sed, aliquet porta turpis.";

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
