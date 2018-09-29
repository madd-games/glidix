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

#include "context.h"

static int getShift(uint32_t mask)
{
	int shift;
	for (shift=0; shift<32; shift++)
	{
		if ((0xFF << shift) == mask) return shift;
	};
	
	return -1;
};

static GLenum srSetRenderTarget(DDIGL_Context *ctx, DDISurface *target)
{
	SRContext *srctx = (SRContext*) ctx->drvdata;
	
	size_t depthSize, stencilSize;
	depthSize = target->width * target->height * (srctx->defaultBuffer->depth->bits >> 3);
	stencilSize = target->width * target->height * (srctx->defaultBuffer->stencil->bits >> 3);
	
	// make them both a multiple of 2
	if (depthSize & 1) depthSize++;
	if (stencilSize & 1) stencilSize++;
	
	// make sure we have allocated space for depth/stencil buffers
	if (target->drvdata == NULL)
	{
		target->drvdata = malloc(depthSize + stencilSize);
	};
	
	// set the buffers
	srctx->defaultBuffer->color[0]->width = target->width;
	srctx->defaultBuffer->color[0]->height = target->height;
	srctx->defaultBuffer->color[0]->data = target->data;
	
	srctx->defaultBuffer->depth->width = target->width;
	srctx->defaultBuffer->depth->height = target->height;
	srctx->defaultBuffer->depth->data = target->drvdata;
	
	srctx->defaultBuffer->stencil->width = target->width;
	srctx->defaultBuffer->stencil->height = target->height;
	srctx->defaultBuffer->stencil->data = (char*) target->drvdata + depthSize;
	
	// thanks
	return GL_NO_ERROR;
};

static void srClearColor(DDIGL_Context *ctx, GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha)
{
	SRContext *srctx = (SRContext*) ctx->drvdata;
	
	uint32_t redPart = (uint32_t)(red * 0xFF) << srctx->redShift;
	uint32_t greenPart = (uint32_t)(green * 0xFF) << srctx->greenShift;
	uint32_t bluePart = (uint32_t)(blue * 0xFF) << srctx->blueShift;
	uint32_t alphaPart = (uint32_t)(alpha * 0xFF) << srctx->alphaShift;
	
	srctx->clearColor = redPart | greenPart | bluePart | alphaPart;
};

static void srClearDepth(DDIGL_Context *ctx, GLclampd depth)
{
	SRContext *srctx = (SRContext*) ctx->drvdata;
	srctx->clearDepth = 0xFFFF * depth;
};

static void srClearStencil(DDIGL_Context *ctx, GLint s)
{
	SRContext *srctx = (SRContext*) ctx->drvdata;
	srctx->clearStencil = s;
};

static GLenum srClear(DDIGL_Context *ctx, GLbitfield mask)
{
	SRContext *srctx = (SRContext*) ctx->drvdata;
	
	if (mask & GL_COLOR_BUFFER_BIT)
	{
		int i;
		for (i=0; i<8; i++)
		{
			if (srctx->currentBuffer->color[i] != NULL)
			{
				uint32_t *put = srctx->currentBuffer->color[i]->data;
				size_t count = srctx->currentBuffer->color[i]->width * srctx->currentBuffer->color[i]->height;
				while (count--) *put++ = srctx->clearColor;
			};
		};
	};
	
	if (mask & GL_DEPTH_BUFFER_BIT)
	{
		if (srctx->currentBuffer->depth != NULL)
		{
			SRDepthStencilBuffer *buffer = srctx->currentBuffer->depth;
			size_t count = buffer->width * buffer->height;
			
			if (buffer->bits == 8)
			{
				uint8_t *put = (uint8_t*) buffer->data;
				while (count--) *put++ = (uint8_t) (srctx->clearDepth >> 8);
			}
			else
			{
				uint16_t *put = (uint16_t*) buffer->data;
				while (count--) *put++ = srctx->clearDepth;
			};
		};
	};
	
	if (mask & GL_STENCIL_BUFFER_BIT)
	{
		if (srctx->currentBuffer->stencil != NULL)
		{
			SRDepthStencilBuffer *buffer = srctx->currentBuffer->stencil;
			size_t count = buffer->width * buffer->height;
			
			if (buffer->bits == 8)
			{
				uint8_t *put = (uint8_t*) buffer->data;
				while (count--) *put++ = (uint8_t) srctx->clearStencil;
			}
			else
			{
				uint16_t *put = (uint16_t*) buffer->data;
				while (count--) *put++ = srctx->clearStencil;
			};
		};
	};
	
	return GL_NO_ERROR;
};

