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

#include "context.h"
#include "draw.h"
#include "vao.h"

#include <stdio.h>

static int srMin3(int a, int b, int c)
{
	if (a > b) a = b;
	if (a > c) a = c;
	return a;
};

static int srMax3(int a, int b, int c)
{
	if (a < b) a = b;
	if (a < c) a = c;
	return a;
};

static void srDrawTriangle(SRContext *srctx, SRVertex *a, SRVertex *b, SRVertex *c)
{
	// convert to framebuffer coordinates
	int ax = srctx->viewX + srctx->viewW * (0.5f * (a->pos[0] + 1.0f));
	int bx = srctx->viewX + srctx->viewW * (0.5f * (b->pos[0] + 1.0f));
	int cx = srctx->viewX + srctx->viewW * (0.5f * (c->pos[0] + 1.0f));
	
	int ay = srctx->viewY + srctx->viewH * (0.5f * (a->pos[1] + 1.0f));
	int by = srctx->viewY + srctx->viewH * (0.5f * (b->pos[1] + 1.0f));
	int cy = srctx->viewY + srctx->viewH * (0.5f * (c->pos[1] + 1.0f));
	
	// calculate the bounding box
	int minX = srMin3(ax, bx, cx);
	int maxX = srMax3(ax, bx, cx);
	int minY = srMin3(ay, by, cy);
	int maxY = srMax3(ay, by, cy);
	
	// create vectors
	SRVec2D posA = {ax, ay};
	SRVec2D posB = {bx, by};
	SRVec2D posC = {cx, by};
	
	// 2 times the area of the triangle
	SRVec2D edge1 = posB - posA;
	SRVec2D edge2 = posC - posA;
	int areaTotal = edge1[0] * edge2[1] - edge1[1] * edge2[0];
	
	// rasterize
	int x, y;
	for (y=minY; y<maxY; y++)
	{
		if (y < 0 || y > srctx->currentBuffer->color[0]->height) continue;
		
		for (x=minX; x<maxX; x++)
		{
			if (x < 0 || x > srctx->currentBuffer->color[0]->width) continue;
			
			SRVec2D posFrag = {x, y};
			
			edge1 = posB - posFrag;
			edge2 = posC - posFrag;
			int areaA = edge1[0] * edge2[1] - edge1[1] * edge2[0];
			
			edge1 = posC - posFrag;
			edge2 = posA - posFrag;
			int areaB = edge1[0] * edge2[1] - edge1[1] * edge2[0];
			
			edge1 = posA - posFrag;
			edge2 = posB - posFrag;
			int areaC = edge1[0] * edge2[1] - edge1[1] * edge2[0];
			
			// barycentric coordinates
			float baryA = (float) areaA / (float) areaTotal;
			float baryB = (float) areaB / (float) areaTotal;
			float baryC = (float) areaC / (float) areaTotal;
			
			// make sure the point lies inside the triangle, else continue
			if (baryA < 0.0 || baryB < 0.0 || baryC < 0.0) continue;
			
			// form the bary vector
			SRVector bary = {baryA, baryB, baryC, 0.0};
			
			// ask the fragment shader for colors
			// TODO: uniforms
			SRVector colors[8];
			srctx->currentPipeline->fragmentShader(NULL, a->data, b->data, c->data, colors, bary);
			
			// write to color buffers
			// TODO: all the other processing of fragments
			int i;
			for (i=0; i<8; i++)
			{
				SRColorBuffer *cbuf = srctx->currentBuffer->color[i];
				if (cbuf != NULL)
				{
					// calculate pixel value
					uint32_t redPart = (uint32_t) (255 * colors[i][0]) << srctx->redShift;
					uint32_t greenPart = (uint32_t) (255 * colors[i][1]) << srctx->greenShift;
					uint32_t bluePart = (uint32_t) (255 * colors[i][2]) << srctx->blueShift;
					uint32_t alphaPart = (uint32_t) (255 * colors[i][3]) << srctx->alphaShift;
					uint32_t pixel = redPart | greenPart | bluePart | alphaPart;
					
					// store it
					uint32_t *put = (uint32_t*) cbuf->data;
					put[(cbuf->height-y-1) * cbuf->width + x] = pixel;
				};
			};
		};
	};
};

