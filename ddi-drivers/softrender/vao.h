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

#ifndef VAO_H_
#define VAO_H_

#include <GL/ddigl.h>

#include "buffer.h"

/**
 * VAO attribute specification.
 */
typedef struct
{
	/**
	 * Buffer from which to take this attribute.
	 */
	DDIGL_Buffer *buffer;
	
	/**
	 * Parameters supplied through attribPointer().
	 */
	GLint size;
	GLenum type;
	GLboolean normalized;
	GLsizei stride;
	GLsizeiptr offset;
} SRVertexAttrib;

/**
 * Definition of DDIGL_VertexArray for softrender.
 */
struct __ddigl_vao
{
	/**
	 * Bitset defining which attributes are enabled.
	 */
	uint64_t enable;
	
	/**
	 * Attribute settings.
	 */
	SRVertexAttrib attribs[64];
};

/**
 * Create a new softrender vertex array.
 */
DDIGL_VertexArray* srCreateVertexArray(DDIGL_Context *ctx, GLenum *error);

/**
 * Bind a vertex array.
 */
GLenum srBindVertexArray(DDIGL_Context *ctx, DDIGL_VertexArray *vao);

/**
 * Delete a vertex array.
 */
void srDeleteVertexArray(DDIGL_Context *ctx, DDIGL_VertexArray *vao);

/**
 * Set whether an attribute in a vertex array is enabled.
 */
GLenum srSetAttribEnable(DDIGL_Context *ctx, GLuint index, GLboolean enable);

/**
 * Set an attribute pointer.
 */
GLenum srAttribPointer(DDIGL_Context *ctx, GLuint index, GLint size, GLenum type, GLboolean normalized,
				GLsizei stride, GLsizeiptr offset);

#endif
