#include <IR.h>
#include <IRLiveness.h>
#include <assert.h>
#include <base64.h>
#include <stdio.h>
#define DEBUG_PRINT_ENABLE 1
#include <debugPrint.h>
typedef int (*gnCmpType)(const graphNodeMapping *, const graphNodeMapping *);
typedef int (*varRefCmpType)(const struct IRVar **, const struct IRVar **);
#define ALLOCATE(x)                                                            \
	({                                                                           \
		typeof(x) *ptr = malloc(sizeof(x));                                        \
		*ptr = x;                                                                  \
		ptr;                                                                       \
	})
static char *var2Str(graphNodeIR var) {
		if(var==NULL)
				return NULL;
		
		if(debugGetPtrNameConst(var))
				return debugGetPtrName(var);
		
		__auto_type value=(struct IRNodeValue*)graphNodeIRValuePtr(var);
		if(value->base.type!=IR_VALUE)
				return NULL;
		
		char buffer[1024];
		sprintf(buffer, "%s-%li", value->val.value.var.value.var->name,value->val.value.var.SSANum);
		char *retVal=malloc(strlen(buffer)+1);
		strcpy(retVal, buffer);

		return retVal;
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
			            var2Str(*graphNodeMappingValuePtr(inNode)),
			            var2Str(*graphNodeMappingValuePtr(outNode)))
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
STR_TYPE_DEF(struct IRVar *, Var);
STR_TYPE_FUNCS(struct IRVar *, Var);
static int IRVarRefCmp(const struct IRVar **a, const struct IRVar **b) {
	return IRVarCmp(a[0], b[0]);
}
struct basicBlock {
	strGraphNodeMappingP nodes;
	strVar read;
	strVar define;
	strVar in;
	strVar out;
};
static void printVars(strVar vars) {
	for (long i = 0; i != strVarSize(vars); i++) {
			DEBUG_PRINT("    - %s,%li\n", vars[i]->value.var->name,vars[i]->SSANum);
	}
}
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
static int untilAssign(const struct __graphNode *node,
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
			graphNodeIRIncoming(*graphNodeMappingValuePtr((graphNodeMapping)node));
	
	if (strGraphEdgeIRPSize(incoming) == 1) {
			__auto_type type=graphEdgeIRValuePtr(incoming[0]);
			if (*type== IR_CONN_DEST)
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
                                            graphNodeMapping start,const void *data,int(*varFilter)(graphNodeIR var,const void *data)) {
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
		const char *name = var2Str(ir);
		__auto_type irNode = graphNodeIRValuePtr(ir);
		// Check if var
		if (isVarNode(irNode)) {
				//If filter predicate provided,filter it out
				if(varFilter)
						if(!varFilter(ir,data))
								continue;
				
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
					            var2Str(*graphNodeMappingValuePtr(nodes[i])))
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
				            var2Str(*graphNodeMappingValuePtr(nodes[i])))
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
		strGraphNodeMappingP exprNodes = NULL;
		graphNodeMappingVisitBackward(assignNodes[i], &exprNodes,
		                              untilAssign, appendToNodes);

		struct basicBlock block;
		block.nodes = exprNodes;
		block.nodes=strGraphNodeMappingPSortedInsert(block.nodes, assignNodes[i], (gnCmpType)ptrPtrCmp);
		block.read = NULL;
		block.define=NULL;

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
			    var2Str(*graphNodeMappingValuePtr(assignNodes[i])));
