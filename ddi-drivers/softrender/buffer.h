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

#ifndef BUFFER_H_
#define BUFFER_H_

#include <GL/ddigl.h>

/**
 * Define the DDIGL_Buffer object for softrender.
 */
struct __ddigl_buffer
{
	/**
	 * Buffer data pointer (may be NULL if size is zero).
	 */
	void*						data;
	
	/**
	 * Size of buffer.
	 */
	size_t						size;
};

/**
 * Create a new buffer object.
 */
DDIGL_Buffer* srCreateBuffer(DDIGL_Context *ctx, GLenum *error);

/**
 * Bind a buffer to a target. This is actually a no-op, since we can just use the binding values in DDIGL_Context
 * instead.
 */
GLenum srBindBuffer(DDIGL_Context *ctx, GLenum target, DDIGL_Buffer *buffer);

/**
 * Change the data store in a buffer.
 */
GLenum srBufferData(DDIGL_Context *ctx, DDIGL_Buffer *buffer, GLsizeiptr size, const GLvoid *data, GLenum usage);

/**
 * Delete a buffer.
 */
void srDeleteBuffer(DDIGL_Context *ctx, DDIGL_Buffer *buffer);

#endif
