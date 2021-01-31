#pragma once
#include <IR.h>
struct frameEntry {
	long offset;
	struct IRVar var;
};
STR_TYPE_DEF(struct frameEntry, FrameEntry);
STR_TYPE_FUNCS(struct frameEntry, FrameEntry);
strFrameEntry IRComputeFrameLayout(graphNodeIR start, long *frameSize);
