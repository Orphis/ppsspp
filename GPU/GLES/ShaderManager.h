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

#pragma once

#include "base/basictypes.h"
#include "../../Globals.h"
#include <map>
#include "VertexShaderGenerator.h"
#include "FragmentShaderGenerator.h"

struct Shader;

struct LinkedShader
{
	LinkedShader(Shader *vs, Shader *fs);
	~LinkedShader();

	void use();

	uint32_t program;
	u32 dirtyUniforms;

	// Pre-fetched attrs and uniforms
	int a_position;
	int a_color0;
	int a_color1;
	int a_texcoord;
	// int a_blendWeight0123;
	// int a_blendWeight4567;

	int u_tex;
	int u_proj;
	int u_proj_through;
	int u_texenv;

	// Fragment processing inputs
	int u_alpharef;
	int u_fogcolor;
	int u_fogcoef;

	// Lighting
	int u_ambientcolor;
	int u_light[4];  // each light consist of vec4[3]
};

// Will reach 32 bits soon :P
enum
{
	DIRTY_PROJMATRIX = (1 << 0),
	DIRTY_PROJTHROUGHMATRIX = (1 << 1),
	DIRTY_FOGCOLOR	 = (1 << 2),
	DIRTY_FOGCOEF    = (1 << 3),
	DIRTY_TEXENV		 = (1 << 4),
	DIRTY_ALPHAREF	 = (1 << 5),
	DIRTY_COLORREF	 = (1 << 6),

	DIRTY_LIGHT0 = (1 << 12),
	DIRTY_LIGHT1 = (1 << 13),
	DIRTY_LIGHT2 = (1 << 14),
	DIRTY_LIGHT3 = (1 << 15),

	DIRTY_GLOBALAMBIENT = (1 << 16),
	DIRTY_MATERIAL = (1 << 17),  // let's set all 4 together (emissive ambient diffuse specular). We hide specular coef in specular.a
	DIRTY_UVSCALEOFFSET = (1 << 18),  // this will be dirtied ALL THE TIME... maybe we'll need to do "last value with this shader compares"

	DIRTY_VIEWMATRIX = (1 << 22),  // Maybe we'll fold this into projmatrix eventually
	DIRTY_TEXMATRIX = (1 << 23),
	DIRTY_BONEMATRIX0 = (1 << 24),
	DIRTY_BONEMATRIX1 = (1 << 25),
	DIRTY_BONEMATRIX2 = (1 << 26),
	DIRTY_BONEMATRIX3 = (1 << 27),
	DIRTY_BONEMATRIX4 = (1 << 28),
	DIRTY_BONEMATRIX5 = (1 << 29),
	DIRTY_BONEMATRIX6 = (1 << 30),
	DIRTY_BONEMATRIX7 = (1 << 31),

	DIRTY_ALL = 0xFFFFFFFF
};

// Real public interface

struct Shader
{
	Shader(const char *code, uint32_t shaderType);
	uint32_t shader;
	const std::string &source() const { return source_; }
private:
	std::string source_;
};


class ShaderManager
{
public:
	ShaderManager() : globalDirty(0xFFFFFFFF) {}

	void ClearCache(bool deleteThem);  // TODO: deleteThem currently not respected
	LinkedShader *ApplyShader(int prim);
	void DirtyShader();
	void DirtyUniform(u32 what);

	int NumVertexShaders() const { return (int)vsCache.size(); }
	int NumFragmentShaders() const { return (int)fsCache.size(); }
	int NumPrograms() const { return (int)linkedShaderCache.size(); }

private:
	void Clear();

	typedef std::map<std::pair<Shader *, Shader *>, LinkedShader *> LinkedShaderCache;

	LinkedShaderCache linkedShaderCache;
	FragmentShaderID lastFSID;
	VertexShaderID lastVSID;

	LinkedShader *lastShader;
	u32 globalDirty;

	typedef std::map<FragmentShaderID, Shader *> FSCache;
	FSCache fsCache;

	typedef std::map<VertexShaderID, Shader *> VSCache;
	VSCache vsCache;
};
