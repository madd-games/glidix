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

static int getTypeSize(GLenum type)
{
	switch (type)
	{
	case GL_BYTE:
		return 1;
	case GL_UNSIGNED_BYTE:
		return 1;
	case GL_SHORT:
		return 2;
	case GL_UNSIGNED_SHORT:
		return 2;
	case GL_INT:
		return 4;
	case GL_UNSIGNED_INT:
		return 4;
	case GL_FLOAT:
		return 4;
	case GL_2_BYTES:
		return 2;
	case GL_3_BYTES:
		return 3;
	case GL_4_BYTES:
		return 4;
	case GL_DOUBLE:
		return 8;
	default:
		return -1;
	};
};

void glVertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid *pointer)
{
	if (__ddigl_current->attribPointer == NULL)
	{
		ddiglSetError(GL_INVALID_OPERATION);
		return;
	};
	
	int typeSize = getTypeSize(type);
	if (typeSize == -1)
	{
		ddiglSetError(GL_INVALID_ENUM);
		return;
	};
	
	if (size != 1 && size != 2 && size != 3 && size != 4 && size != GL_BGRA)
	{
		ddiglSetError(GL_INVALID_VALUE);
	};

	if (stride == 0)
	{
		stride = typeSize * size;
	};
	
	GLenum error = __ddigl_current->attribPointer(
			__ddigl_current,
			index, size, type, normalized, stride, (GLsizeiptr) pointer
	);

	if (error != GL_NO_ERROR)
	{
		ddiglSetError(error);
	};
};
