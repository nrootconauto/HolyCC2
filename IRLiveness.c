#include <IR.h>
#include <IRLiveness.h>
#include <assert.h>
#include <base64.h>
#include <cleanup.h>
//#define DEBUG_PRINT_ENABLE 1
#include <basicBlocks.h>
#include <debugPrint.h>
#include <stdio.h>
void *IR_ATTR_BASIC_BLOCK = "BASIC_BLOCK";
typedef int (*gnCmpType)(const graphNodeMapping *, const graphNodeMapping *);
typedef int (*varRefCmpType)(const struct IRVar **, const struct IRVar **);
#define ALLOCATE(x)                                                                                                                                                \
	({                                                                                                                                                               \
		typeof(x) *ptr = malloc(sizeof(x));                                                                                                                            \
		*ptr = x;                                                                                                                                                      \
		ptr;                                                                                                                                                           \
	})
static char *__var2Str(struct IRVar var) {
	if (var.var->name) {
		char buffer[1024];
		sprintf(buffer, "%s-%li", var.var->name, var.SSANum);
		char *retVal = malloc(strlen(buffer) + 1);
		strcpy(retVal, buffer);
		return retVal;
	} else {
		char buffer[1024];
		sprintf(buffer, "%p-%li", var.var, var.SSANum);
		char *retVal = malloc(strlen(buffer) + 1);
		strcpy(retVal, buffer);
		return retVal;
	}
}
static char *var2Str(graphNodeIR var) {
	if (debugGetPtrNameConst(var))
		return debugGetPtrName(var);

	__auto_type value = (struct IRNodeValue *)graphNodeIRValuePtr(var);
	if (!var)
		return NULL;
	if (value->base.type != IR_VALUE)
		return NULL;
	if (value->val.type != IR_VAL_VAR_REF)
		return NULL;

	return __var2Str(value->val.value.var);
}
static int filterVars(void *data, struct __graphNode *node) {
	graphNodeIR enterNode = data;
	if (node == enterNode)
		return 1;

	if (graphNodeIRValuePtr(node)->type != IR_VALUE)
		return 0;

	struct IRNodeValue *irNode = (void *)graphNodeIRValuePtr(node);
	if (irNode->val.type != IR_VAL_VAR_REF)
		return 0;

	return 1;
}
static char *ptr2Str(const void *a) {
	return base64Enc((void *)&a, sizeof(a));
}

