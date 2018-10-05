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

void glBufferData(GLenum target, GLsizeiptr size, const GLvoid *data, GLenum usage)
{
	// 'usage' values are a simple range, so we can easily check it
	if (usage < GL_STREAM_DRAW || usage > GL_DYNAMIC_COPY)
	{
		ddiglSetError(GL_INVALID_ENUM);
		return;
	};
	
	if (!ddiglIsBufferTarget(target))
	{
		ddiglSetError(GL_INVALID_ENUM);
		return;
	};
	
	if (size < 0)
	{
		ddiglSetError(GL_INVALID_VALUE);
		return;
	};
	
	DDIGL_Buffer *buffer = ddiglGetBuffer(target);
	if (buffer == NULL)
	{
		ddiglSetError(GL_INVALID_OPERATION);
		return;
	};
	
	if (__ddigl_current->bufferData == NULL)
	{
		ddiglSetError(GL_INVALID_OPERATION);
		return;
	};
	
	GLenum error = __ddigl_current->bufferData(__ddigl_current, buffer, size, data, usage);
	if (error != GL_NO_ERROR)
	{
		ddiglSetError(error);
	};
};
