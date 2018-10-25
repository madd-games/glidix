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

#ifndef __DDIGL_H
#define __DDIGL_H

#include <GL/gl.h>
#include <libddi.h>

/**
 * Opaque structure for GL buffer driver data. Pointers to this are used throughout the library,
 * and a driver may actually define the structure.
 */
typedef struct __ddigl_buffer DDIGL_Buffer;

/**
 * Opaque structure for GL vertex array (VAO) driver data. Pointers to this are used throughout the library,
 * and a driver may actually define the structure.
 */
typedef struct __ddigl_vao DDIGL_VertexArray;

/**
 * Opaque structure for pipeline objects. A pipeline object is a driver-specific representation of a fully linked
 * shader program. Pointers to this are used throughout the library, and a driver may actually define the structure.
 */
typedef struct __ddigl_pipeline DDIGL_Pipeline;

/**
 * GL object.
 */
#define	DDIGL_OBJ_FREE				0		/* object name unused */
#define	DDIGL_OBJ_RESV				1		/* object name allocated but not yet created */
#define	DDIGL_OBJ_BUFFER			2		/* GL buffer object */
#define	DDIGL_OBJ_VAO				3		/* vertex array object */
typedef struct
{
	/**
	 * Type (one of the above).
	 */
	int					type;
	
	/**
	 * Pointer to the object.
	 */
	union
	{
		DDIGL_Buffer*			asBuffer;
		DDIGL_VertexArray*		asVAO;
	};
} DDIGL_Object;

/**
 * 4-level tree mapping object names to objects.
 */
typedef struct __ddigl_objnode
{
	union
	{
		struct __ddigl_objnode*		nodes[256];
		DDIGL_Object*			objs[256];
	};
} DDIGL_ObjNode;

/**
 * Context parameters structure.
 */
typedef struct __ddigl_params
{
	/**
	 * Number of bits in the stencil buffer (8 or 16). Default 8.
	 */
	int					stencilBits;
	
	/**
	 * Number of bits in the depth buffer (8 or 16). Default 16.
	 */
	int					depthBits;
} DDIGL_ContextParams;

/**
 * Context description.
 */
