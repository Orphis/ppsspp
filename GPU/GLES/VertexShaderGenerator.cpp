// Copyright (c) 2012- PPSSPP Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0 or later versions.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official git repository and contact information can be found at
// https://github.com/hrydgard/ppsspp and http://www.ppsspp.org/.

// TODO: We should transition from doing the transform in software, as seen in TransformPipeline.cpp,
// into doing the transform in the vertex shader - except for Rectangles, there we really need to do
// the transforms ourselves.

#include <stdio.h>

#include "../ge_constants.h"
#include "../GPUState.h"

#include "VertexShaderGenerator.h"

// SDL 1.2 on Apple does not have support for OpenGL 3 and hence needs
// special treatment in the shader generator.
#ifdef __APPLE__
#define FORCE_OPENGL_2_0
#endif

#undef WRITE

static char buffer[16384];

#define WRITE(x, ...) p+=sprintf(p, x "\n" __VA_ARGS__)

// prim so we can special case for RECTANGLES :(
void ComputeVertexShaderID(VertexShaderID *id, int prim)
{
	int doTexture = (gstate.textureMapEnable & 1) && !(gstate.clearmode & 1);

	memset(id->d, 0, sizeof(id->d));
	id->d[0] = gstate.lmode & 1;
	id->d[0] |= ((int)gstate.isModeThrough()) << 1;
	id->d[0] |= ((int)gstate.isFogEnabled()) << 2;
	id->d[0] |= doTexture << 3;

	// Bits that we will need:
	// lightenable * 4
	// lighttype * 4
	// lightcomp * 4
	// uv gen:
	// mapping type
	// texshade light choices (ONLY IF uv mapping type is shade)
}

void WriteLight(char *p, int l) {
	// TODO
}

char *GenerateVertexShader()
{
	char *p = buffer;
#if defined(USING_GLES2)
	WRITE("precision highp float;");
#elif !defined(FORCE_OPENGL_2_0)
	WRITE("#version 130");
#endif

	int lmode = gstate.lmode & 1;

	int doTexture = (gstate.textureMapEnable & 1) && !(gstate.clearmode & 1);

	WRITE("attribute vec3 a_position;");
	if (doTexture)
		WRITE("attribute vec2 a_texcoord;");
	WRITE("attribute vec4 a_color0;");
	if (lmode)
		WRITE("attribute vec4 a_color1;");

	if (gstate.isModeThrough())	{
		WRITE("uniform mat4 u_proj_through;");
	} else {
		WRITE("uniform mat4 u_proj;");
		// Add all the uniforms we'll need to transform properly.
	}

	WRITE("varying vec4 v_color0;");
	if (lmode)
		WRITE("varying vec4 v_color1;");
	if (doTexture)
		WRITE("varying vec2 v_texcoord;");
	if (gstate.isFogEnabled())
		WRITE("varying float v_depth;");
	WRITE("void main() {");
	WRITE("  v_color0 = a_color0;");
	if (lmode)
		WRITE("  v_color1 = a_color1;");
	if (doTexture)
		WRITE("  v_texcoord = a_texcoord;");
	if (gstate.isModeThrough())	{
		WRITE("  gl_Position = u_proj_through * vec4(a_position, 1.0);");
	} else {
		WRITE("  gl_Position = u_proj * vec4(a_position, 1.0);");
	}
	if (gstate.isFogEnabled()) {
		WRITE("  v_depth = gl_Position.z;");
	}
	WRITE("}");

	return buffer;
}

