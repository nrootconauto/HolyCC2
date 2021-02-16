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
PTR_MAP_FUNCS(graphNodeIR, int, Visited);
static __thread ptrMapGraphNode mappedNodes=NULL;
static int __IRFilter(graphNodeMapping parent,ptrMapVisited *visited,graphNodeIR node,graphNodeIR endNode) {
		int hit=0;
		
		strGraphNodeIRP queue CLEANUP(strGraphNodeIRPDestroy)=strGraphNodeIRPAppendItem(NULL, node);
		while(strGraphNodeIRPSize(queue)) {
				graphNodeIR curr;
				queue=strGraphNodeIRPPop(queue, &curr);

				if(ptrMapVisitedGet(*visited, curr))
						continue;
				ptrMapVisitedAdd(*visited, curr, 1);
				
				if(ptrMapGraphNodeGet(mappedNodes, curr)) {
						if(!graphNodeMappingConnectedTo(parent, *ptrMapGraphNodeGet(mappedNodes, curr)))
								graphNodeMappingConnect(parent, *ptrMapGraphNodeGet(mappedNodes, curr), NULL);
						hit=1;
						continue;
				}

				if(curr==endNode)
						continue;
				
				strGraphNodeIRP out CLEANUP(strGraphNodeIRPDestroy)=graphNodeIROutgoingNodes(curr);
				int hitInExpr=0;
				for(long o=0;o!=strGraphNodeIRPSize(out);o++) {
						if(!ptrMapVisitedGet(*visited, out[o])) {
								__auto_type end=IREndOfExpr(out[o]);
								if(end&&!endNode) {
										if(end!=out[o]) {
														strGraphNodeIRP stmtNodes CLEANUP(strGraphNodeIRPDestroy)=IRStmtNodes(end);
												int hasSelectInStmt=0;
												int startsInStmt=0;
												for(long s=0;s!=strGraphNodeIRPSize(stmtNodes);s++) {
														if(ptrMapGraphNodeGet(mappedNodes, stmtNodes[s])) {
																if(*ptrMapGraphNodeGet(mappedNodes, stmtNodes[s])==parent)
																		startsInStmt=1;
																hasSelectInStmt=1;
														}
												}
												if(!startsInStmt) {
														if(hasSelectInStmt) {
																__IRFilter(parent, visited, out[o], end);
																continue;
														}
												}
										}
										queue=strGraphNodeIRPAppendItem(queue, end);
										continue;
								}
						}
						queue=strGraphNodeIRPAppendItem(queue, out[o]);
				}
		}
		return hit;
}
/*
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
*/
graphNodeIR IRFilter(graphNodeIR start, int (*pred)(graphNodeIR, const void *), const void *data) {
		graphNodeMapping mapping = graphNodeMappingCreate(start, 0);
		mappedNodes=ptrMapGraphNodeCreate();
		strGraphNodeIRP allNodes CLEANUP(strGraphNodeIRPDestroy)=graphNodeIRAllNodes(start);
		for(long n=0;n!=strGraphNodeIRPSize(allNodes);n++) {
				if(pred(allNodes[n],data)||allNodes[n]==start) {
						ptrMapGraphNodeAdd(mappedNodes, allNodes[n], graphNodeMappingCreate(allNodes[ n],0));
				}
		}

		long kCount=ptrMapGraphNodeSize(mappedNodes);
		graphNodeIR keys[kCount];
		ptrMapGraphNodeKeys(mappedNodes, keys);
		for(long k=0;k!=kCount;k++) {
				__auto_type visited=ptrMapVisitedCreate();
				strGraphNodeIRP out CLEANUP(strGraphNodeIRPDestroy)=graphNodeIROutgoingNodes(keys[k]);
				for(long o=0;o!=strGraphNodeIRPSize(out);o++)
						__IRFilter(*ptrMapGraphNodeGet(mappedNodes, keys[k]), &visited, out[o], NULL);
				ptrMapVisitedDestroy(visited, NULL);
		}

		__auto_type retVal=*ptrMapGraphNodeGet(mappedNodes, start);
		ptrMapGraphNodeDestroy(mappedNodes, NULL);
		return retVal;
}