#endif
		}

		// Find read variable refs
		for (long i = 0; i != strGraphNodePSize(exprNodes); i++) {

			struct IRNode *irNode =
			    graphNodeIRValuePtr(*graphNodeMappingValuePtr(exprNodes[i]));
			if (isVarNode(irNode)) {
					//If filter predicate provided,filter it out
					if(varFilter)
							if(!varFilter(exprNodes[i],data))
									continue;
				
				struct IRNodeValue *var = (void *)irNode;

				// Ensure isnt assigned node
				if (NULL != strVarSortedFind(block.define, &var->val.value.var,
				                             IRVarRefCmp))
					continue;

				block.read = strVarSortedInsert(block.read, &var->val.value.var,
				                                IRVarRefCmp);
#if DEBUG_PRINT_ENABLE
				DEBUG_PRINT(
				    "Reading from %s\n",
				    var2Str(*graphNodeMappingValuePtr(exprNodes[i])));
#endif
			}
		}

		// Add to  consumed nodes
		consumedNodes = strGraphNodeMappingPSetUnion(consumedNodes, exprNodes,
		                                             (gnCmpType)ptrPtrCmp);
		//Add assign node to consumed nodes
		consumedNodes=strGraphNodeIRPSortedInsert(consumedNodes, assignNodes[i], (gnCmpType)ptrPtrCmp);
		
		// Push to blocks
		retVal = strBasicBlockAppendItem(retVal, ALLOCATE(block));
	}
#if DEBUG_PRINT_ENABLE
	DEBUG_PRINT("Removing: %li items for block %li:\n",
	            strGraphNodeMappingPSize(nodes), strBasicBlockSize(retVal));
	for (long i = 0; i != strGraphNodeMappingPSize(nodes); i++) {
		DEBUG_PRINT("    - %s\n",
		            var2Str(*graphNodeMappingValuePtr((nodes[i]))));
	}
#endif
	// Remove consumed from possible sinks
	nodes = strGraphNodeMappingPSetDifference(nodes, consumedNodes,
	                                          (gnCmpType)ptrPtrCmp);
#if DEBUG_PRINT_ENABLE
	DEBUG_PRINT("Removing: %li items for block %li:\n",
	            strGraphNodeMappingPSize(nodes), strBasicBlockSize(retVal));
	for (long i = 0; i != strGraphNodeMappingPSize(nodes); i++) {
		DEBUG_PRINT("    - %s\n",
		            var2Str(*graphNodeMappingValuePtr((nodes[i]))));
	}