typedef struct __ddigl_ctx
{
	/**
	 * Driver-specific data.
	 */
	void*					drvdata;
	
	/**
	 * Error flag.
	 */
	GLenum					error;
	
	/**
	 * Set the specified surface as the default render target of this context. This is called
	 * when the context is made current for this surface. Returns GL_NO_ERROR on success, or
	 * an OpenGL error number on failure.
	 */
	GLenum (*setRenderTarget)(struct __ddigl_ctx *ctx, DDISurface *target);
	
	/**
	 * Set the clear color for the specified context.
	 */
	void (*clearColor)(struct __ddigl_ctx *ctx, GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha);
	
	/**
	 * Clear buffers. Return GL_NO_ERROR on success, or a GL error number on error.
	 */
	GLenum (*clear)(struct __ddigl_ctx *ctx, GLbitfield mask);
	
	/**
	 * Block until all previous GL commands have completed.
	 */
	void (*flush)(struct __ddigl_ctx *ctx);
	
	/**
	 * Set the clear depth for the specified context.
	 */
	void (*clearDepth)(struct __ddigl_ctx *ctx, GLclampd depth);
	
	/**
	 * Set the clear stencil value for the specified context.
	 */
	void (*clearStencil)(struct __ddigl_ctx *ctx, GLint s);
	
	/**
	 * Mapping of object names to objects.
	 */
	DDIGL_ObjNode objRoot;
	
	/**
	 * Create a buffer object in the specified context. Return NULL on error and set 'error' to an
	 * OpenGL error number.
	 */
	DDIGL_Buffer* (*createBuffer)(struct __ddigl_ctx *ctx, GLenum *error);
	
	/**
	 * Bind the specified buffer to the specified target. Return GL_NO_ERROR on success, or an error
	 * number on error. If 'buffer' is NULL, remove the binding.
	 */
	GLenum (*bindBuffer)(struct __ddigl_ctx *ctx, GLenum target, DDIGL_Buffer *buffer);
	
	/**
	 * Current buffer bindings.
	 */
	DDIGL_Buffer* arrayBuffer;
	DDIGL_Buffer* atomicCounterBuffer;
	DDIGL_Buffer* copyReadBuffer;
	DDIGL_Buffer* copyWriteBuffer;
	DDIGL_Buffer* dispatchIndirectBuffer;
	DDIGL_Buffer* drawIndirectBuffer;
	DDIGL_Buffer* elementArrayBuffer;
	DDIGL_Buffer* pixelPackBuffer;
	DDIGL_Buffer* pixelUnpackBuffer;
	DDIGL_Buffer* queryBuffer;
	DDIGL_Buffer* shaderStorageBuffer;
	DDIGL_Buffer* textureBuffer;
	DDIGL_Buffer* transformFeedbackBuffer;
	DDIGL_Buffer* uniformBuffer;
	
	/**
	 * Set the buffer store to the specified data and usage. If 'data' is NULL, allocate the store but
	 * leave it uninitialized. Basically implements glBufferData() and glNamedBufferData(). If successful,
	 * return GL_NO_ERROR; otherwise, return an error number.
	 */
	GLenum (*bufferData)(struct __ddigl_ctx *ctx, DDIGL_Buffer *buffer, GLsizeiptr size, const GLvoid *data, GLenum usage);
	
	/**
	 * Delete the specified buffer.
	 */
	void (*deleteBuffer)(struct __ddigl_ctx *ctx, DDIGL_Buffer *buffer);
	
	/**
	 * Create a vertex array object in the specified context. Returns NULL on error and sets 'error' to
	 * a GL error number.
	 */
	DDIGL_VertexArray* (*createVertexArray)(struct __ddigl_ctx *ctx, GLenum *error);
	
	/**
	 * The default (0) VAO and the currently-bound one.
	 */
	DDIGL_VertexArray *vaoDefault;
	DDIGL_VertexArray *vaoCurrent;
	
	/**
	 * Bind the specified VAO. Return GL_NO_ERROR on success, or a GL error number on error.
	 */
	GLenum (*bindVertexArray)(struct __ddigl_ctx *ctx, DDIGL_VertexArray *vao);
	
	/**
	 * Delete the specified VAO.
	 */
	void (*deleteVertexArray)(struct __ddigl_ctx *ctx, DDIGL_VertexArray *vao);
	
	/**
	 * Enable/disable vertex attribute arrays. Return GL_NO_ERROR on success, or a GL error number
	 * on error.
	 */
	GLenum (*setAttribEnable)(struct __ddigl_ctx *ctx, GLuint index, GLboolean enable);
	
	/**
	 * Set the location from which to fetch a vertex attribute. This change shall be applied to the
	 * currently-bound VAO (which, if necessary, can be access via 'ctx->vaoCurrent'). The attribute
	 * values shall be pulled from the currently-bound array buffer ('ctx->arrayBuffer').
	 *
	 * 'index' is the attribute index (location).
	 *
	 * 'type' and 'size' can be any valid combination as defined for glVertexAttribPointer() or its
	 * other 2 versions. 'normalized' is ignored with the other 2 versions (they don't take the value),
	 * and is taken into account when applicable.
	 *
	 * 'stride' is never 0. If a stride of 0 was passed to an API entry point, it will automatically
	 * calculate it.
	 *
	 * 'offset' is the offset into the current array buffer, to the first value of the attribute. The
	 * next value is at 'offset+stride' bytes, and so on.
	 *
	 * Returns GL_NO_ERROR on success, or a GL error number on error.
	 */
	GLenum (*attribPointer)(struct __ddigl_ctx *ctx, GLuint index, GLint size, GLenum type, GLboolean normalized,
				GLsizei stride, GLsizeiptr offset);
				
	/**
	 * Draw the specified range of vertices from the current vertex array, using the specified mode.
	 * Return GL_NO_ERROR on success, or a GL error number on error.
	 */
	GLenum (*drawArrays)(struct __ddigl_ctx *ctx, GLenum mode, GLint first, GLsizei count);
	
	/**
	 * Set the viewport.
	 */
	void (*viewport)(struct __ddigl_ctx *ctx, GLint x, GLint y, GLsizei width, GLsizei height);
} DDIGL_Context;

extern DDIGL_Context* __ddigl_current;

/**
 * Create a context parameters structure.
 */
DDIGL_ContextParams* ddiglCreateContextParams();

/**
 * Destroy a context parameters structure.
 */
void ddiglDestroyContextParams(DDIGL_ContextParams *params);

/**
 * Create a GL context. If 'params' is NULL, then default parameters are used. The selected 'format' must be
 * compatible with the graphics driver in use. Returns the context on success, or NULL on error; and then sets
 * 'error' to a GL error number if it's not NULL.
 */
DDIGL_Context* ddiglCreateContext(DDIPixelFormat *format, DDIGL_ContextParams *params, GLenum *error);

/**
 * Make the specified context and render target current. Returns GL_NO_ERROR on success, or an OpenGL error number
 * on error.
 */
GLenum ddiglMakeCurrent(DDIGL_Context *ctx, DDISurface *target);

/**
 * Set the error flag on the current context.
 */
void ddiglSetError(GLenum error);

/**
 * Convenience function to clamp values.
 */
GLclampf ddiglClampf(GLclampf value, GLclampf minval, GLclampf maxval);

/**
 * Get an object by name. This function never returns NULL; but it might return an object marked as "free" or
 * "reserved".
 */
DDIGL_Object* ddiglGetObject(GLuint name);

/**
 * Generate object names.
 */
void ddiglGenObjects(GLsizei n, GLuint *names);

/**
 * Check if the specified argument is a valid buffer binding target.
 */
int ddiglIsBufferTarget(GLenum target);

/**
 * Return the buffer bound to the specified target; NULL if no binding.
 */
DDIGL_Buffer* ddiglGetBuffer(GLenum target);

#endif
