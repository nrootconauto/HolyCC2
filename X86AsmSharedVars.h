#pragma once
#include "frameLayout.h"
#include "basicBlocks.h"
#include "IR.h"
PTR_MAP_FUNCS(struct parserFunction*, char *, AsmFuncName)
extern __thread ptrMapFrameOffset localVarFrameOffsets;
extern __thread ptrMapAsmFuncName asmFuncNames;
extern __thread strVar asmFuncArgVars;
extern long bytesOnStack;
