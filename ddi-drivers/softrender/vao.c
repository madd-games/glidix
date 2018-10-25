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
#include "vao.h"

DDIGL_VertexArray* srCreateVertexArray(DDIGL_Context *ctx, GLenum *error)
{
	DDIGL_VertexArray *vao = (DDIGL_VertexArray*) malloc(sizeof(DDIGL_VertexArray));
	if (vao == NULL)
	{
		*error = GL_OUT_OF_MEMORY;
		return NULL;
	};
	
	memset(vao, 0, sizeof(DDIGL_VertexArray));
	return vao;
};

GLenum srBindVertexArray(DDIGL_Context *ctx, DDIGL_VertexArray *vao)
{
	// no-op: we can just use ctx->vaoCurrent when we need the binding
	return GL_NO_ERROR;
};

void srDeleteVertexArray(DDIGL_Context *ctx, DDIGL_VertexArray *vao)
{
	free(vao);
};

GLenum srSetAttribEnable(DDIGL_Context *ctx, GLuint index, GLboolean enable)
{
	if (index >= 64) return GL_INVALID_VALUE;
	
	if (enable)
	{
		ctx->vaoCurrent->enable |= (1 << index);
	}
	else
	{
		ctx->vaoCurrent->enable &= ~(1 << index);
	};
	
	return GL_NO_ERROR;
};

GLenum srAttribPointer(DDIGL_Context *ctx, GLuint index, GLint size, GLenum type, GLboolean normalized,
				GLsizei stride, GLsizeiptr offset)
{
	if (index >= 64) return GL_INVALID_VALUE;
	if (ctx->arrayBuffer == NULL) return GL_INVALID_OPERATION;
	
	SRVertexAttrib *attrib = &ctx->vaoCurrent->attribs[index];
	attrib->buffer = ctx->arrayBuffer;
	attrib->size = size;
	attrib->type = type;
	attrib->normalized = normalized;
	attrib->stride = stride;
	attrib->offset = offset;
	
	return GL_NO_ERROR;
};