static float srFetchFloat(GLenum typeFetch, const void **fetch)
{
	switch (typeFetch)
	{
	// TODO: other types
	case GL_FLOAT:
		{
			const float *ffetch = (const float*) (*fetch);
			*fetch = (ffetch+1);
			return *ffetch;
		};
	default:
		return 0.0f;
	};
};

static void srFetchData(GLenum typePut, void **put, GLenum typeFetch, const void **fetch)
{
	switch (typePut)
	{
	// TODO: other types
	case GL_FLOAT:
		{
			float *fput = (float*) (*put);
			*put = (fput+1);
			*fput = srFetchFloat(typeFetch, fetch);
		};
		break;
	};
};

static SRVertex* srFetchVertex(DDIGL_Pipeline *pl, DDIGL_VertexArray *vao, GLint vertexIndex)
{
	// load the vertex input structure
	char vin[pl->szVertex];
	memset(vin, 0, pl->szVertex);
	
	int i;
	for (i=0; i<64; i++)
	{
		if (pl->attribs[i].type != 0 && (vao->enable & (1UL << i)))
		{
			// TODO: handle GL_BGRA properly
			
			SRVarDef *def = &pl->attribs[i];
			SRVertexAttrib *attr = &vao->attribs[i];
			
			void *put = vin + def->offset;
			const void *fetch = (const char*) attr->buffer->data
				+ attr->stride * vertexIndex
				+ attr->offset;
			
			size_t num = def->size;
			if (num > attr->size) num = attr->size;
			
			while (num--)
			{
				srFetchData(def->type, &put, attr->type, &fetch);
			};
		};
	};
	
	// allocate output structure
	SRVertex *vout = (SRVertex*) malloc(sizeof(SRVertex) + pl->szVertexFragment);
	memset(vout->data, 0, pl->szVertexFragment);
	
	// execute vertex shader
	// TODO: uniforms
	vout->pos = pl->vertexShader(NULL, vin, vout->data);
	
	// thanks
	return vout;
};

static SRVertex *srFetchVertexEx(DDIGL_Pipeline *pl, DDIGL_VertexArray *vao, GLint vertexIndex, GLint *indices)
{
	if (indices == NULL) return srFetchVertex(pl, vao, vertexIndex);
	else return srFetchVertex(pl, vao, indices[vertexIndex]);
};

static GLenum srDrawGeneric(DDIGL_Context *ctx, GLenum mode, GLint first, GLsizei count, GLint *indices)
{
	SRContext *srctx = (SRContext*) ctx->drvdata;
	DDIGL_Pipeline *pl = srctx->currentPipeline;
	DDIGL_VertexArray *vao = ctx->vaoCurrent;
	
	GLint i;
	//SRVertex *ref;
	SRVertex *a;
	SRVertex *b;
	SRVertex *c;
	
	switch (mode)
	{
	case GL_TRIANGLES:
		count -= (count % 3);
		
		for (i=first; i<first+count; i+=3)
		{
			a = srFetchVertexEx(pl, vao, i, indices);
			b = srFetchVertexEx(pl, vao, i+1, indices);
			c = srFetchVertexEx(pl, vao, i+2, indices);
			srDrawTriangle(srctx, a, b, c);
		};
		
		break;
	// TODO: the other modes
	default:
		return GL_INVALID_ENUM;
	};
	
	return GL_NO_ERROR;
};

GLenum srDrawArrays(DDIGL_Context *ctx, GLenum mode, GLint first, GLsizei count)
{
	return srDrawGeneric(ctx, mode, first, count, NULL);
};