int srInitGL(void *drvctx, DDIPixelFormat *format, DDIGL_ContextParams *params, DDIGL_Context *ctx)
{
	// make sure the pixel format is suitable
	if (format->bpp != 4) return GL_INVALID_VALUE;
	
	int redShift = getShift(format->redMask);
	int greenShift = getShift(format->greenMask);
	int blueShift = getShift(format->blueMask);
	int alphaShift = getShift(format->alphaMask);
	
	// all shifts must be divisible by 8 and actually exist
	if (redShift == -1 || redShift % 8) return GL_INVALID_VALUE;
	if (greenShift == -1 || greenShift % 8) return GL_INVALID_VALUE;
	if (blueShift == -1 || blueShift % 8) return GL_INVALID_VALUE;
	if (alphaShift == -1 || alphaShift % 8) return GL_INVALID_VALUE;
	
	// it can't have any spacing either
	if (format->pixelSpacing != 0 || format->scanlineSpacing != 0) return GL_INVALID_VALUE;
	
	// check the sanity of parameters
	if (params->stencilBits != 8 && params->stencilBits != 16) return GL_INVALID_VALUE;
	if (params->depthBits != 8 && params->depthBits != 16) return GL_INVALID_VALUE;
	
	// create the default framebuffer
	SRFramebuffer *fbuf = (SRFramebuffer*) malloc(sizeof(SRFramebuffer));
	memset(fbuf, 0, sizeof(SRFramebuffer));
	
	SRColorBuffer *colorBuffer = (SRColorBuffer*) malloc(sizeof(SRColorBuffer));
	memset(colorBuffer, 0, sizeof(SRColorBuffer));
	fbuf->color[0] = colorBuffer;
	
	SRDepthStencilBuffer *depthBuffer = (SRDepthStencilBuffer*) malloc(sizeof(SRDepthStencilBuffer));
	memset(depthBuffer, 0, sizeof(SRDepthStencilBuffer));
	fbuf->depth = depthBuffer;
	depthBuffer->bits = params->depthBits;
	
	SRDepthStencilBuffer *stencilBuffer = (SRDepthStencilBuffer*) malloc(sizeof(SRDepthStencilBuffer));
	memset(stencilBuffer, 0, sizeof(SRDepthStencilBuffer));
	fbuf->stencil = stencilBuffer;
	stencilBuffer->bits = params->stencilBits;
	
	// create the context
	SRContext *srctx = (SRContext*) malloc(sizeof(SRContext));
	memset(srctx, 0, sizeof(SRContext));
	srctx->defaultBuffer = fbuf;
	srctx->currentBuffer = fbuf;
	srctx->redShift = redShift;
	srctx->greenShift = greenShift;
	srctx->blueShift = blueShift;
	srctx->alphaShift = alphaShift;
	srctx->clearColor = 0;
	srctx->clearDepth = 0xFFFF;
	srctx->clearStencil = 0;
	
	// finalize our work
	ctx->drvdata = srctx;
	ctx->setRenderTarget = srSetRenderTarget;
	ctx->clearDepth = srClearDepth;
	ctx->clearColor = srClearColor;
	ctx->clearStencil = srClearStencil;
	ctx->clear = srClear;
	
	// OK
	return GL_NO_ERROR;
};
