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

static void deleteBuffer(DDIGL_Buffer *buffer)
{
	if (__ddigl_current->arrayBuffer == buffer) __ddigl_current->arrayBuffer = NULL;
	if (__ddigl_current->atomicCounterBuffer == buffer) __ddigl_current->atomicCounterBuffer = NULL;
	if (__ddigl_current->copyReadBuffer == buffer) __ddigl_current->copyReadBuffer = NULL;
	if (__ddigl_current->copyWriteBuffer == buffer) __ddigl_current->copyWriteBuffer = NULL;
	if (__ddigl_current->dispatchIndirectBuffer == buffer) __ddigl_current->dispatchIndirectBuffer = NULL;
	if (__ddigl_current->drawIndirectBuffer == buffer) __ddigl_current->drawIndirectBuffer = NULL;
	if (__ddigl_current->elementArrayBuffer == buffer) __ddigl_current->elementArrayBuffer = NULL;
	if (__ddigl_current->pixelPackBuffer == buffer) __ddigl_current->pixelPackBuffer = NULL;
	if (__ddigl_current->pixelUnpackBuffer == buffer) __ddigl_current->pixelUnpackBuffer = NULL;
	if (__ddigl_current->queryBuffer == buffer) __ddigl_current->queryBuffer = NULL;
	if (__ddigl_current->shaderStorageBuffer == buffer) __ddigl_current->shaderStorageBuffer = NULL;
	if (__ddigl_current->textureBuffer == buffer) __ddigl_current->textureBuffer = NULL;
	if (__ddigl_current->transformFeedbackBuffer == buffer) __ddigl_current->transformFeedbackBuffer = NULL;
	if (__ddigl_current->uniformBuffer == buffer) __ddigl_current->uniformBuffer = NULL;
	
	if (__ddigl_current->deleteBuffer != NULL)
	{
		__ddigl_current->deleteBuffer(__ddigl_current, buffer);
	};
};

static void deleteBufferName(GLuint name)
{
	DDIGL_Object *obj = ddiglGetObject(name);
	if (obj->type == DDIGL_OBJ_RESV)
	{
		obj->type = DDIGL_OBJ_FREE;
	}
	else if (obj->type == DDIGL_OBJ_BUFFER)
	{
		deleteBuffer(obj->asBuffer);
		memset(obj, 0, sizeof(DDIGL_Object));
	};
};

void glDeleteBuffers(GLsizei n, GLuint *buffers)
{
	while (n--)
	{
		deleteBufferName(*buffers++);
	};
};
