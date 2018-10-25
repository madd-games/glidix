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

#ifndef CONTEXT_H_
#define CONTEXT_H_

#include <GL/ddigl.h>

/**
 * Softrender-specific color buffer information.
 */
typedef struct
{
	/**
	 * Dimensions.
	 */
	unsigned int					width, height;
	
	/**
	 * The pixel data.
	 */
	void*						data;
} SRColorBuffer;

/**
 * Softrender-specific depth/stencil buffer information.
 */
typedef struct
{
	/**
	 * Dimensions.
	 */
	unsigned int					width, height;
	
	/**
	 * Number of bits (8 or 16).
	 */
	int						bits;
	
	/**
	 * The depth/stencil data.
	 */
	void*						data;
} SRDepthStencilBuffer;

/**
 * Softrender-specific framebuffer information.
 */
typedef struct
{
	/**
	 * Color buffer(s).
	 */
	SRColorBuffer*					color[8];
	
	/**
	 * Depth and stencil buffers.
	 */
	SRDepthStencilBuffer*				depth;
	SRDepthStencilBuffer*				stencil;
} SRFramebuffer;

/**
 * Softrender-specific GL context information.
 */
typedef struct
{
	/**
	 * Default framebuffer.
	 */
	SRFramebuffer*					defaultBuffer;
	
	/**
	 * Current framebuffer.
	 */
	SRFramebuffer*					currentBuffer;

	/**
	 * Current pipeline.
	 */
	DDIGL_Pipeline*					currentPipeline;
	
	/**
	 * Number of bits that need to be appended to the right of an 8-bit value to place it in
	 * the correct bits of a color word.
	 */
	int						redShift;
	int						greenShift;
	int						blueShift;
	int						alphaShift;

	/**
	 * Clear color, depth and stencil values.
	 */
	uint32_t					clearColor;
	uint16_t					clearDepth;
	uint16_t					clearStencil;
	
	/**
	 * Viewport.
	 */
	int						viewX, viewY;
	int						viewW, viewH;
} SRContext;

int srInitGL(void *drvctx, DDIPixelFormat *format, DDIGL_ContextParams *params, DDIGL_Context *ctx);

#endif
