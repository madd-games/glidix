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

#include "buffer.h"

DDIGL_Buffer* srCreateBuffer(DDIGL_Context *ctx, GLenum *error)
{
	DDIGL_Buffer *buffer = (DDIGL_Buffer*) malloc(sizeof(DDIGL_Buffer));
	if (buffer == NULL)
	{
		*error = GL_OUT_OF_MEMORY;
		return NULL;
	};
	
	buffer->data = NULL;
	buffer->size = 0;
	return buffer;
};

GLenum srBindBuffer(DDIGL_Context *ctx, GLenum target, DDIGL_Buffer *buffer)
{
	return GL_NO_ERROR;
};

GLenum srBufferData(DDIGL_Context *ctx, DDIGL_Buffer *buffer, GLsizeiptr size, const GLvoid *data, GLenum usage)
{
	void *newData = malloc(size);
	if (newData == NULL)
	{
		return GL_OUT_OF_MEMORY;
	};
	
	free(buffer->data);
	buffer->data = newData;
	buffer->size = size;
	
	return GL_NO_ERROR;
};

void srDeleteBuffer(DDIGL_Context *ctx, DDIGL_Buffer *buffer)
{
	free(buffer->data);
	free(buffer);
};
