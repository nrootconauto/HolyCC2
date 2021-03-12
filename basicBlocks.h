#pragma once
#include "IR.h"
struct blockMetaNode {
	graphNodeMapping node;
	struct basicBlock *block;
};
STR_TYPE_DEF(struct IRVar, Var);
STR_TYPE_FUNCS(struct IRVar, Var);
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
PTR_MAP_FUNCS(struct __graphNode *, struct blockMetaNode, BlockMetaNode);
strBasicBlock IRGetBasicBlocksFromExpr(graphNodeIR dontDestroy, ptrMapBlockMetaNode metaNodes, graphNodeMapping start, strGraphNodeMappingP *consumedNodes,
                                       const void *data, int (*varFilter)(graphNodeIR var, const void *data));