static void copyConnections(strGraphEdgeP in, strGraphEdgeP out) {
	// Connect in to out(if not already connectected)
	for (long inI = 0; inI != strGraphEdgeMappingPSize(in); inI++) {
		for (long outI = 0; outI != strGraphEdgeMappingPSize(out); outI++) {
			__auto_type inNode = graphEdgeMappingIncoming(in[inI]);
			__auto_type outNode = graphEdgeMappingOutgoing(out[outI]);

			// Check if not connected to
			if (__graphIsConnectedTo(inNode, outNode))
				continue;

			DEBUG_PRINT("Connecting %s to %s\n", var2Str(*graphNodeMappingValuePtr(inNode)), var2Str(*graphNodeMappingValuePtr(outNode)))
			graphNodeMappingConnect(inNode, outNode, NULL);
		}
	}
}
static void __filterTransparentKill(graphNodeMapping node) {
	__auto_type in = graphNodeMappingIncoming(node);
	__auto_type out = graphNodeMappingOutgoing(node);

	copyConnections(in, out);

	graphNodeMappingKill(&node, NULL, NULL);
}
static int IRVarRefCmp(const struct IRVar **a, const struct IRVar **b) {
	return IRVarCmp(a[0], b[0]);
}
static void printVars(strVar vars) {
	for (long i = 0; i != strVarSize(vars); i++) {
		__auto_type name = vars[i].var->name;
		if (name) {
			DEBUG_PRINT("    - %s,%li\n", name, vars[i].SSANum);
		} else {
			DEBUG_PRINT("    - %p,%li\n", vars[i].var, vars[i].SSANum);
		}
	}
}
static int isExprEdge(graphEdgeIR edge) {
	switch (*graphEdgeIRValuePtr(edge)) {
	case IR_CONN_DEST:
	case IR_CONN_FUNC:
	case IR_CONN_FUNC_ARG_1 ... IR_CONN_FUNC_ARG_128:
	case IR_CONN_SIMD_ARG:
	case IR_CONN_SOURCE_A:
	case IR_CONN_SOURCE_B:
		return 1;
	default:
		return 0;
	}
};
static int untilWriteOut(const struct __graphNode *node, const struct __graphEdge *edge, const void *data) {
	//
	// Edge may be a "virtual"(mapped edge from replace that has no value)
	// fail is edge value isnt present
	//
	__auto_type edgeValue = *graphEdgeMappingValuePtr((void *)edge);
	if (!edgeValue)
		return 0;

	if (!isExprEdge(edgeValue))
		return 0;

	strGraphEdgeIRP outgoing CLEANUP(strGraphEdgeIRPDestroy) = graphNodeIROutgoing(*graphNodeMappingValuePtr((graphNodeMapping)node));
	strGraphEdgeIRP outgoingAssigns CLEANUP(strGraphEdgeIRPDestroy) = IRGetConnsOfType(outgoing, IR_CONN_DEST);
	if (strGraphEdgeIRPSize(outgoingAssigns)) {
		return 0;
	}

	return 1;
}
static int ptrPtrCmp(const void *a, const void *b) {
	if (*(void **)a > *(void **)b)
		return 1;
	else if (*(void **)a < *(void **)b)
		return -1;
	else
		return 0;
}
static int isExprNodeOrNotVisited(const struct __graphNode *node, const struct __graphEdge *edge, const void *data) {
	// Check if not already visited
	const strGraphNodeMappingP *visited = data;
	if (NULL != strGraphNodeMappingPSortedFind(*visited, (struct __graphNode *)node, (gnCmpType)ptrPtrCmp))
		return 0;

	//
	// Edges may be "virtual"(edges in mapped graph that come from replaces)
	// So if edge value is NULL,fail
	//
	__auto_type edgeVal = *graphEdgeMappingValuePtr((struct __graphEdge *)edge);
	if (!edgeVal)
		return 0;

	return isExprEdge(edgeVal);
}
static void appendToNodes(struct __graphNode *node, void *data) {
	strGraphNodeMappingP *nodes = data;
	*nodes = strGraphNodeMappingPSortedInsert(*nodes, node, (gnCmpType)ptrPtrCmp);
}
static int isVarNode(const struct IRNode *irNode) {
	if (irNode->type == IR_VALUE) {
		struct IRNodeValue *val = (void *)irNode;
		if (val->val.type == IR_VAL_VAR_REF)
			return 1;
	}

	return 0;
}

