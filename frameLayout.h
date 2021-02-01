#pragma once
#include <IR.h>
struct frameEntry {
	long offset;
	struct IRVar var;
};
STR_TYPE_DEF(struct frameEntry, FrameEntry);
STR_TYPE_FUNCS(struct frameEntry, FrameEntry);
PTR_MAP_FUNCS(struct parserVar *, long, FrameOffset);
strFrameEntry IRComputeFrameLayout(graphNodeIR start, long *frameSize);
