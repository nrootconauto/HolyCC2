#include <IR.h>
#include <IRLiveness.h>
#include <assert.h>
#include <cleanup.h>
typedef int (*gnCmpType)(const graphNodeMapping *, const graphNodeMapping *);
typedef int (*varRefCmpType)(const struct IRVar **, const struct IRVar **);
#define ALLOCATE(x)                                                            \
	({                                                                           \
		typeof(x) *ptr = malloc(sizeof(x));                                        \
		*ptr = x;                                                                  \
		ptr;                                                                       \
	})
static void basicBlockAttrDestroy(struct IRAttr *attr);
static int IRVarRefCmp(const struct IRVar **a, const struct IRVar **b) {
	return IRVarCmp(a[0], b[0]);
}

static int isExprEdge(graphEdgeIR edge) {
		return IRIsExprEdge(*graphEdgeIRValuePtr(edge));
};

static int untilWriteOut(const struct __graphNode *node,
                       const struct __graphEdge *edge, const void *data) {
	//
	// Edge may be a "virtual"(mapped edge from replace that has no value)
	// fail is edge value isnt present
	//
	__auto_type edgeValue = *graphEdgeMappingValuePtr((void *)edge);
	if (!edgeValue)
		return 0;

	if (!isExprEdge(edgeValue))
		return 0;

	strGraphEdgeIRP outgoing CLEANUP(strGraphEdgeIRPDestroy) =
	    graphNodeIROutgoing(*graphNodeMappingValuePtr((graphNodeMapping)node));
	strGraphEdgeIRP outgoingAssigns CLEANUP(strGraphEdgeIRPDestroy)=IRGetConnsOfType(outgoing, IR_CONN_DEST);
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

static int isVarNode(const struct IRNode *irNode) {
	if (irNode->type == IR_VALUE) {
		struct IRNodeValue *val = (void *)irNode;
		if (val->val.type == IR_VAL_VAR_REF)
			return 1;
	}

	return 0;
}
static void appendToNodes(struct __graphNode *node, void *data) {
	strGraphNodeMappingP *nodes = data;
	*nodes = strGraphNodeMappingPSortedInsert(*nodes, node, (gnCmpType)ptrPtrCmp);
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
			return NULL;
		}

	return visited;
}

