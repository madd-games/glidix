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

#ifndef __GL_H
#define __GL_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Define GLAPIENTRY and stuff.
 */

#ifndef GLAPI
#define GLAPI extern
#endif

#ifndef GLAPIENTRY
#define GLAPIENTRY
#endif

#ifndef APIENTRY
#define APIENTRY GLAPIENTRY
#endif

#ifndef APIENTRYP
#define APIENTRYP APIENTRY *
#endif

#ifndef GLAPIENTRYP
#define GLAPIENTRYP GLAPIENTRY *
#endif

/**
 * Booleans.
 */
#define	GL_TRUE					(1 == 1)
#define	GL_FALSE				(1 == 0)

/**
 * Attribute bits for glClear() etc.
 */
#define GL_CURRENT_BIT				0x00000001
#define GL_POINT_BIT				0x00000002
#define GL_LINE_BIT				0x00000004
#define GL_POLYGON_BIT				0x00000008
#define GL_POLYGON_STIPPLE_BIT			0x00000010
#define GL_PIXEL_MODE_BIT			0x00000020
#define GL_LIGHTING_BIT				0x00000040
#define GL_FOG_BIT				0x00000080
#define GL_DEPTH_BUFFER_BIT			0x00000100
#define GL_ACCUM_BUFFER_BIT			0x00000200
#define GL_STENCIL_BUFFER_BIT			0x00000400
#define GL_VIEWPORT_BIT				0x00000800
#define GL_TRANSFORM_BIT			0x00001000
#define GL_ENABLE_BIT				0x00002000
#define GL_COLOR_BUFFER_BIT			0x00004000
#define GL_HINT_BIT				0x00008000
#define GL_EVAL_BIT				0x00010000
#define GL_LIST_BIT				0x00020000
#define GL_TEXTURE_BIT				0x00040000
#define GL_SCISSOR_BIT				0x00080000
#define GL_ALL_ATTRIB_BITS			0xFFFFFFFF

/**
 * Errors.
 */
#define GL_NO_ERROR 				0
#define GL_INVALID_ENUM				0x0500
#define GL_INVALID_VALUE			0x0501
#define GL_INVALID_OPERATION			0x0502
#define GL_STACK_OVERFLOW			0x0503
#define GL_STACK_UNDERFLOW			0x0504
#define GL_OUT_OF_MEMORY			0x0505

/**
 * Buffer binding targets.
 */
#define GL_ARRAY_BUFFER				0x8892
#define GL_ATOMIC_COUNTER_BUFFER		0x92C0
#define GL_COPY_READ_BUFFER			0x8F36
#define GL_COPY_WRITE_BUFFER			0x8F37
#define GL_DISPATCH_INDIRECT_BUFFER		0x90EE
#define GL_DRAW_INDIRECT_BUFFER			0x8F3F
#define GL_ELEMENT_ARRAY_BUFFER			0x8893
#define GL_PIXEL_PACK_BUFFER			0x88EB
#define GL_PIXEL_UNPACK_BUFFER			0x88EC
#define GL_QUERY_BUFFER				0x9192
#define GL_SHADER_STORAGE_BUFFER		0x90D2
#define GL_TEXTURE_BUFFER			0x8C2A
#define GL_TRANSFORM_FEEDBACK_BUFFER		0x8C8E
#define GL_UNIFORM_BUFFER			0x8A11

/**
 * Buffer usage values.
 */
#define GL_STREAM_DRAW				0x88E0
#define GL_STREAM_READ				0x88E1
#define GL_STREAM_COPY				0x88E2
#define GL_STATIC_DRAW				0x88E4
#define GL_STATIC_READ				0x88E5
#define GL_STATIC_COPY				0x88E6
#define GL_DYNAMIC_DRAW				0x88E8
#define GL_DYNAMIC_READ				0x88E9
#define GL_DYNAMIC_COPY				0x88EA

/**
 * Types.
 */
typedef unsigned int				GLenum;
typedef unsigned char				GLboolean;
typedef unsigned int				GLbitfield;
typedef void					GLvoid;
typedef signed char				GLbyte;
typedef short					GLshort;
typedef int					GLint;
typedef unsigned char				GLubyte;
typedef unsigned short				GLushort;
typedef unsigned int				GLuint;
typedef int					GLsizei;
typedef float					GLfloat;
typedef float					GLclampf;
typedef double					GLdouble;
typedef double					GLclampd;
typedef long					GLsizeiptr;

/**
 * Error handling.
 */
GLenum glGetError();

/**
 * Buffer clearing.
 */
void glClearDepth(GLclampd depth);
void glClearDepthf(GLclampf depth);
void glClearStencil(GLint s);
void glClearColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha);
void glClear(GLbitfield mask);

/**
 * Flushing.
 */
void glFlush();
void glFinish();

/**
 * Buffers.
 */
void glGenBuffers(GLsizei n, GLuint *buffers);
void glDeleteBuffers(GLsizei n, GLuint *buffers);
void glBindBuffer(GLenum target, GLuint bufname);
void glBufferData(GLenum target, GLsizeiptr size, const GLvoid *data, GLenum usage);

#ifdef __cplusplus
}	/* extern "C" */
#endif

#endif	/* __GL_H */
