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

#include "../Globals.h"

#include "MemMap.h"
#include "Core.h"

enum
{
	GPR_SIZE_32=0,
	GPR_SIZE_64=1
};

enum CPUType
{
	CPUTYPE_INVALID=0,
	CPUTYPE_ALLEGREX=1,
	CPUTYPE_ME=2,
};

class CPU
{
private:
	u32 id;
public:
	virtual void SingleStep() = 0;
	virtual int RunLoopUntil(u64 globalTicks) = 0;
};

#define MAX_NUM_CPU 2


extern CPU *currentCPU;
extern int numCPUs;
