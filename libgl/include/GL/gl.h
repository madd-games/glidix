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

/* Data types */
#define GL_BYTE					0x1400
#define GL_UNSIGNED_BYTE			0x1401
#define GL_SHORT				0x1402
#define GL_UNSIGNED_SHORT			0x1403
#define GL_INT					0x1404
#define GL_UNSIGNED_INT				0x1405
#define GL_FLOAT				0x1406
#define GL_2_BYTES				0x1407
#define GL_3_BYTES				0x1408
#define GL_4_BYTES				0x1409
#define GL_DOUBLE				0x140A

/**
 * Stuff added in OpenGL 1.2
 */
#define GL_RESCALE_NORMAL			0x803A
#define GL_CLAMP_TO_EDGE			0x812F
#define GL_MAX_ELEMENTS_VERTICES		0x80E8
#define GL_MAX_ELEMENTS_INDICES			0x80E9
#define GL_BGR					0x80E0
#define GL_BGRA					0x80E1
#define GL_UNSIGNED_BYTE_3_3_2			0x8032
#define GL_UNSIGNED_BYTE_2_3_3_REV		0x8362
#define GL_UNSIGNED_SHORT_5_6_5			0x8363
#define GL_UNSIGNED_SHORT_5_6_5_REV		0x8364
#define GL_UNSIGNED_SHORT_4_4_4_4		0x8033
#define GL_UNSIGNED_SHORT_4_4_4_4_REV		0x8365
#define GL_UNSIGNED_SHORT_5_5_5_1		0x8034
#define GL_UNSIGNED_SHORT_1_5_5_5_REV		0x8366
#define GL_UNSIGNED_INT_8_8_8_8			0x8035
#define GL_UNSIGNED_INT_8_8_8_8_REV		0x8367
#define GL_UNSIGNED_INT_10_10_10_2		0x8036
#define GL_UNSIGNED_INT_2_10_10_10_REV		0x8368
#define GL_LIGHT_MODEL_COLOR_CONTROL		0x81F8
#define GL_SINGLE_COLOR				0x81F9
#define GL_SEPARATE_SPECULAR_COLOR		0x81FA
#define GL_TEXTURE_MIN_LOD			0x813A
#define GL_TEXTURE_MAX_LOD			0x813B
#define GL_TEXTURE_BASE_LEVEL			0x813C
#define GL_TEXTURE_MAX_LEVEL			0x813D
#define GL_SMOOTH_POINT_SIZE_RANGE		0x0B12
#define GL_SMOOTH_POINT_SIZE_GRANULARITY	0x0B13
#define GL_SMOOTH_LINE_WIDTH_RANGE		0x0B22
#define GL_SMOOTH_LINE_WIDTH_GRANULARITY	0x0B23
#define GL_ALIASED_POINT_SIZE_RANGE		0x846D
#define GL_ALIASED_LINE_WIDTH_RANGE		0x846E
#define GL_PACK_SKIP_IMAGES			0x806B
#define GL_PACK_IMAGE_HEIGHT			0x806C
#define GL_UNPACK_SKIP_IMAGES			0x806D
#define GL_UNPACK_IMAGE_HEIGHT			0x806E
#define GL_TEXTURE_3D				0x806F
#define GL_PROXY_TEXTURE_3D			0x8070
#define GL_TEXTURE_DEPTH			0x8071
#define GL_TEXTURE_WRAP_R			0x8072
#define GL_MAX_3D_TEXTURE_SIZE			0x8073
#define GL_TEXTURE_BINDING_3D			0x806A

/**
 * Primitive modes.
 */
#define GL_POINTS				0x0000
#define GL_LINES				0x0001
#define GL_LINE_LOOP				0x0002
#define GL_LINE_STRIP				0x0003
#define GL_TRIANGLES				0x0004
#define GL_TRIANGLE_STRIP			0x0005
#define GL_TRIANGLE_FAN				0x0006
#define GL_QUADS				0x0007
#define GL_QUAD_STRIP				0x0008
#define GL_POLYGON				0x0009
#define GL_LINES_ADJACENCY			0x000A
#define GL_LINE_STRIP_ADJACENCY			0x000B
#define GL_TRIANGLES_ADJACENCY			0x000C
#define GL_TRIANGLE_STRIP_ADJACENCY		0x000D
#define GL_PATCHES				0x000E

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
 * Viewport.
 */
void glViewport(GLint x, GLint y, GLsizei w, GLsizei h);

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

/**
 * Vertex array objects (VAOs).
 */
void glGenVertexArrays(GLsizei n, GLuint *arrays);
void glBindVertexArray(GLuint vao);
void glDeleteVertexArrays(GLsizei n, GLuint *arrays);
void glEnableVertexAttribArray(GLuint index);
void glDisableVertexAttribArray(GLuint index);
void glVertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid *pointer);

/**
 * Drawing.
 */
void glDrawArrays(GLenum mode, GLint first, GLsizei count);

#ifdef __cplusplus
}	/* extern "C" */
#endif

#endif	/* __GL_H */
