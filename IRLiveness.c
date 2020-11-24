#include <IR.h>
#include <IRLiveness.h>
#include <assert.h>
#include <base64.h>
#include <stdio.h>
#define DEBUG_PRINT_ENABLE 1
#include <debugPrint.h>
typedef int (*gnCmpType)(const graphNodeMapping *, const graphNodeMapping *);
typedef int (*varRefCmpType)(const struct IRVarRef **,
                             const struct IRVarRef **);
#define ALLOCATE(x)                                                            \
	({                                                                           \
		typeof(x) *ptr = malloc(sizeof(x));                                        \
		*ptr = x;                                                                  \
		ptr;                                                                       \
	})
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
static char *ptr2Str(const void *a) { return base64Enc((void *)&a, sizeof(a)); }

static void copyConnections(strGraphEdgeP in, strGraphEdgeP out) {
	// Connect in to out(if not already connectected)
	for (long inI = 0; inI != strGraphEdgeMappingPSize(in); inI++) {
		for (long outI = 0; outI != strGraphEdgeMappingPSize(out); outI++) {
			__auto_type inNode = graphEdgeMappingIncoming(in[inI]);
			__auto_type outNode = graphEdgeMappingOutgoing(out[outI]);

			// Check if not connected to
			if (__graphIsConnectedTo(inNode, outNode))
				continue;

			DEBUG_PRINT("Connecting %s to %s\n",
			            debugGetPtrNameConst(*graphNodeMappingValuePtr(inNode)),
			            debugGetPtrNameConst(*graphNodeMappingValuePtr(outNode)))
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
STR_TYPE_DEF(struct IRVarRef *, Var);
STR_TYPE_FUNCS(struct IRVarRef *, Var);
struct basicBlock {
	strGraphNodeMappingP nodes;
	strVar read;
	strVar define;
	strVar in;
	strVar out;
};
static int isExprEdge(graphEdgeIR edge) {
	switch (*graphEdgeIRValuePtr(edge)) {
	case IR_CONN_DEST:
	case IR_CONN_FUNC:
	case IR_CONN_FUNC_ARG:
	case IR_CONN_SIMD_ARG:
	case IR_CONN_SOURCE_A:
	case IR_CONN_SOURCE_B:
		return 1;
	default:
		return 0;
	}
};
static int untilIncomingAssign(const struct __graphNode *node,
                               const struct __graphEdge *edge,
                               const void *data) {
	//
	// Edge may be a "virtual"(mapped edge from replace that has no value)
	// fail is edge value isnt present
	//
	__auto_type edgeValue = *graphEdgeMappingValuePtr((void *)edge);
	if (!edgeValue)
		return 0;

	if (!isExprEdge(edgeValue))
		return 0;

	strGraphEdgeIRP incoming __attribute__((cleanup(strGraphEdgeIRPDestroy))) =
	    graphNodeIRIncoming((void *)node);
	if (strGraphEdgeIRPSize(incoming) == 1)
		if (*graphEdgeIRValuePtr(incoming[0]) == IR_CONN_DEST)
			return 0;

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
static int isExprNodeOrNotVisited(const struct __graphNode *node,
                                  const struct __graphEdge *edge,
                                  const void *data) {
	// Check if not already visited
	const strGraphNodeMappingP *visited = data;
	if (NULL != strGraphNodeMappingPSortedFind(
	                *visited, (struct __graphNode *)node, (gnCmpType)ptrPtrCmp))
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
STR_TYPE_DEF(struct basicBlock *, BasicBlock);
STR_TYPE_FUNCS(struct basicBlock *, BasicBlock);
static int isVarNode(const struct IRNode *irNode) {
	if (irNode->type == IR_VALUE) {
		struct IRNodeValue *val = (void *)irNode;
		if (val->val.type == IR_VAL_VAR_REF)
			return 1;
	}

	return 0;
}
struct blockMetaNode {
	graphNodeMapping node;
	struct basicBlock *block;
};
MAP_TYPE_DEF(struct blockMetaNode, BlockMetaNode);
MAP_TYPE_FUNCS(struct blockMetaNode, BlockMetaNode);
static strGraphNodeMappingP visitAllAdjExprTo(graphNodeMapping node) {
	strGraphNodeMappingP visited = strGraphNodeMappingPAppendItem(NULL, node);

	for (;;) {
	loop:;
		long oldSize = strGraphNodeMappingPSize(visited);

		for (long i = 0; i != oldSize; i++) {
			// Visit forwards and backwards
			graphNodeMappingVisitBackward(visited[i], &visited,
			                              isExprNodeOrNotVisited, appendToNodes);
			graphNodeMappingVisitForward(visited[i], &visited, isExprNodeOrNotVisited,
			                             appendToNodes);

			// Restart search if added new items
			if (strGraphNodeMappingPSize(visited) != oldSize)
				goto loop;
		}

		// If no new items found break
		if (strGraphNodeMappingPSize(visited) == oldSize)
			break;
	}

	// If no other vistied nodes other than node,then return NULL as node is
	// present by defualt
	if (1 == strGraphNodeMappingPSize(visited))
		if (visited[0] == node) {
			strGraphNodeMappingPDestroy(&visited);
			return NULL;
		}

	return visited;
}
static strBasicBlock getBasicBlocksFromExpr(graphNodeIR dontDestroy,
                                            mapBlockMetaNode metaNodes,
                                            graphNodeMapping start) {
	strGraphNodeMappingP nodes = visitAllAdjExprTo(start);
	if (nodes == NULL)
		return NULL;

	strBasicBlock retVal = NULL;

	//
	// Find assign nodes
	//
	strGraphNodeMappingP assignNodes = NULL;
	for (long i = 0; i != strGraphNodeMappingPSize(nodes); i++) {
		__auto_type ir = *graphNodeMappingValuePtr(nodes[i]);
		const char *name = debugGetPtrNameConst(ir);
		__auto_type irNode = graphNodeIRValuePtr(ir);
		// Check if var
		if (isVarNode(irNode)) {
			// Check if assign var
			strGraphEdgeIRP incomingEdges
			    __attribute__((cleanup(strGraphEdgeIRPDestroy))) =
			        graphNodeIRIncoming(*graphNodeMappingValuePtr(nodes[i]));
			if (strGraphEdgeIRPSize(incomingEdges) == 1)
				if (*graphEdgeIRValuePtr(incomingEdges[0]) == IR_CONN_DEST) {
					assignNodes = strGraphNodeMappingPSortedInsert(assignNodes, nodes[i],
					                                               (gnCmpType)ptrPtrCmp);
#if DEBUG_PRINT_ENABLE
					DEBUG_PRINT("Assign node :%s\n",
					            debugGetPtrNameConst(*graphNodeMappingValuePtr(nodes[i])))
#endif
				}
		}
	}

	//
	// Find "sinks" that dont end with assigns and dont lead to any more
	// expression edges
	//
	strGraphNodeMappingP sinks = NULL;
	for (long i = 0; i != strGraphNodeMappingPSize(nodes); i++) {
		strGraphEdgeIRP outgoing __attribute__((cleanup(strGraphEdgeIRPDestroy))) =
		    NULL;
		outgoing = graphNodeMappingOutgoing(*graphNodeMappingValuePtr(nodes[i]));

		// Check if all outgoing edgesdont belong to expressions
		int hasExprOutgoing = 0;
		for (long i = 0; i != strGraphEdgePSize(outgoing); i++) {
			if (isExprEdge(outgoing[i])) {
				hasExprOutgoing = 1;
				break;
			}
		}

		// Insert into sinks
		if (!hasExprOutgoing) {
			// Ensure isnt an assign operation(assign operations may have no outgoing
			// expression edges)
			if (NULL == strGraphNodeMappingPSortedFind(assignNodes, nodes[i],
			                                           (gnCmpType)ptrPtrCmp)) {
				sinks = strGraphNodeMappingPSortedInsert(sinks, nodes[i],
				                                         (gnCmpType)ptrPtrCmp);
#if DEBUG_PRINT_ENABLE
				DEBUG_PRINT("Assigning sink node :%s\n",
				            debugGetPtrNameConst(*graphNodeMappingValuePtr(nodes[i])))
#endif
			}
		}
	}

	//
	// Turn sinks and assigns into sub-blocks
	//
	strGraphNodeMappingP consumedNodes = NULL;

	// Concat assign nodes with sinks
	__auto_type oldSinks = strGraphNodeMappingPClone(sinks);
	assignNodes = strGraphNodeMappingPConcat(assignNodes, sinks);

	for (long i = 0; i != strGraphNodeMappingPSize(assignNodes); i++) {
		strGraphNodeMappingP exprNodes =
		    strGraphNodeIRPAppendItem(NULL, assignNodes[i]);
		graphNodeMappingVisitBackward(assignNodes[i], &exprNodes,
		                              untilIncomingAssign, appendToNodes);

		struct basicBlock block;
		block.nodes = exprNodes;
		block.read = NULL;

		// Check if node is asssigned to,or only read from
		__auto_type node = *graphNodeMappingValuePtr(assignNodes[i]);
		strGraphEdgeIRP incoming __attribute__((cleanup(strGraphEdgeIRPDestroy))) =
		    graphNodeIRIncoming(node);

		if (strGraphEdgeIRPSize(incoming) == 1) {
			if (*graphEdgeIRValuePtr(incoming[0]) == IR_CONN_DEST) {
				// is Connected to dest node
				struct IRNode *irNode = (void *)graphNodeIRValuePtr(node);
				assert(isVarNode(irNode));
				block.define = strVarAppendItem(
				    NULL, &((struct IRNodeValue *)irNode)->val.value.var);
			}
		} else {
			// Not assigned to
			block.define = NULL;
		}

		//
		// IF isnt a sink,is part of the original assign nodes
		//
		if (NULL == strGraphNodeMappingPSortedFind(oldSinks, assignNodes[i],
		                                           (gnCmpType)ptrPtrCmp)) {
			__auto_type defineRef = &((struct IRNodeValue *)graphNodeIRValuePtr(
			                              *graphNodeMappingValuePtr(assignNodes[i])))
			                             ->val.value.var;
			block.define = strVarAppendItem(NULL, defineRef);
#if DEBUG_PRINT_ENABLE
			DEBUG_PRINT(
			    "Writing to %s\n",
			    debugGetPtrNameConst(*graphNodeMappingValuePtr(assignNodes[i])));
#endif
		} else {
			block.define = NULL;
		}

		// Find read variable refs
		for (long i = 0; i != strGraphNodePSize(exprNodes); i++) {

			struct IRNode *irNode =
			    graphNodeIRValuePtr(*graphNodeMappingValuePtr(exprNodes[i]));
			if (isVarNode(irNode)) {
				struct IRNodeValue *var = (void *)irNode;

				// Ensure isnt assigned node
				if (NULL != strVarSortedFind(block.define, &var->val.value.var,
				                             (varRefCmpType)ptrPtrCmp))
					continue;

				block.read = strVarSortedInsert(block.read, &var->val.value.var,
				                                (varRefCmpType)ptrPtrCmp);
#if DEBUG_PRINT_ENABLE
				DEBUG_PRINT(
				    "Reading from %s\n",
				    debugGetPtrNameConst(*graphNodeMappingValuePtr(exprNodes[i])));
#endif
			}
		}

		// Add to  consumed nodes
		consumedNodes = strGraphNodeMappingPSetUnion(consumedNodes, exprNodes,
		                                             (gnCmpType)ptrPtrCmp);

		// Push to blocks
		retVal = strBasicBlockAppendItem(retVal, ALLOCATE(block));
	}
#if DEBUG_PRINT_ENABLE
	DEBUG_PRINT("Removing: %li items for block %li:\n",
	            strGraphNodeMappingPSize(nodes), strBasicBlockSize(retVal));
	for (long i = 0; i != strGraphNodeMappingPSize(nodes); i++) {
		DEBUG_PRINT("    - %s\n",
		            debugGetPtrNameConst(*graphNodeMappingValuePtr((nodes[i]))));
	}
#endif
	// Remove consumed from possible sinks
	nodes = strGraphNodeMappingPSetDifference(nodes, consumedNodes,
	                                          (gnCmpType)ptrPtrCmp);
	// ALL NODES MUST BE CONSUMED OR SOMETHING WENT WRONG
	assert(strGraphNodeMappingPSize(nodes) == 0);

	//
	// Replace sub-blocks (make sure not to include already replaces nodes in
	// reaplce operations)
	//
	strGraphNodeMappingP replacedNodes = NULL;
	for (long i = 0; i != strBasicBlockSize(retVal); i++) {
		__auto_type toReplace = strGraphNodeMappingPAppendData(
		    NULL, retVal[i]->nodes, strGraphNodeMappingPSize(retVal[i]->nodes));
		toReplace = strGraphNodeMappingPSetDifference(toReplace, replacedNodes,
		                                              (gnCmpType)ptrPtrCmp);

		// Remove dontDestroy from to replace
		strGraphNodeMappingP dummy =
		    strGraphNodeMappingPAppendItem(NULL, dontDestroy);
		toReplace = strGraphNodeMappingPSetDifference(toReplace, dummy,
		                                              (gnCmpType)ptrPtrCmp);
		strGraphNodeMappingPDestroy(&dummy);

#if DEBUG_PRINT_ENABLE
		DEBUG_PRINT("Replacing: %li items:\n", strGraphNodeMappingPSize(toReplace));
		for (long i2 = 0; i2 != strGraphNodeMappingPSize(toReplace); i2++) {
			DEBUG_PRINT("    - %s\n", debugGetPtrNameConst(*graphNodeMappingValuePtr(
			                              (toReplace[i2]))));
		}
#endif

		// Create meta node entry
		__auto_type metaNode = graphNodeMappingCreate(NULL, 0);
		struct blockMetaNode pair;
		pair.block = retVal[i];
		pair.node = metaNode;
		char *hash = ptr2Str(metaNode);
		mapBlockMetaNodeInsert(metaNodes, hash, pair);
		free(hash);
#if DEBUG_PRINT_ENABLE
		char buffer[128];
		sprintf(buffer, "Block %s  metanode",
		        debugGetPtrNameConst(*graphNodeMappingValuePtr(toReplace[0])));
		debugAddPtrName(metaNode, buffer);
#endif

		// Replace with metaNode
		graphMappingReplaceNodes(toReplace, metaNode, NULL, NULL);
	}

	strGraphNodeMappingPDestroy(&consumedNodes);
	strGraphNodeMappingPDestroy(&nodes);
	strGraphNodeMappingPDestroy(&replacedNodes);

	return retVal;
}
static void basicBlockDestroy(struct basicBlock **block) {
	strVarDestroy(&block[0]->in);
	strVarDestroy(&block[0]->out);
	free(block[0]);
	strGraphNodeMappingPDestroy(&block[0]->nodes);
}

static void __visitForwardOrdered(strGraphNodeMappingP *order,
                                  strGraphNodeMappingP *visited,
                                  graphNodeMapping node) {
	strGraphNodeMappingP outgoing = graphNodeMappingOutgoingNodes(node);
	for (long i = 0; i != strGraphNodeMappingPSize(outgoing); i++) {
		__auto_type node2 = outgoing[i];
		// Ensrue isnt visted
		if (NULL !=
		    strGraphNodeMappingPSortedFind(*visited, node2, (gnCmpType)ptrPtrCmp))
			continue;

		// Add to visited
		*visited =
		    strGraphNodeMappingPSortedInsert(*visited, node2, (gnCmpType)ptrPtrCmp);

		// append to order
		*order = strGraphNodeMappingPAppendItem(*order, node2);
#if DEBUG_PRINT_ENABLE
		DEBUG_PRINT("Order %li is %s\n", strGraphNodeMappingPSize(*order),
		            debugGetPtrNameConst(node2));
#endif

		// Recur
		__visitForwardOrdered(order, visited, node2);
	}
	strGraphNodeMappingPDestroy(&outgoing);
}
static char *printMappedEdge(struct __graphEdge *edge) { return NULL; }
static char *printMappedNode(struct __graphNode *node) {
	return debugGetPtrName(node);
}
static strGraphNodeMappingP sortNodesBackwards(graphNodeMapping node) {
	strGraphNodeMappingP order = NULL;
	strGraphNodeMappingP visited = NULL;

	__visitForwardOrdered(&order, &visited, node);

	strGraphNodeMappingPDestroy(&visited);

	// Reverse order
	long len = strGraphNodeMappingPSize(order);
	for (long i = 0; i != len; i++) {
		__auto_type a = &order[i];
		__auto_type b = &order[len - i - 1];
		__auto_type tmp = *a;
		*a = *b;
		*b = tmp;
	}

	return order;
}
graphNodeIRLive IRInterferenceGraph(graphNodeIR start) {
	mapBlockMetaNode metaNodes = mapBlockMetaNodeCreate();

	__auto_type allNodes = graphNodeIRAllNodes(start);
	__auto_type mappedClone = createGraphMap(allNodes, 1);

	//
	// First find basic blocks by searching alVarRefs for basic blocks,removing
	// consumed nodes then repeating
	//
	strGraphNodeMappingP visited = NULL;
	__auto_type allMappedNodes = graphNodeMappingAllNodes(mappedClone);
	for (;;) {
	loop:;
		int found = 0;

		// Dont visit already visted nodes so remove them
		allMappedNodes = strGraphNodeMappingPSetDifference(allMappedNodes, visited,
		                                                   (gnCmpType)ptrPtrCmp);

		for (long i = 0; i != strGraphNodeMappingPSize(allMappedNodes); i++) {
			// Add current node visted;
			visited = strGraphNodeMappingPSortedInsert(visited, allMappedNodes[i],
			                                           (gnCmpType)ptrPtrCmp);

			__auto_type basicBlocks =
			    getBasicBlocksFromExpr(start, metaNodes, allMappedNodes[i]);

			// NULL if not found
			if (!basicBlocks)
				continue;

			// Remove nodes in basic blocks
			for (long i = 0; i != strBasicBlockSize(basicBlocks); i++) {
				allMappedNodes = strGraphNodeMappingPSetDifference(
				    allMappedNodes, basicBlocks[i]->nodes, (gnCmpType)ptrPtrCmp);
			}

			// Mark as found;
			found = 1;
			break;
		}

		if (!found)
			break;
	}
	strGraphNodeMappingPDestroy(&allMappedNodes);

	//
	// Found basic blocks,so filter out all non-metablock notes(in metaNodes)
	//
	__auto_type allMappedNodes2 = graphNodeMappingAllNodes(mappedClone);
	for (long i = 0; i != strGraphNodeMappingPSize(allMappedNodes2); i++) {
		// Dont destroy if start node
		if (allMappedNodes2[i] == mappedClone)
			continue;

		//"Transparent" remove if not metaNode
		char *hash = ptr2Str(allMappedNodes2[i]);
		if (NULL == mapBlockMetaNodeGet(metaNodes, hash))
			__filterTransparentKill(allMappedNodes2[i]);

		free(hash);
	}
	strGraphNodeMappingPDestroy(&allMappedNodes2);

//
// https://lambda.uta.edu/cse5317/spring01/notes/node37.html
//

// Sort if "backwards" order
#if DEBUG_PRINT_ENABLE
	graphPrint(mappedClone, printMappedNode, printMappedEdge);
#endif
	__auto_type backwards = sortNodesBackwards(mappedClone);

	// Contains only metaNodes node
	// Init ins/outs to empty sets
	for (long i = 0; i != strGraphNodeMappingPSize(backwards); i++) {
		char *hash = ptr2Str(backwards[i]);
		__auto_type find = mapBlockMetaNodeGet(metaNodes, hash);
		free(hash);

		// Could be start node
		if (!find)
			continue;

		find->block->in = NULL;
		find->block->out = NULL;
	}

	for (;;) {
		int changed = 0;

		for (long i = 0; i != strGraphNodeMappingPSize(backwards); i++) {
			char *hash = ptr2Str(backwards[i]);
			__auto_type find = mapBlockMetaNodeGet(metaNodes, hash);
			free(hash);

			// Could be start node
			if (!find)
				continue;

			__auto_type oldIns = strVarClone(find->block->in);
			__auto_type oldOuts = strVarClone(find->block->in);

			// read[n] Union (out[n]-define[n])
			__auto_type newIns = strVarClone(find->block->read);
			__auto_type diff =
			    strVarSetDifference(strVarClone(find->block->out), find->block->out,
			                        (varRefCmpType)ptrPtrCmp);
			newIns = strVarSetUnion(newIns, diff, (varRefCmpType)ptrPtrCmp);
			strVarDestroy(&diff);

			// Union of successors insert
			strVar newOuts = NULL;
			__auto_type succs = graphNodeMappingOutgoingNodes(backwards[i]);
			for (long i = 0; i != strGraphNodeMappingPSize(succs); i++) {
				char *hash = ptr2Str(succs[i]);
				__auto_type find2 = mapBlockMetaNodeGet(metaNodes, hash);
				free(hash);

				// Skip if not found(may be start node)
				if (!find2)
					continue;

				// Union
				newOuts =
				    strVarSetUnion(newOuts, find2->block->in, (varRefCmpType)ptrPtrCmp);
			}

			// Destroy old ins/outs then re-assign with new ones
			strVarDestroy(&find->block->in);
			strVarDestroy(&find->block->out);
			find->block->in = newIns;
			find->block->out = newOuts;

			// Check if changed
			if (strVarSize(oldIns) == strVarSize(newIns))
				changed |=
				    0 != memcmp(oldIns, newIns, strVarSize(oldIns) * sizeof(*oldIns));
			else
				changed |= 1;

			if (strVarSize(oldOuts) == strVarSize(newOuts))
				changed |= 0 != memcmp(oldOuts, newOuts,
				                       strVarSize(oldOuts) * sizeof(*oldOuts));
			else
				changed |= 1;

			// Destroy olds
			strVarDestroy(&oldIns), strVarDestroy(&oldOuts);
		}

		if (!changed)
			break;
	}
	strGraphNodeMappingPDestroy(&backwards);

	return NULL;
}