#endif
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
			DEBUG_PRINT("    - %s\n", var2Str(*graphNodeMappingValuePtr(
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
												var2Str(*graphNodeMappingValuePtr(toReplace[0])));
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
		DEBUG_PRINT("Order %li is %s is %p\n", strGraphNodeMappingPSize(*order),
		            var2Str(node2), node2);
#endif

		// Recur
		__visitForwardOrdered(order, visited, node2);
	}
	strGraphNodeMappingPDestroy(&outgoing);
}
static char *printMappedEdge(struct __graphEdge *edge) { return NULL; }
static char *printMappedNodesValue(struct __graphNode *node) {
	return var2Str(*graphNodeMappingValuePtr(node));
}
static char *printMappedNode(struct __graphNode *node) {
	return var2Str(node);
}
static strGraphNodeMappingP sortNodes(graphNodeMapping node) {
	strGraphNodeMappingP order = NULL;
	strGraphNodeMappingP visited = NULL;

	__visitForwardOrdered(&order, &visited, node);

	strGraphNodeMappingPDestroy(&visited);

	return order;
}
static void killNode(void *ptr) {
	__graphNodeKill(*(struct __graphNode **)ptr, NULL, NULL);
}
struct varRefNodePair {
	struct IRVar *ref;
	graphNodeIRLive node;
};
STR_TYPE_DEF(struct varRefNodePair, VarRefNodePair);
STR_TYPE_FUNCS(struct varRefNodePair, VarRefNodePair);
static int varRefNodePairCmp(const struct varRefNodePair *a,
                             const struct varRefNodePair *b) {
	return IRVarRefCmp((void *)&a->ref, (void *)&b->ref);
}
static char *node2GraphViz(const struct __graphNode *node,
                           mapGraphVizAttr *unused, const void *data) {
	char *n1 = debugGetPtrName(node);
	if (n1)
		return n1;

	return debugGetPtrName(*graphNodeMappingValuePtr((struct __graphNode *)node));
}
graphNodeIRLive IRInterferenceGraph(graphNodeIR start) {
		return IRInterferenceGraphFilter(start,NULL,NULL)[0];
}
static void validateStrVarSet(strVar vars) {
		for(long i=0;i<strVarSize(vars)-1;i++) {
				assert(0>=IRVarCmp(vars[i], vars[i+1]));
		}
}
strGraphNodeIRLiveP __IRInterferenceGraphFilter(graphNodeMapping start,const void *data,int(*varFilter)(graphNodeIR node,const void *data)) {
	mapBlockMetaNode metaNodes = mapBlockMetaNodeCreate();

	//
	// First find basic blocks by searching alVarRefs for basic blocks,removing
	// consumed nodes then repeating
	//
	strGraphNodeMappingP visited = NULL;
	__auto_type allMappedNodes = graphNodeMappingAllNodes(start);
	__auto_type mappedClone=start;
	
	/*
char *name=tmpnam(NULL);
	    FILE *f=fopen(name, "w");
	    graph2GraphViz(f, mappedClone, "Tmp", node2GraphViz, NULL, NULL, NULL);
	    fclose(f);
	    char buffer[1024];
	    sprintf(buffer,"dot -Tsvg %s>/tmp/dot.svg && firefox /tmp/dot.svg",name)
	    system(buffer);
	*/
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
					getBasicBlocksFromExpr(mappedClone, metaNodes, allMappedNodes[i],data,varFilter);

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
	printf("Before:\n");
	graphPrint(mappedClone, printMappedNodesValue, printMappedEdge);
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
	printf("Final:\n");
	graphPrint(mappedClone, printMappedNode, printMappedEdge);
#endif
	__auto_type forwards = sortNodes(mappedClone);

	// Contains only metaNodes node
	// Init ins/outs to empty sets
	for (long i = strGraphNodeMappingPSize(forwards) - 1; i >= 0; i--) {
		char *hash = ptr2Str(forwards[i]);
		__auto_type find = mapBlockMetaNodeGet(metaNodes, hash);
		free(hash);

#if DEBUG_PRINT_ENABLE
		DEBUG_PRINT("Reseting in/outs of %s to empty.\n",
		            var2Str(find->node));
#endif

		// Could be start node
		if (!find)
			continue;

		find->block->in = NULL;
		find->block->out = NULL;
	}

	for (;;) {
		int changed = 0;

		for (long i = strGraphNodeMappingPSize(forwards) - 1; i >= 0; i--) {
			char *hash = ptr2Str(forwards[i]);
			__auto_type find = mapBlockMetaNodeGet(metaNodes, hash);
			free(hash);

			// Could be start node
			if (!find)
				continue;

			__auto_type oldIns = strVarClone(find->block->in);
			__auto_type oldOuts = strVarClone(find->block->out);

#if DEBUG_PRINT_ENABLE
			DEBUG_PRINT("Old ins of %s:\n", var2Str(find->node));
			printVars(oldIns);
			DEBUG_PRINT("Old outs of %s:\n", var2Str(find->node));
			printVars(oldOuts);
#endif

			// read[n] Union (out[n]-define[n])
			__auto_type newIns = strVarClone(find->block->read);
			__auto_type diff =
			    strVarSetDifference(strVarClone(find->block->out),
			                        find->block->define, (varRefCmpType)IRVarRefCmp);
			validateStrVarSet(diff);
			newIns = strVarSetUnion(newIns, diff, (varRefCmpType)IRVarRefCmp);
			validateStrVarSet(diff);
			newIns = strVarUnique(newIns, IRVarRefCmp, NULL);
			validateStrVarSet(newIns);
			strVarDestroy(&diff);

			// Union of successors insert
			strVar newOuts = NULL;
			__auto_type succs = graphNodeMappingOutgoingNodes(forwards[i]);
			for (long i = 0; i != strGraphNodeMappingPSize(succs); i++) {
				char *hash = ptr2Str(succs[i]);
				__auto_type find2 = mapBlockMetaNodeGet(metaNodes, hash);
				free(hash);

				// Skip if not found(may be start node)
				if (!find2)
					continue;

				// Union
				newOuts = strVarSetUnion(newOuts, find2->block->in,
				                         (varRefCmpType)IRVarRefCmp);
			}
			newOuts = strVarUnique(newOuts, IRVarRefCmp, NULL);
#if DEBUG_PRINT_ENABLE
			DEBUG_PRINT("New ins of %s:\n", var2Str(find->node));
			printVars(newIns);
			DEBUG_PRINT("New outs of %s:\n", var2Str(find->node));
			printVars(newOuts);
#endif

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

	//
	// Create interference graph
	//
	strGraphNodeIRLiveP retVal = NULL;
	strVarRefNodePair assocArray = NULL;
	for (long i = 0; i != strGraphNodePSize(forwards); i++) {
		// Find meta node
		char *hash = ptr2Str(forwards[i]);
		__auto_type find = mapBlockMetaNodeGet(metaNodes, hash);
		free(hash);

		// Could be start node
		if (!find)
			continue;

		//
		// We first search for the items and register them before we connect them
		//
		strGraphNodeIRLiveP liveAtOnce = NULL;
		for (long i2 = 0; i2 != strVarSize(find->block->in); i2++) {
			struct varRefNodePair pair;
			pair.ref = find->block->in[i2];
		registerLoop:;
			// Look for value
			__auto_type find2 =
			    strVarRefNodePairSortedFind(assocArray, pair, varRefNodePairCmp);
			if (NULL == find2) {
				// Create a node
				struct IRVarLiveness live;
				live.ref = find->block->in[i2];

				// Set the ndoe and insert
				pair.node = graphNodeIRLiveCreate(live, 0);
				assocArray =
				    strVarRefNodePairSortedInsert(assocArray, pair, varRefNodePairCmp);

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
				__auto_type ref1 =
				    debugGetPtrNameConst(graphNodeIRLiveValuePtr(liveAtOnce[i1])->ref);
				__auto_type ref2 =
				    debugGetPtrNameConst(graphNodeIRLiveValuePtr(liveAtOnce[i2])->ref);
				DEBUG_PRINT("Connecting %s to %s\n", ref1, ref2);
#endif
			}
		}

		strGraphNodeIRLivePDestroy(&liveAtOnce);
	}

	strGraphNodeMappingPDestroy(&forwards);
	mapBlockMetaNodeDestroy(metaNodes, killNode);

	//
	strGraphNodeIRLiveP allGraphs=NULL;
	for(;strGraphNodeIRLivePSize(retVal)!=0;) {
			//Use first node
			allGraphs=strGraphNodeIRLivePSortedInsert(allGraphs, retVal[0], (gnCmpType)ptrPtrCmp);

			//Filter out all accessible nodes from retVal,then we look for the next graph
			__auto_type asscesibleNodes=graphNodeIRLiveAllNodes(retVal[0]);
			retVal=strGraphNodeIRLivePSetDifference(retVal, asscesibleNodes, (gnCmpType)ptrPtrCmp);

			strGraphNodeIRLivePDestroy(&asscesibleNodes);
	}

	strGraphNodeIRLivePDestroy(&retVal);
	
	return allGraphs;
}
strGraphNodeIRLiveP IRInterferenceGraphFilter(graphNodeIR start,const void *data,int(*varFilter)(graphNodeIR node,const void *data)) {
		__auto_type mappedClone = graphNodeCreateMapping(start, 0);
		return __IRInterferenceGraphFilter(mappedClone,data,varFilter);
}
