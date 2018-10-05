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

int ddiglIsBufferTarget(GLenum target)
{
	switch (target)
	{
	case GL_ARRAY_BUFFER:
	case GL_ATOMIC_COUNTER_BUFFER:
	case GL_COPY_READ_BUFFER:
	case GL_COPY_WRITE_BUFFER:
	case GL_DISPATCH_INDIRECT_BUFFER:
	case GL_DRAW_INDIRECT_BUFFER:
	case GL_ELEMENT_ARRAY_BUFFER:
	case GL_PIXEL_PACK_BUFFER:
	case GL_PIXEL_UNPACK_BUFFER:
	case GL_QUERY_BUFFER:
	case GL_SHADER_STORAGE_BUFFER:
	case GL_TEXTURE_BUFFER:
	case GL_TRANSFORM_FEEDBACK_BUFFER:
	case GL_UNIFORM_BUFFER:
		return GL_TRUE;
	default:
		return GL_FALSE;
	};
};

DDIGL_Buffer* ddiglGetBuffer(GLenum target)
{
	switch (target)
	{
	case GL_ARRAY_BUFFER:
		return __ddigl_current->arrayBuffer;
	case GL_ATOMIC_COUNTER_BUFFER:
		return __ddigl_current->atomicCounterBuffer;
	case GL_COPY_READ_BUFFER:
		return __ddigl_current->copyReadBuffer;
	case GL_COPY_WRITE_BUFFER:
		return __ddigl_current->copyWriteBuffer;
	case GL_DISPATCH_INDIRECT_BUFFER:
		return __ddigl_current->dispatchIndirectBuffer;
	case GL_DRAW_INDIRECT_BUFFER:
		return __ddigl_current->drawIndirectBuffer;
	case GL_ELEMENT_ARRAY_BUFFER:
		return __ddigl_current->elementArrayBuffer;
	case GL_PIXEL_PACK_BUFFER:
		return __ddigl_current->pixelPackBuffer;
	case GL_PIXEL_UNPACK_BUFFER:
		return __ddigl_current->pixelUnpackBuffer;
	case GL_QUERY_BUFFER:
		return __ddigl_current->queryBuffer;
	case GL_SHADER_STORAGE_BUFFER:
		return __ddigl_current->shaderStorageBuffer;
	case GL_TEXTURE_BUFFER:
		return __ddigl_current->textureBuffer;
	case GL_TRANSFORM_FEEDBACK_BUFFER:
		return __ddigl_current->transformFeedbackBuffer;
	case GL_UNIFORM_BUFFER:
		return __ddigl_current->uniformBuffer;
	default:
		return NULL;
	};
};