static void __visitForwardOrdered(strGraphNodeMappingP *order, strGraphNodeMappingP *visited, graphNodeMapping node) {
	strGraphNodeMappingP outgoing = graphNodeMappingOutgoingNodes(node);
	for (long i = 0; i != strGraphNodeMappingPSize(outgoing); i++) {
		__auto_type node2 = outgoing[i];
		// Ensrue isnt visted
		if (NULL != strGraphNodeMappingPSortedFind(*visited, node2, (gnCmpType)ptrPtrCmp))
			continue;

		// Add to visited
		*visited = strGraphNodeMappingPSortedInsert(*visited, node2, (gnCmpType)ptrPtrCmp);

		// append to order
		*order = strGraphNodeMappingPAppendItem(*order, node2);
#if DEBUG_PRINT_ENABLE
		DEBUG_PRINT("Order %li is %s is %p\n", strGraphNodeMappingPSize(*order), var2Str(node2), node2);
#endif

		// Recur
		__visitForwardOrdered(order, visited, node2);
	}
}
static char *printMappedEdge(struct __graphEdge *edge) {
	return NULL;
}
static char *printMappedNodesValue(struct __graphNode *node) {
	return var2Str(*graphNodeMappingValuePtr(node));
}
static char *printMappedNode(struct __graphNode *node) {
	return ptr2Str(node);
}
static strGraphNodeMappingP sortNodes(graphNodeMapping node) {
	strGraphNodeMappingP order = NULL;
	strGraphNodeMappingP visited = NULL;

	__visitForwardOrdered(&order, &visited, node);

	return order;
}
static void killNode(void *ptr) {
	__graphNodeKill(*(struct __graphNode **)ptr, NULL, NULL);
}
struct varRefNodePair {
	struct IRVar ref;
	graphNodeIRLive node;
};
STR_TYPE_DEF(struct varRefNodePair, VarRefNodePair);
STR_TYPE_FUNCS(struct varRefNodePair, VarRefNodePair);
static int varRefNodePairCmp(const struct varRefNodePair *a, const struct varRefNodePair *b) {
	return IRVarCmp(&a->ref, &b->ref);
}
static char *node2GraphViz(const struct __graphNode *node, mapGraphVizAttr *unused, const void *data) {
	char *n1 = debugGetPtrName(node);
	if (n1)
		return n1;

	return debugGetPtrName(*graphNodeMappingValuePtr((struct __graphNode *)node));
}
graphNodeIRLive IRInterferenceGraph(graphNodeIR start) {
	return IRInterferenceGraphFilter(start, NULL, NULL)[0];
}
static char *strDup(const char *text) {
	return strcpy(malloc(strlen(text) + 1), text);
}
STR_TYPE_DEF(char, Char);
STR_TYPE_FUNCS(char, Char);
static char *nodeToLabel(const struct __graphNode *node, mapGraphVizAttr *attrs, const void *data) {
	mapGraphVizAttrInsert(*attrs, "shape", strDup("record"));
	ptrMapBlockMetaNode *Data = (void *)data;
	__auto_type block = ptrMapBlockMetaNodeGet(*Data, (struct __graphNode *)node);
	if (!block)
		return NULL;
	strChar text CLEANUP(strCharDestroy) = strCharAppendItem(NULL, '{');
	char *name = ptr2Str(node);
	text = strCharAppendData(text, name, strlen(name));
	text = strCharAppendItem(text, '|');
	for (long i = 0; i != strVarSize(block->block->in); i++) {
		char *varStr = __var2Str(block->block->in[i]);
		text = strCharAppendData(text, varStr, strlen(varStr));
		text = strCharAppendItem(text, ',');
		free(varStr);
	}
	text = strCharAppendItem(text, '|');
	for (long i = 0; i != strVarSize(block->block->out); i++) {
		char *varStr = __var2Str(block->block->out[i]);
		text = strCharAppendData(text, varStr, strlen(varStr));
		text = strCharAppendItem(text, ',');
		free(varStr);
	}
	text = strCharAppendItem(text, '}');
	text = strCharAppendItem(text, '\0');
	return strDup(text);
}
static void debugShowGraphIR(graphNodeMapping enter, ptrMapBlockMetaNode *nodeData) {
	const char *name = tmpnam(NULL);
	FILE *f = fopen(name, "w");
	graph2GraphViz(f, enter, "basicblocks", nodeToLabel, NULL, nodeData, NULL);
	fclose(f);
	char buffer[1024];
	sprintf(buffer, "sleep 0.1 &&dot -Tsvg %s > /tmp/live.svg && firefox /tmp/live.svg & ", name);

	system(buffer);
}
strGraphNodeIRLiveP __IRInterferenceGraphFilter(graphNodeMapping start, const void *data, int (*varFilter)(graphNodeIR node, const void *data)) {
	ptrMapBlockMetaNode metaNodes = ptrMapBlockMetaNodeCreate();

	//
	// First find basic blocks by searching alVarRefs for basic blocks,removing
	// consumed nodes then repeating
	//
	strGraphNodeMappingP visited = NULL;
	__auto_type allMappedNodes = graphNodeMappingAllNodes(start);
	__auto_type mappedClone = start;
	for (;;) {
	loop:;
		int found = 0;

		// Dont visit already visted nodes so remove them
		allMappedNodes = strGraphNodeMappingPSetDifference(allMappedNodes, visited, (gnCmpType)ptrPtrCmp);

		for (long i = 0; i != strGraphNodeMappingPSize(allMappedNodes); i++) {
			// Add current node visted;
			visited = strGraphNodeMappingPSortedInsert(visited, allMappedNodes[i], (gnCmpType)ptrPtrCmp);
			strGraphNodeMappingP consumedNodes CLEANUP(strGraphNodeMappingPDestroy) = NULL;
			__auto_type basicBlocks = IRGetBasicBlocksFromExpr(mappedClone, metaNodes, allMappedNodes[i], &consumedNodes, data, varFilter);

			// NULL if not found
			if (!basicBlocks)
				continue;

			// Remove nodes in basic blocks
			allMappedNodes = strGraphNodeMappingPSetDifference(allMappedNodes, consumedNodes, (gnCmpType)ptrPtrCmp);

			// Mark as found;
			found = 1;
			break;
		}

		if (!found)
			break;
	}

	//
	// Found basic blocks,so filter out all non-metablock notes(in metaNodes)
	//
	__auto_type allMappedNodes2 = graphNodeMappingAllNodes(mappedClone);
	for (long i = 0; i != strGraphNodeMappingPSize(allMappedNodes2); i++) {
		// Dont destroy if start node
		if (allMappedNodes2[i] == mappedClone)
			continue;

		//"Transparent" remove if not metaNode
		if (NULL == ptrMapBlockMetaNodeGet(metaNodes, allMappedNodes2[i]))
			__filterTransparentKill(allMappedNodes2[i]);
	}
	// debugShowGraphIR(mappedClone);
	//
// https://lambda.uta.edu/cse5317/spring01/notes/node37.html
//

// Sort if "backwards" order
#if DEBUG_PRINT_ENABLE
	printf("Final:\n");
#endif
	__auto_type forwards = sortNodes(mappedClone);

	// Contains only metaNodes node
	// Init ins/outs to empty sets
	for (long i = strGraphNodeMappingPSize(forwards) - 1; i >= 0; i--) {
		__auto_type find = ptrMapBlockMetaNodeGet(metaNodes, forwards[i]);

#if DEBUG_PRINT_ENABLE
		DEBUG_PRINT("Reseting in/outs of %s to empty.\n", ptr2Str(find->node));
#endif

		// Could be start node
		if (!find)
			continue;

		find->block->in = NULL;
		find->block->out = NULL;
	}

	for (;;) {
		int changed = 0;
		DEBUG_PRINT("START,%i\n\n", 10);
		for (long i = strGraphNodeMappingPSize(forwards) - 1; i >= 0; i--) {
			__auto_type find = ptrMapBlockMetaNodeGet(metaNodes, forwards[i]);

			// Could be start node
			if (!find)
				continue;

			__auto_type oldIns = strVarClone(find->block->in);
			__auto_type oldOuts = strVarClone(find->block->out);

#if DEBUG_PRINT_ENABLE
			DEBUG_PRINT("=======(%s)=======", ptr2Str(find->node));
			DEBUG_PRINT("Old ins of %s:\n", ptr2Str(find->node));
			printVars(oldIns);
			DEBUG_PRINT("Old outs of %s:\n", ptr2Str(find->node));
			printVars(oldOuts);
#endif

			// read[n] Union (out[n]-define[n])
			__auto_type newIns = strVarClone(find->block->read);
			__auto_type diff = strVarSetDifference(strVarClone(find->block->out), find->block->define, IRVarCmp);
			newIns = strVarSetUnion(newIns, diff, IRVarCmp);
			newIns = strVarUnique(newIns, IRVarCmp, NULL);

			// Union of successors insert
			strVar newOuts = NULL;
			__auto_type succs = graphNodeMappingOutgoingNodes(forwards[i]);
			for (long i = 0; i != strGraphNodeMappingPSize(succs); i++) {
				__auto_type find2 = ptrMapBlockMetaNodeGet(metaNodes, succs[i]);

				// Skip if not found(may be start node)
				if (!find2)
					continue;

				// Union
				newOuts = strVarSetUnion(newOuts, find2->block->in, IRVarCmp);
			}
			newOuts = strVarUnique(newOuts, IRVarCmp, NULL);
#if DEBUG_PRINT_ENABLE
			DEBUG_PRINT("New ins of %s:\n", ptr2Str(find->node));
			printVars(newIns);
			DEBUG_PRINT("New outs of %s:\n", ptr2Str(find->node));
			printVars(newOuts);
#endif
			// Destroy old ins/outs then re-assign with new ones
			find->block->in = newIns;
			find->block->out = newOuts;

			// Check if changed
			if (strVarSize(oldIns) == strVarSize(newIns)) {
					for(long v=0;v!=strVarSize(oldIns);v++)
							changed|=0!=IRVarCmp(&oldIns[v], &newIns[v]);
			} else
				changed |= 1;

			if (strVarSize(oldOuts) == strVarSize(newOuts)) {
					for(long v=0;v!=strVarSize(newOuts);v++)
							changed|=0!=IRVarCmp(&oldOuts[v], &newOuts[v]);
			} else
				changed |= 1;

			// Destroy olds
		}

		if (!changed)
			break;
	}
	// debugShowGraphIR(mappedClone, &metaNodes);

	//
	// Create interference graph
	//
	strGraphNodeIRLiveP retVal = NULL;
	strVarRefNodePair assocArray = NULL;
	for (long i = 0; i != strGraphNodePSize(forwards); i++) {
		// Find meta node
		__auto_type find = ptrMapBlockMetaNodeGet(metaNodes, forwards[i]);

		// Could be start node
		if (!find)
			continue;

		{
			//
			// We first search for the items and register them before we connect them
			//
			strGraphNodeIRLiveP liveAtOnce = NULL;
			for (long i2 = 0; i2 != strVarSize(find->block->in); i2++) {
				struct varRefNodePair pair;
				pair.ref = find->block->in[i2];
			registerLoop:;
				// Look for value
				__auto_type find2 = strVarRefNodePairSortedFind(assocArray, pair, varRefNodePairCmp);
				if (NULL == find2) {
					// Create a node
					struct IRVarLiveness live;
					live.ref = find->block->in[i2];

					// Set the ndoe and insert
					pair.node = graphNodeIRLiveCreate(live, 0);
					assocArray = strVarRefNodePairSortedInsert(assocArray, pair, varRefNodePairCmp);

					// Any node wil node
					retVal = strGraphNodeIRLivePSortedInsert(retVal, pair.node, (gnCmpType)ptrPtrCmp);
					goto registerLoop;
				}

				// Append to live at once
				liveAtOnce = strGraphNodeIRLivePAppendItem(liveAtOnce, find2->node);
			}

			//
			// Connect the nodes to eachother(bi-directionally to simular udirecred
			// graph)
			//
			for (long i1 = 0; i1 != strGraphNodeIRLivePSize(liveAtOnce); i1++) {
				for (long i2 = 0; i2 != strGraphNodeIRLivePSize(liveAtOnce); i2++) {
					// Dont connect to self
					if (i1 == i2)
						continue;

					// Dont reconnect
					if (graphNodeIRLiveConnectedTo(liveAtOnce[i1], liveAtOnce[i2]))
						continue;

					// Connect(bi-directional to simulate undirected)
					graphNodeIRLiveConnect(liveAtOnce[i1], liveAtOnce[i2], NULL);
					graphNodeIRLiveConnect(liveAtOnce[i2], liveAtOnce[i1], NULL);

#if DEBUG_PRINT_ENABLE
					__auto_type ref1 = debugGetPtrNameConst(&graphNodeIRLiveValuePtr(liveAtOnce[i1])->ref);
					__auto_type ref2 = debugGetPtrNameConst(&graphNodeIRLiveValuePtr(liveAtOnce[i2])->ref);
					DEBUG_PRINT("Connecting %s to %s\n", ref1, ref2);
#endif
				}
			}
		}
		//
		// Connect edges def[n] to out[n]
		//
		{
			//
			// We first search for the items and register them before we connect them
			//
			strGraphNodeIRLiveP outNodes = NULL;
			for (long i2 = 0; i2 != strVarSize(find->block->out); i2++) {
				struct varRefNodePair pair;
				pair.ref = find->block->out[i2];
			registerLoop2:;
				// Look for value
				__auto_type find2 = strVarRefNodePairSortedFind(assocArray, pair, varRefNodePairCmp);
				if (NULL == find2) {
					// Create a node
					struct IRVarLiveness live;
					live.ref = find->block->out[i2];

					// Set the ndoe and insert
					pair.node = graphNodeIRLiveCreate(live, 0);
					assocArray = strVarRefNodePairSortedInsert(assocArray, pair, varRefNodePairCmp);

					// Any node wil node
					retVal = strGraphNodeIRLivePSortedInsert(retVal, pair.node, (gnCmpType)ptrPtrCmp);
					goto registerLoop2;
				}

				outNodes = strGraphNodeIRLivePAppendItem(outNodes, find2->node);
				;
			}
			strGraphNodeIRLiveP defNodes = NULL;
			for (long i2 = 0; i2 != strVarSize(find->block->define); i2++) {
				struct varRefNodePair pair;
				pair.ref = find->block->define[i2];
			registerLoop3:;
				// Look for value
				__auto_type find2 = strVarRefNodePairSortedFind(assocArray, pair, varRefNodePairCmp);
				if (NULL == find2) {
					// Create a node
					struct IRVarLiveness live;
					live.ref = find->block->define[i2];

					// Set the ndoe and insert
					pair.node = graphNodeIRLiveCreate(live, 0);
					assocArray = strVarRefNodePairSortedInsert(assocArray, pair, varRefNodePairCmp);

					// Any node wil node
					retVal = strGraphNodeIRLivePSortedInsert(retVal, pair.node, (gnCmpType)ptrPtrCmp);
					goto registerLoop3;
				}

				defNodes = strGraphNodeIRLivePAppendItem(defNodes, find2->node);
				;
			}
			//
			// Connect the nodes to eachother(bi-directionally to simular udirecred
			// graph)
			//
			for (long i1 = 0; i1 != strGraphNodeIRLivePSize(outNodes); i1++) {
				for (long i2 = 0; i2 != strGraphNodeIRLivePSize(defNodes); i2++) {
					// Dont reconnect
					if (graphNodeIRLiveConnectedTo(outNodes[i1], defNodes[i2]))
						continue;

					if (outNodes[i1] == defNodes[i2])
						continue;

					// Connect(bi-directional to simulate undirected)
					graphNodeIRLiveConnect(outNodes[i1], defNodes[i2], NULL);
					graphNodeIRLiveConnect(defNodes[i2], outNodes[i1], NULL);
				}
			}
		}
	}
	ptrMapBlockMetaNodeDestroy(metaNodes, killNode);

	//
	strGraphNodeIRLiveP allGraphs = NULL;
	for (; strGraphNodeIRLivePSize(retVal) != 0;) {
		// Use first node
		allGraphs = strGraphNodeIRLivePSortedInsert(allGraphs, retVal[0], (gnCmpType)ptrPtrCmp);

		// Filter out all accessible nodes from retVal,then we look for the next
		// graph
		__auto_type asscesibleNodes = graphNodeIRLiveAllNodes(retVal[0]);
		retVal = strGraphNodeIRLivePSetDifference(retVal, asscesibleNodes, (gnCmpType)ptrPtrCmp);
	}
	return allGraphs;
}
strGraphNodeIRLiveP IRInterferenceGraphFilter(graphNodeIR start, const void *data, int (*varFilter)(graphNodeIR node, const void *data)) {
	__auto_type mappedClone = graphNodeCreateMapping(start, 0);
	return __IRInterferenceGraphFilter(mappedClone, data, varFilter);
}
