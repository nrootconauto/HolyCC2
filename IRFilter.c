#include <IR.h>
#include <cleanup.h>
static int isFlowNode(enum IRConnType type) {
	switch (type) {
	case IR_CONN_FLOW:
	case IR_CONN_COND_FALSE:
	case IR_CONN_COND_TRUE:
		return 1;
	default:
		return 0;
	}
}
static int isFlowBlockOrStartOrEndPred(const struct __graphNode *node, const void *start) {
	// Check if multiple incoming flows
	strGraphEdgeIRP incoming CLEANUP(strGraphEdgeIRPDestroy) = graphNodeIRIncoming((graphNodeIR)node);
	int flowCount = 0;
	for (long i = 0; i != strGraphEdgeIRPSize(incoming); i++)
		if (isFlowNode(*graphEdgeIRValuePtr(incoming[i])))
			flowCount++;

	if (flowCount > 1)
		return 1;

	// Check if multiple flow out
	strGraphEdgeIRP outgoing CLEANUP(strGraphEdgeIRPDestroy) = graphNodeIROutgoing((graphNodeIR)node);
	flowCount = 0;
	for (long i = 0; i != strGraphEdgeIRPSize(outgoing); i++)
		if (isFlowNode(*graphEdgeIRValuePtr(outgoing[i])))
			flowCount++;
	if (flowCount > 1)
		return 1;

	// Check if start node(data is start)
	if (start == node)
		return 1;

	// Check if end(no outgoing)
	if (strGraphEdgeIRPSize(outgoing) == 0)
		return 1;

	return 0;
}
static void strGraphPathDestroy2(strGraphPath *path) {
	for (long i = 0; i != strGraphPathSize(*path); i++)
		strGraphEdgeIRPDestroy(&path[0][i]);

	strGraphPathDestroy(path);
}
static int ptrPtrCmp(const void *a, const void *b) {
	if (*(void **)a > *(void **)b)
		return 1;
	else if (*(void **)a < *(void **)b)
		return -1;
	else
		return 0;
}
struct assocPairIR2Mapping {
	graphNodeIR ir;
	graphNodeMapping mapping;
};
STR_TYPE_DEF(struct assocPairIR2Mapping, Assoc);
STR_TYPE_FUNCS(struct assocPairIR2Mapping, Assoc);
static int assocCmp(const struct assocPairIR2Mapping *a, const struct assocPairIR2Mapping *b) {
	return ptrPtrCmp(&a->ir, &b->ir);
}
static int assocContainsMapping(const void *node, const struct assocPairIR2Mapping *pair) {
	return pair->mapping == node;
}
typedef int (*gnCmpType)(const graphNodeIR *, const graphNodeIR *);
static void __filterTransparentKill(graphNodeMapping node) {
	strGraphEdgeMappingP in CLEANUP(strGraphEdgeMappingPDestroy) = graphNodeMappingIncoming(node);
	strGraphEdgeMappingP out CLEANUP(strGraphEdgeMappingPDestroy) = graphNodeMappingOutgoing(node);
	for (long i = 0; i != strGraphEdgeMappingPSize(in); i++) {
		for (long o = 0; o != strGraphEdgeMappingPSize(out); o++) {
			if (!graphNodeMappingConnectedTo(graphEdgeMappingIncoming(in[i]), graphEdgeMappingOutgoing(out[o])))
				graphNodeMappingConnect(graphEdgeMappingIncoming(in[i]), graphEdgeMappingOutgoing(out[o]), NULL);
		}
	}
	graphNodeMappingKill(&node, NULL, NULL);
}
graphNodeIR IRFilter(graphNodeIR start, int (*pred)(graphNodeIR, const void *), const void *data) {
	graphNodeMapping mapping = graphNodeCreateMapping(start, 0);
	strGraphNodeMappingP allNodes CLEANUP(strGraphNodeMappingPDestroy) = graphNodeMappingAllNodes(mapping);
	for (long i = 0; i < strGraphNodeMappingPSize(allNodes); i++) {
		if (*graphNodeMappingValuePtr(allNodes[i]) == start)
			continue;
		if (!pred(*graphNodeMappingValuePtr(allNodes[i]), data))
			__filterTransparentKill(allNodes[i]);
	}
	return mapping;
}
