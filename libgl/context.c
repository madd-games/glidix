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

#include <GL/ddigl.h>

#include <stdlib.h>
#include <libddi.h>

DDIGL_Context* __ddigl_current;

DDIGL_ContextParams* ddiglCreateContextParams()
{
	DDIGL_ContextParams *params = (DDIGL_ContextParams*) malloc(sizeof(DDIGL_ContextParams));
	
	params->stencilBits = 8;
	params->depthBits = 16;
	
	return params;
};

void ddiglDestroyContextParams(DDIGL_ContextParams *params)
{
	free(params);
};

DDIGL_Context* ddiglCreateContext(DDIPixelFormat *format, DDIGL_ContextParams *params, GLenum *error)
{
	if (ddiDriver->initgl == NULL)
	{
		// don't bother creating the context if we have no driver anyway
		if (error != NULL) *error = GL_INVALID_OPERATION;
		return NULL;
	};
	
	DDIGL_Context *ctx = (DDIGL_Context*) malloc(sizeof(DDIGL_Context));
	memset(ctx, 0, sizeof(DDIGL_Context));
	ctx->drvdata = NULL;
	ctx->error = GL_NO_ERROR;
	
	int mustFreeParams = 0;
	if (params == NULL)
	{
		params = ddiglCreateContextParams();
		mustFreeParams = 1;
	};
	
	int status = ddiDriver->initgl(ddiDrvCtx, format, params, ctx);
	if (mustFreeParams) free(params);
	
	if (status != GL_NO_ERROR)
	{
		if (error != NULL) *error = status;
		free(ctx);
		return NULL;
	};

	return ctx;
};

GLenum ddiglMakeCurrent(DDIGL_Context *ctx, DDISurface *target)
{
	if ((target->flags & DDI_RENDER_TARGET) == 0) return GL_INVALID_VALUE;
	
	if (ctx->setRenderTarget == NULL)
	{
		return GL_INVALID_OPERATION;
	};
	
	GLenum error = ctx->setRenderTarget(ctx, target);
	if (error != GL_NO_ERROR)
	{
		return error;
	};
	
	__ddigl_current = ctx;
	return GL_NO_ERROR;
};

void ddiglSetError(GLenum error)
{
	if (__ddigl_current->error == GL_NO_ERROR) __ddigl_current->error = error;
};
