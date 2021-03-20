#pragma once
#include "IR.h"
PTR_MAP_FUNCS(struct parserVar *, long, FrameOffset);
void IRComputeFrameLayout(graphNodeIR start, long *frameSize,ptrMapFrameOffset *offsets);