strBasicBlock
IRGetBasicBlocksFromExpr(graphNodeIR dontDestroy, ptrMapBlockMetaNode metaNodes,
                       graphNodeMapping start, const void *data,
                       int (*varFilter)(graphNodeIR var, const void *data)) {
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
		__auto_type irNode = graphNodeIRValuePtr(ir);
		// Check if var
		if (isVarNode(irNode)) {
			// If filter predicate provided,filter it out
			if (varFilter)
				if (!varFilter(ir, data))
					continue;
			
			// Check if assign var
			strGraphEdgeIRP incomingEdges =
			    graphNodeIRIncoming(*graphNodeMappingValuePtr(nodes[i]));
			strGraphEdgeIRP incomingAssigns CLEANUP(strGraphEdgeIRPDestroy)=IRGetConnsOfType(incomingEdges, IR_CONN_DEST);
				if (strGraphEdgeIRPSize(incomingAssigns)) {
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
		strGraphEdgeIRP outgoing = NULL;
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
	strGraphNodeMappingP startFroms CLEANUP(strGraphNodeMappingPDestroy) = strGraphNodeMappingPConcat(strGraphNodeMappingPClone(assignNodes), sinks);

	for (long i = 0; i != strGraphNodeMappingPSize(startFroms); i++) {
		strGraphNodeMappingP exprNodes CLEANUP(strGraphNodeIRPDestroy) = strGraphNodeIRPAppendItem(NULL, startFroms[i]);
		//If is an assign(not a sink),starts search for until-assigns from incoming as assign node is assigned into
		if(NULL!=strGraphNodeMappingPSortedFind(assignNodes, startFroms[i], (gnCmpType)ptrPtrCmp)) {
				strGraphNodeMappingP incomingAssign CLEANUP(strGraphNodeIRPDestroy)=graphNodeMappingIncomingNodes(startFroms[i]);
				exprNodes=strGraphNodeMappingPSortedInsert(exprNodes, incomingAssign[0], (gnCmpType)ptrPtrCmp);
				graphNodeMappingVisitBackward(incomingAssign[0], &exprNodes, untilWriteOut,
		                              appendToNodes);
		} else {
				graphNodeMappingVisitBackward(startFroms[i], &exprNodes, untilWriteOut,
		                              appendToNodes);
		}
		struct basicBlock block;
		block.nodes = strGraphNodeMappingPClone(exprNodes);
		block.read = NULL;
		block.define = NULL;

		// Check if node is asssigned to,or only read from
		__auto_type node = *graphNodeMappingValuePtr(startFroms[i]);
		strGraphEdgeIRP incoming = graphNodeIRIncoming(node);
		strGraphNodeIRP writeNodes CLEANUP(strGraphNodeIRPDestroy)=NULL;

		strGraphEdgeIRP incomingAssigns CLEANUP(strGraphEdgeIRPDestroy)=IRGetConnsOfType(incoming, IR_CONN_DEST);
		if (strGraphEdgeIRPSize(incomingAssigns)) {
				// is Connected to dest node
				struct IRNode *irNode = (void *)graphNodeIRValuePtr(node);
				assert(isVarNode(irNode));
				block.define = strVarAppendItem(
																																				NULL, &((struct IRNodeValue *)irNode)->val.value.var);
		}

		//
		// IF isnt a sink,is part of the original assign nodes
		//
		if (NULL == strGraphNodeMappingPSortedFind(oldSinks, startFroms[i],
		                                           (gnCmpType)ptrPtrCmp)) {
			__auto_type defineRef = &((struct IRNodeValue *)graphNodeIRValuePtr(
			                              *graphNodeMappingValuePtr(startFroms[i])))
			                             ->val.value.var;
			block.define = strVarAppendItem(NULL, defineRef);
#if DEBUG_PRINT_ENABLE
			DEBUG_PRINT("Writing to %s\n",
			            var2Str(*graphNodeMappingValuePtr(startFroms[i])));
#endif
			writeNodes=strGraphNodeIRPSortedInsert(writeNodes, startFroms[i], (gnCmpType)ptrPtrCmp);
		}

		// Find read variable refs
		for (long i2 = 0; i2 != strGraphNodePSize(exprNodes); i2++) {

			struct IRNode *irNode =
			    graphNodeIRValuePtr(*graphNodeMappingValuePtr(exprNodes[i2]));
			if (isVarNode(irNode)) {
				// If filter predicate provided,filter it out
				if (varFilter)
					if (!varFilter(exprNodes[i2], data))
						continue;

				//Ensure isn't a writen-into node of the sub-block
				if(startFroms[i]==exprNodes[i2])
						continue;
				
				struct IRNodeValue *var = (void *)irNode;

				block.read =
				    strVarSortedInsert(block.read, &var->val.value.var, IRVarRefCmp);
#if DEBUG_PRINT_ENABLE
				DEBUG_PRINT("Reading from %s\n",
				            var2Str(*graphNodeMappingValuePtr(exprNodes[i2])));
#endif
			}
		}

		// Add to  consumed nodes
		consumedNodes = strGraphNodeMappingPSetUnion(consumedNodes, exprNodes,
		                                             (gnCmpType)ptrPtrCmp);
		// Add assign node to consumed nodes
		consumedNodes = strGraphNodeIRPSortedInsert(consumedNodes, startFroms[i],
		                                            (gnCmpType)ptrPtrCmp);

		// Push to blocks
		__auto_type blockPtr = ALLOCATE(block);
		retVal = strBasicBlockAppendItem(retVal, blockPtr);
		// Add attribute to nodes
		blockPtr->refCount = 0;
		for (long i = 0; i != strGraphNodeMappingPSize(blockPtr->nodes); i++) {
			struct IRAttrBasicBlock bbAttr;
			bbAttr.base.name = IR_ATTR_BASIC_BLOCK;
			bbAttr.base.destroy=basicBlockAttrDestroy;
			bbAttr.block = blockPtr;

			__auto_type attr = __llCreate(&bbAttr, sizeof(bbAttr));
			IRAttrReplace(*graphNodeMappingValuePtr(blockPtr->nodes[i]), attr);

			blockPtr->refCount++;
		}
	}
#if DEBUG_PRINT_ENABLE
	DEBUG_PRINT("Removing: %li items for block %li:\n",
	            strGraphNodeMappingPSize(nodes), strBasicBlockSize(retVal));
	for (long i = 0; i != strGraphNodeMappingPSize(nodes); i++) {
		DEBUG_PRINT("    - %s\n", var2Str(*graphNodeMappingValuePtr((nodes[i]))));
	}
#endif
	// Remove consumed from possible sinks
	nodes = strGraphNodeMappingPSetDifference(nodes, consumedNodes,
	                                          (gnCmpType)ptrPtrCmp);
#if DEBUG_PRINT_ENABLE
	DEBUG_PRINT("Removing: %li items for block %li:\n",
	            strGraphNodeMappingPSize(nodes), strBasicBlockSize(retVal));
	for (long i = 0; i != strGraphNodeMappingPSize(nodes); i++) {
		DEBUG_PRINT("    - %s\n", var2Str(*graphNodeMappingValuePtr((nodes[i]))));
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
		replacedNodes=strGraphNodeMappingPSetUnion(replacedNodes, toReplace, (gnCmpType)ptrPtrCmp);
		// Remove dontDestroy from to replace
		strGraphNodeMappingP dummy =
		    strGraphNodeMappingPAppendItem(NULL, dontDestroy);
		toReplace = strGraphNodeMappingPSetDifference(toReplace, dummy,
		                                              (gnCmpType)ptrPtrCmp);
#if DEBUG_PRINT_ENABLE
		DEBUG_PRINT("Replacing: %li items:\n", strGraphNodeMappingPSize(toReplace));
		for (long i2 = 0; i2 != strGraphNodeMappingPSize(toReplace); i2++) {
			DEBUG_PRINT("    - %s\n",
			            var2Str(*graphNodeMappingValuePtr((toReplace[i2]))));
		}
#endif

		// Create meta node entry
		__auto_type metaNode = graphNodeMappingCreate(NULL, 0);
		struct blockMetaNode pair;
		pair.block = retVal[i];
		pair.node = metaNode;
		ptrMapBlockMetaNodeAdd(metaNodes, metaNode, pair);

#if DEBUG_PRINT_ENABLE
		char buffer[128];
		sprintf(buffer, "Block %s  metanode",
		        var2Str(*graphNodeMappingValuePtr(toReplace[0])));
		debugAddPtrName(metaNode, buffer);
#endif

		// Replace with metaNode
		graphMappingReplaceNodes(toReplace, metaNode, NULL, NULL);
	}

	return retVal;
}
static void basicBlockDestroy(struct basicBlock *bb) {
	if (0 >= bb->refCount--) {
		strVarDestroy(&bb->define);
		strVarDestroy(&bb->in);
		strVarDestroy(&bb->out);
		strVarDestroy(&bb->read);
		strGraphNodeIRPDestroy(&bb->nodes);

		free(bb);
	}
}
static void basicBlockAttrDestroy(struct IRAttr *attr) {
		struct IRAttrBasicBlock *bbAttr=(void*)attr;
		basicBlockDestroy(bbAttr->block);
}
