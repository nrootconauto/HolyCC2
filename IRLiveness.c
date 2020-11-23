#include <IR.h>
#include <assert.h>
#include <base64.h>
#include <stdio.h>
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
static void copyConnections(strGraphNodeP in, strGraphNodeP out) {
	// Connect in to out(if not already connectected)
	for (long inI = 0; inI != strGraphNodeMappingPSize(in); inI++) {
		for (long outI = 0; outI != strGraphNodeMappingPSize(out); outI++) {
			// Check if not connected to
			if (__graphIsConnectedTo(in[inI], out[outI]))
				continue;

			graphNodeMappingConnect(in[inI], out[outI], NULL);
		}
	}
}
static void __filterTransparentKill(graphNodeMapping node) {
	__auto_type in = graphNodeMappingIncomingNodes(node);
	__auto_type out = graphNodeMappingOutgoingNodes(node);

	copyConnections(in, out);

	graphNodeMappingKill(&node, NULL, NULL);
}
static void replaceNodes(strGraphNodeMappingP nodes,
                         graphNodeMapping replaceWith) {
	//
	// "Transparently" Kill all but one node (that will contain the
	// incoming/outgoing connections of all the nodes) Then we will transfer siad
	// connections to replacewith
	//
	__auto_type len = strGraphNodeMappingPSize(nodes);
	for (long i = 0; i < len - 1; i++) {
		__filterTransparentKill(nodes[i]);
	}

	__auto_type in = graphNodeMappingIncomingNodes(nodes[len - 1]);
	__auto_type out = graphNodeMappingOutgoingNodes(nodes[len - 1]);

	for (long i = 0; i != strGraphNodeMappingPSize(in); i++)
		graphNodeMappingConnect(in[i], replaceWith, NULL);

	for (long i = 0; i != strGraphNodeMappingPSize(out); i++)
		graphNodeMappingConnect(replaceWith, out[i], NULL);

	strGraphNodeMappingPDestroy(&in);
	strGraphNodeMappingPDestroy(&out);
};
struct IRVarLiveness {
	graphNodeIR var;
};
GRAPH_TYPE_DEF(struct IRVarLiveness, void *, IRLive);
GRAPH_TYPE_FUNCS(struct IRVarLiveness, void *, IRLive);
STR_TYPE_DEF(struct IRVarRef *, Var);
STR_TYPE_FUNCS(struct IRVarRef *, Var);
struct basicBlock {
	strGraphNodeMappingP nodes;
	strVar read;
	strVar define;
	strVar in;
	strVar out;
};
static int untilIncomingAssign(const struct __graphNode *node,
                               const struct __graphEdge *edge,
                               const void *data) {
	strGraphEdgeIRP incoming
	    __attribute__((cleanup(strGraphEdgeIRLivePDestroy))) =
	        graphNodeIRIncoming((void *)node);
	if (strGraphEdgeIRPSize(incoming) == 1)
		if (*graphEdgeIRValuePtr(incoming[0]) == IR_CONN_DEST)
			return 0;

	return 1;
}
static int isExprEdge(graphEdgeIR edge) {
	switch (*graphEdgeIRValuePtr(edge)) {
	case IR_CONN_DEST:
	case IR_CONN_FUNC:
	case IR_CONN_FUNC_ARG:
	case IR_CONN_SIMD_ARG:
	case IR_CONN_FLOW:
	case IR_CONN_SOURCE_A:
	case IR_CONN_SOURCE_B:
		return 1;
	default:
		return 0;
	}
};
static int isExprNode(const struct __graphNode *node,
                      const struct __graphEdge *edge, const void *data) {
	return isExprEdge(*graphEdgeMappingValuePtr((struct __graphEdge *)edge));
}
static int ptrPtrCmp(const void *a, const void *b) {
	if (*(void **)a > *(void **)b)
		return 1;
	else if (*(void **)a < *(void **)b)
		return -1;
	else
		return 0;
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
static strBasicBlock getBasicBlocksFromExpr(graphNodeIR dontDestroy,
                                            mapBlockMetaNode metaNodes,
                                            graphNodeMapping start) {
	strGraphNodeMappingP nodes = NULL;
	graphNodeMappingVisitBackward(start, &nodes, isExprNode, appendToNodes);
	graphNodeMappingVisitForward(start, &nodes, isExprNode, appendToNodes);

	if (nodes == NULL)
		return NULL;

	nodes = strGraphNodeMappingPSortedInsert(nodes, start, (gnCmpType)ptrPtrCmp);

	strBasicBlock retVal = NULL;

	//
	// Find assign nodes
	//
	strGraphNodeMappingP assignNodes = NULL;
	for (long i = 0; i != strGraphNodeMappingPSize(nodes); i++) {
		__auto_type irNode =
		    graphNodeIRValuePtr(*graphNodeMappingValuePtr(nodes[i]));
		// Check if var
		if (isVarNode(irNode)) {
			// Check if assign var
			strGraphEdgeIRP incomingEdges
			    __attribute__((cleanup(strGraphEdgeIRPDestroy))) =
			        graphNodeIRIncoming(*graphNodeMappingValuePtr(nodes[i]));
			if (strGraphEdgeIRPSize(incomingEdges) == 1)
				if (*graphEdgeIRValuePtr(incomingEdges[0]) == IR_CONN_DEST)
					assignNodes = strGraphNodeMappingPSortedInsert(assignNodes, nodes[i],
					                                               (gnCmpType)ptrPtrCmp);
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
		if (hasExprOutgoing)
			sinks = strGraphNodeMappingPSortedInsert(sinks, nodes[i],
			                                         (gnCmpType)ptrPtrCmp);
	}

	//
	// Turn sinks and assigns into sub-blocks
	//
	strGraphNodeMappingP consumedNodes = NULL;

	// Concat assign nodes with sinks
	assignNodes = strGraphNodeMappingPConcat(assignNodes, sinks);

	for (long i = 0; i != strGraphNodeMappingPSize(assignNodes); i++) {
		strGraphNodeMappingP exprNodes =
		    strGraphNodeIRPAppendItem(NULL, assignNodes[i]);
		graphNodeMappingVisitBackward(assignNodes[i], &exprNodes,
		                              untilIncomingAssign, appendToNodes);

		struct basicBlock block;
		block.nodes = exprNodes;
		block.read = NULL;

		// Check if node is asssigned to
		__auto_type node = *graphNodeMappingValuePtr(assignNodes[i]);
		strGraphEdgeIRP incoming
		    __attribute__((cleanup(strGraphEdgeIRLivePDestroy))) =
		        graphNodeIRIncoming(node);
		if (strGraphEdgeIRPSize(incoming) == 1) {
			if (*graphEdgeIRValuePtr(incoming[0]) == IR_CONN_DEST) {
				struct IRNode *irNode = (void *)graphNodeIRValuePtr(node);
				assert(isVarNode(irNode));
				block.define = strVarAppendItem(
				    NULL, &((struct IRNodeValue *)irNode)->val.value.var);
			}
		} else {
			// Not assigned to
			block.define = NULL;
		}

		__auto_type defineRef = &((struct IRNodeValue *)graphNodeIRValuePtr(
		                              *graphNodeMappingValuePtr(assignNodes[i])))
		                             ->val.value.var;
		block.define = strVarAppendItem(NULL, defineRef);
		// Find read variable refs
		for (long i = 0; i != strGraphNodePSize(exprNodes); i++) {
			struct IRNode *irNode =
			    graphNodeIRValuePtr(*graphNodeMappingValuePtr(exprNodes[i]));
			if (isVarNode(irNode)) {
				struct IRNodeValue *var = (void *)irNode;
				block.read = strVarSortedInsert(block.read, &var->val.value.var,
				                                (varRefCmpType)ptrPtrCmp);
			}
		}

		// Add to  consumed nodes
		consumedNodes = strGraphNodeMappingPSetUnion(consumedNodes, exprNodes,
		                                             (gnCmpType)ptrPtrCmp);

		// Push to blocks
		retVal = strBasicBlockAppendItem(retVal, ALLOCATE(block));
	}

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

		// Create meta node entry
		__auto_type metaNode = graphNodeMappingCreate(NULL, 0);
		struct blockMetaNode pair;
		pair.block = retVal[i];
		pair.node = metaNode;
		char *hash = ptr2Str(metaNode);
		mapBlockMetaNodeInsert(metaNodes, hash, pair);
		free(hash);

		// Replace with metaNode
		replaceNodes(toReplace, metaNode);
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

		// Recur
		__visitForwardOrdered(order, visited, node2);
	}
	strGraphNodeMappingPDestroy(&outgoing);
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
void IRInterferenceGraph(graphNodeIR start) {
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
		                                                   (gnCmpType)visited);

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
	__auto_type backwards = sortNodesBackwards(start);

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
}
