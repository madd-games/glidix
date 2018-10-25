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

#include "pipeline.h"

/**
 * Default vertex shader: output the vertex position and do nothing else.
 */
static SRVector srDefaultVertexShader(const void *uniforms, const void *inputs, void *outputs)
{
	return *((SRVector*)inputs);
};

/**
 * Default fragment shader: output white.
 */
static void srDefaultFragmentShader(const void *uniforms, const void *inputs1, const void *inputs2, const void *inputs3, SRVector *outputs, SRVector bary)
{
	static SRVector white = {1.0, 1.0, 1.0, 1.0};
	outputs[0] = white;
};

/**
 * Default pipeline. This is use if no shader program was set yet.
 */
DDIGL_Pipeline srDefaultPipeline = {
	// no memory-mapped area, all functions are static
	.ptr = NULL,
	.size = 0,
	
	// vertex shader input is 16 bytes long (position)
	// everything else is size 0
	.szVertex = 16,
	
	// shaders
	.vertexShader = srDefaultVertexShader,
	.fragmentShader = srDefaultFragmentShader,
	
	// one input attribute: the vertex position itself (vec4)
	.attribs = {
		{GL_FLOAT, 4, 0}
	},
};
