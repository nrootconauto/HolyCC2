#pragma once
#include <IR.h>
struct IRVarLiveness {
	struct IRVar ref;
};
GRAPH_TYPE_DEF(struct IRVarLiveness, void *, IRLive);
GRAPH_TYPE_FUNCS(struct IRVarLiveness, void *, IRLive);

extern void *IR_ATTR_BASIC_BLOCK;
struct IRAttrBasicBlock {
	struct IRAttr base;
	struct basicBlock *block;
};
graphNodeIRLive IRInterferenceGraph(graphNodeIR start);
strGraphNodeIRLiveP
IRInterferenceGraphFilter(graphNodeIR start, const void *data,
                          int (*varFilter)(graphNodeIR node, const void *data));
STR_TYPE_DEF(struct IRVar *, Var);
STR_TYPE_FUNCS(struct IRVar *, Var);
struct basicBlock {
	long refCount;
	strGraphNodeMappingP nodes;
	strVar read;
	strVar define;
	strVar in;
	strVar out;
};
STR_TYPE_DEF(struct basicBlock *, BasicBlock);
STR_TYPE_FUNCS(struct basicBlock *, BasicBlock);
void IRLivenessRemoveBasicBlockAttrs(graphNodeIR node);
