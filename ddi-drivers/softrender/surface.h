/*
	Madd Software Renderer

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

#ifndef SOFTRENDER_SURFACE_H_
#define SOFTRENDER_SURFACE_H_

#include <libddi.h>

/**
 * Creates a surface. Implements ddiDriver->createSurface().
 */
int srCreateSurface(void *drvctx, DDISurface *surface, char *data);

/**
 * Opens a shared surface. Implements ddiDriver->openSurface().
 */
int srOpenSurface(void *drvctx, DDISurface *surface);

/**
 * Blit surfaces. Implements ddiDriver->blit().
 */
void srBlit(void *drvctx, DDISurface *src, int srcX, int srcY, DDISurface *dest, int destX, int destY, int width, int height);

/**
 * Overlay surfaces. Implements ddiDriver->overlay().
 */
void srOverlay(void *drvctx, DDISurface *src, int srcX, int srcY, DDISurface *dest, int destX, int destY, int width, int height);

/**
 * Fill a rectangle. Implements ddiDriver->rect().
 */
void srRect(void *drvctx, DDISurface *surf, int x, int y, int width, int height, DDIColor *color);

#endif
