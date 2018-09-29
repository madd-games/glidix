/*
	Glidix GL

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

#ifndef __DDIGL_H
#define __DDIGL_H

#include <GL/gl.h>
#include <libddi.h>

/**
 * Context parameters structure.
 */
typedef struct __ddigl_params
{
	/**
	 * Number of bits in the stencil buffer (8 or 16). Default 8.
	 */
	int					stencilBits;
	
	/**
	 * Number of bits in the depth buffer (8 or 16). Default 16.
	 */
	int					depthBits;
} DDIGL_ContextParams;

/**
 * Context description.
 */
typedef struct __ddigl_ctx
{
	/**
	 * Driver-specific data.
	 */
	void*					drvdata;
	
	/**
	 * Error flag.
	 */
	GLenum					error;
	
	/**
	 * Set the specified surface as the default render target of this context. This is called
	 * when the context is made current for this surface. Returns GL_NO_ERROR on success, or
	 * an OpenGL error number on failure.
	 */
	GLenum (*setRenderTarget)(struct __ddigl_ctx *ctx, DDISurface *target);
	
	/**
	 * Set the clear color for the specified context.
	 */
	void (*clearColor)(struct __ddigl_ctx *ctx, GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha);
	
	/**
	 * Clear buffers. Return GL_NO_ERROR on success, or a GL error number on error.
	 */
	GLenum (*clear)(struct __ddigl_ctx *ctx, GLbitfield mask);
	
	/**
	 * Block until all previous GL commands have completed.
	 */
	void (*flush)(struct __ddigl_ctx *ctx);
	
	/**
	 * Set the clear depth for the specified context.
	 */
	void (*clearDepth)(struct __ddigl_ctx *ctx, GLclampd depth);
	
	/**
	 * Set the clear stencil value for the specified context.
	 */
	void (*clearStencil)(struct __ddigl_ctx *ctx, GLint s);
} DDIGL_Context;

extern DDIGL_Context* __ddigl_current;

/**
 * Create a context parameters structure.
 */
DDIGL_ContextParams* ddiglCreateContextParams();

/**
 * Destroy a context parameters structure.
 */
void ddiglDestroyContextParams(DDIGL_ContextParams *params);

/**
 * Create a GL context. If 'params' is NULL, then default parameters are used. The selected 'format' must be
 * compatible with the graphics driver in use. Returns the context on success, or NULL on error; and then sets
 * 'error' to a GL error number if it's not NULL.
 */
DDIGL_Context* ddiglCreateContext(DDIPixelFormat *format, DDIGL_ContextParams *params, GLenum *error);

/**
 * Make the specified context and render target current. Returns GL_NO_ERROR on success, or an OpenGL error number
 * on error.
 */
GLenum ddiglMakeCurrent(DDIGL_Context *ctx, DDISurface *target);

/**
 * Set the error flag on the current context.
 */
void ddiglSetError(GLenum error);

/**
 * Convenience function to clamp values.
 */
GLclampf ddiglClampf(GLclampf value, GLclampf minval, GLclampf maxval);

#endif
