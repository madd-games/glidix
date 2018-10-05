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

static void setBinding(GLenum target, DDIGL_Buffer *buffer)
{
	GLenum error = __ddigl_current->bindBuffer(__ddigl_current, target, buffer);
	if (error != GL_NO_ERROR)
	{
		ddiglSetError(error);
		return;
	};
	
	switch (target)
	{
	case GL_ARRAY_BUFFER:
		__ddigl_current->arrayBuffer = buffer;
		break;
	case GL_ATOMIC_COUNTER_BUFFER:
		__ddigl_current->atomicCounterBuffer = buffer;
		break;
	case GL_COPY_READ_BUFFER:
		__ddigl_current->copyReadBuffer = buffer;
		break;
	case GL_COPY_WRITE_BUFFER:
		__ddigl_current->copyWriteBuffer = buffer;
		break;
	case GL_DISPATCH_INDIRECT_BUFFER:
		__ddigl_current->dispatchIndirectBuffer = buffer;
		break;
	case GL_DRAW_INDIRECT_BUFFER:
		__ddigl_current->drawIndirectBuffer = buffer;
		break;
	case GL_ELEMENT_ARRAY_BUFFER:
		__ddigl_current->elementArrayBuffer = buffer;
		break;
	case GL_PIXEL_PACK_BUFFER:
		__ddigl_current->pixelPackBuffer = buffer;
		break;
	case GL_PIXEL_UNPACK_BUFFER:
		__ddigl_current->pixelUnpackBuffer = buffer;
		break;
	case GL_QUERY_BUFFER:
		__ddigl_current->queryBuffer = buffer;
		break;
	case GL_SHADER_STORAGE_BUFFER:
		__ddigl_current->shaderStorageBuffer = buffer;
		break;
	case GL_TEXTURE_BUFFER:
		__ddigl_current->textureBuffer = buffer;
		break;
	case GL_TRANSFORM_FEEDBACK_BUFFER:
		__ddigl_current->transformFeedbackBuffer = buffer;
		break;
	case GL_UNIFORM_BUFFER:
		__ddigl_current->uniformBuffer = buffer;
		break;
	};
};

void glBindBuffer(GLenum target, GLuint bufname)
{
	if (!ddiglIsBufferTarget(target))
	{
		ddiglSetError(GL_INVALID_ENUM);
		return;
	};

	if (__ddigl_current->createBuffer == NULL || __ddigl_current->bindBuffer == NULL)
	{
		ddiglSetError(GL_INVALID_OPERATION);
		return;
	};
	
	if (bufname == 0)
	{
		setBinding(target, NULL);
		return;
	};
	
	DDIGL_Object *obj = ddiglGetObject(bufname);
	if (obj->type == DDIGL_OBJ_RESV)
	{
		GLenum error;
		DDIGL_Buffer *newbuf = __ddigl_current->createBuffer(__ddigl_current, &error);
		if (newbuf == NULL)
		{
			ddiglSetError(error);
			return;
		};
		
		obj->type = DDIGL_OBJ_BUFFER;
		obj->asBuffer = newbuf;
	};
	
	if (obj->type != DDIGL_OBJ_BUFFER)
	{
		ddiglSetError(GL_INVALID_VALUE);
		return;
	};
	
	setBinding(target, obj->asBuffer);
};
