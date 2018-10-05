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

DDIGL_Object* ddiglGetObject(GLuint name)
{
	DDIGL_ObjNode *node = &__ddigl_current->objRoot;
	
	union
	{
		unsigned char bytes[4];
		GLuint beval;
	} vals;
	
	vals.beval = __builtin_bswap32(name);
	
	int i;
	for (i=0; i<3; i++)
	{
		unsigned char ent = vals.bytes[i];
		if (node->nodes[ent] == NULL)
		{
			DDIGL_ObjNode *newNode = (DDIGL_ObjNode*) malloc(sizeof(DDIGL_ObjNode));
			memset(newNode, 0, sizeof(DDIGL_ObjNode));
			node->nodes[ent] = newNode;
		};
		
		node = node->nodes[ent]; 
	};
	
	unsigned char objent = vals.bytes[3];
	if (node->objs[objent] == NULL)
	{
		DDIGL_Object *obj = (DDIGL_Object*) malloc(sizeof(DDIGL_Object));
		memset(obj, 0, sizeof(DDIGL_Object));
		node->objs[objent] = obj;
	};
	
	return node->objs[objent];
};

static GLuint ddiglAllocName()
{
	GLuint cand;
	for (cand=1;;cand++)
	{
		DDIGL_Object *obj = ddiglGetObject(cand);
		if (obj->type == DDIGL_OBJ_FREE)
		{
			obj->type = DDIGL_OBJ_RESV;
			return cand;
		};
	};
};

void ddiglGenObjects(GLsizei n, GLuint *names)
{
	if (n < 0)
	{
		ddiglSetError(GL_INVALID_VALUE);
		return;
	};
	
	while (n--)
	{
		*names++ = ddiglAllocName();
	};
};
