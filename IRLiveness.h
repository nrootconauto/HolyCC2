#pragma once
#include "IR.h"
#include "basicBlocks.h"
#include "ptrMap.h"
struct IRVarLiveness {
	struct IRVar ref;
};
GRAPH_TYPE_DEF(struct IRVarLiveness, void *, IRLive);
GRAPH_TYPE_FUNCS(struct IRVarLiveness, void *, IRLive);
struct IRAttrBasicBlock {
	struct IRAttr base;
	struct basicBlock *block;
};
graphNodeIRLive IRInterferenceGraph(graphNodeIR start);
strGraphNodeIRLiveP IRInterferenceGraphFilter(graphNodeIR start, const void *data, int (*varFilter)(graphNodeIR node, const void *data));
