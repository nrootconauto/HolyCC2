#include <IR.h>
#include <IRLiveness.h>
#include <assert.h>
#include <cleanup.h>
#define DEBUG_PRINT_ENABLE 1
#include <debugPrint.h>
static char *var2Str(graphNodeIR node) {
		char buffer[1024];
		struct IRNodeValue *val=(void*)graphNodeIRValuePtr(node);
		if(val->base.type!=IR_VALUE)
				return NULL;
		if(val->val.type!=IR_VAL_VAR_REF)
				return NULL;
		if(val->val.value.var.value.var->name)
				sprintf(buffer, "%s-%li", val->val.value.var.value.var->name,val->val.value.var.SSANum);
		else
				sprintf(buffer, "%p-%li", val->val.value.var.value.var,val->val.value.var.SSANum);

		return strcpy(malloc(strlen(buffer)+1),buffer);
}
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

static __thread void *varFilterData;
static __thread int (*varFilterPred)(graphNodeIR,const void*);
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
			if(varFilterPred) {
					if(!varFilterPred(graphEdgeIROutgoing(outgoingAssigns[0]),varFilterData))
							return 1;
			}
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
static __thread graphNodeMapping startAtAssign=NULL;
//TODO
static int untilAssignOutPred(const struct __graphNode *n,const struct __graphEdge *e,const void *data) {
		if(graphEdgeMappingOutgoing((graphEdgeMapping)e)==startAtAssign)
				return 1;
		
		graphNodeIR ir=*graphNodeMappingValuePtr((graphNodeMapping)n);
		if(!ir)
				return 0;
		strGraphEdgeIRP out CLEANUP(strGraphEdgeIRPDestroy)=graphNodeIROutgoing(ir);
		strGraphEdgeIRP outAssign CLEANUP(strGraphEdgeIRPDestroy)=IRGetConnsOfType(out, IR_CONN_DEST);

		int allNotExpr=1;
		for(long i=0;i!=strGraphEdgeIRPSize(out);i++)
				if(isExprEdge(out[i]))
						allNotExpr=0;
		if(allNotExpr)
				return 0;
		
		return 0==strGraphEdgeIRPSize(outAssign);
}
static void visitBackwardsByPrec(graphNodeMapping start,strGraphNodeIRP *result) {
		__auto_type ir=*graphNodeMappingValuePtr(start);
		strGraphEdgeIRP in CLEANUP(strGraphEdgeIRPDestroy)=IREdgesByPrec(ir);
		*result=strGraphNodeIRPAppendItem(*result, start);

		strGraphEdgeMappingP inM CLEANUP(strGraphEdgeMappingPDestroy)=graphNodeMappingIncoming(start);
		
		for(long i=0;i!=strGraphEdgeIRPSize(in);i++) {
				long mapEdgeI=-1;
				for(long m=0;m!=strGraphEdgeMappingPSize(inM);m++) {
						__auto_type ir=*graphNodeMappingValuePtr(graphEdgeMappingIncoming(inM[m]));
						if(ir==graphEdgeIRIncoming(in[i])) {
								mapEdgeI=m;
								break;
						}
				}
				assert(mapEdgeI!=-1);
				
				if(untilAssignOutPred(graphEdgeIRIncoming(inM[mapEdgeI]), inM[mapEdgeI], NULL))
						visitBackwardsByPrec(graphEdgeIRIncoming(inM[mapEdgeI]),result);
		}
}
static void bbFromExpr(graphNodeIR start,strBasicBlock *results,int(*varFilter)(graphNodeMapping,const void*),const void *data) {
		DEBUG_PRINT("ENTERING SUB-BLOCK %li\n", strBasicBlockSize(*results));
		//
		// startAtAssign marks the node we are starting at,it signifes to not stop at the incoming assign to start
		//
		startAtAssign=start;
		strGraphNodeMappingP exprNodes CLEANUP(strGraphNodeMappingPDestroy)=NULL;
		visitBackwardsByPrec(start,&exprNodes);
		struct basicBlock block;
		block.nodes = NULL;
		block.read = NULL;
		block.define = NULL;
		
		// Find read variable refs
		strGraphNodeMappingP doLater CLEANUP(strGraphNodeMappingPDestroy)=NULL;
		for (long i2 = 0; i2 != strGraphNodePSize(exprNodes); i2++) {

				__auto_type node = *graphNodeMappingValuePtr(exprNodes[i2]);
				struct IRNode *irNode =
						graphNodeIRValuePtr(*graphNodeMappingValuePtr(exprNodes[i2]));
				if (isVarNode(irNode)) {
						// If filter predicate provided,filter it out
						if (varFilter)
								if (!varFilter(*graphNodeMappingValuePtr(exprNodes[i2]), data))
										continue;

						// Check if node is asssigned to,or only read from
						strGraphEdgeIRP incoming = graphNodeIRIncoming(node);
						strGraphEdgeIRP incomingAssigns CLEANUP(strGraphEdgeIRPDestroy)=IRGetConnsOfType(incoming, IR_CONN_DEST);
						if (strGraphEdgeIRPSize(incomingAssigns)) {
								//
								// is Connected to dest node,so if is the target assign,mark as written into
								// otherwise mark for do later
								//
								if(*graphNodeMappingValuePtr(start)==node) {
										struct IRNode *irNode = (void *)graphNodeIRValuePtr(node);
										struct IRNodeValue *val=(void*)irNode;
										block.define = strVarSortedInsert(block.define, val->val.value.var,IRVarCmp);

#if DEBUG_PRINT_ENABLE
										DEBUG_PRINT("Writing to %s\n",
																						var2Str(*graphNodeMappingValuePtr(exprNodes[i2])));
#endif
										block.nodes=strGraphNodeIRPSortedInsert(block.nodes, exprNodes[i2], (gnCmpType)ptrPtrCmp); 
								} else {
										doLater=strGraphNodeMappingPAppendItem(doLater, exprNodes[i2]);
										goto registerRead;
								}
						} else {
						registerRead:;
								struct IRNodeValue *var = (void *)irNode;
								block.read =
										strVarSortedInsert(block.read, var->val.value.var, IRVarCmp);

#if DEBUG_PRINT_ENABLE
								DEBUG_PRINT("Reading from %s\n",
																				var2Str(*graphNodeMappingValuePtr(exprNodes[i2])));
#endif
								block.nodes=strGraphNodeIRPSortedInsert(block.nodes, exprNodes[i2], (gnCmpType)ptrPtrCmp);
						}
				} else
						block.nodes=strGraphNodeIRPSortedInsert(block.nodes, exprNodes[i2], (gnCmpType)ptrPtrCmp);
		}
		
		*results=strBasicBlockAppendItem(*results, ALLOCATE(block));
		for(long d=0;d!=strGraphNodeMappingPSize(doLater);d++)
				bbFromExpr(doLater[d], results, varFilter, data);
} 
strBasicBlock
IRGetBasicBlocksFromExpr(graphNodeIR dontDestroy, ptrMapBlockMetaNode metaNodes,
																									graphNodeMapping start,strGraphNodeMappingP *consumedNodes, const void *data,
                       int (*varFilter)(graphNodeIR var, const void *data)) {
		varFilterPred=varFilter;
		varFilterData=(void*)data;
	strGraphNodeMappingP nodes = visitAllAdjExprTo(start);
	if (nodes == NULL)
		return NULL;

	strBasicBlock retVal = NULL;
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
				sinks = strGraphNodeMappingPSortedInsert(sinks, nodes[i],
				                                         (gnCmpType)ptrPtrCmp);
#if DEBUG_PRINT_ENABLE
				DEBUG_PRINT("Assigning sink node :%s\n",
				            var2Str(*graphNodeMappingValuePtr(nodes[i])))
#endif
		}
	}

	//
	// Turn sinks and assigns into sub-blocks
	//
	
	// Concat assign nodes with sinks
	__auto_type oldSinks = strGraphNodeMappingPClone(sinks);
	strGraphNodeMappingP startFroms CLEANUP(strGraphNodeMappingPDestroy) = strGraphNodeMappingPClone(sinks);

	for (long i = 0; i != strGraphNodeMappingPSize(startFroms); i++) {
			strGraphNodeMappingP exprNodes CLEANUP(strGraphNodeIRPDestroy) = visitAllAdjExprTo(startFroms[i]);
			strBasicBlock resultBlocks CLEANUP(strBasicBlockDestroy)=NULL;
			bbFromExpr(startFroms[i],  &resultBlocks, varFilter, varFilterData);

			retVal=strBasicBlockAppendData(retVal, (const struct basicBlock **)resultBlocks, strBasicBlockSize(resultBlocks));
			for(long b=0;b!=strBasicBlockSize(resultBlocks);b++) {
					__auto_type blockPtr=resultBlocks[b];
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
	}
#if DEBUG_PRINT_ENABLE
	DEBUG_PRINT("Removing: %li items for block %li:\n",
	            strGraphNodeMappingPSize(nodes), strBasicBlockSize(retVal));
	for (long i = 0; i != strGraphNodeMappingPSize(nodes); i++) {
		DEBUG_PRINT("    - %s\n", var2Str(*graphNodeMappingValuePtr((nodes[i]))));
	}
#endif

	//
	// Replace with dummy,create a "chain" of topologically sorted y=*exp* operations(1 assign per node)
	// Then replace dummy with this chain
	//
	__auto_type dummy=graphNodeMappingCreate(NULL, 0);
	graphMappingReplaceNodes(nodes, dummy, NULL, NULL);

	graphNodeMapping last=NULL;
	graphNodeMapping first=NULL;
	for(long b=strBasicBlockSize(retVal)-1;b>=0;b--) {
			struct blockMetaNode pair;
			__auto_type meta=graphNodeMappingCreate(NULL, 0);
			pair.node=meta;
			pair.block=retVal[b];
			ptrMapBlockMetaNodeAdd(metaNodes, meta, pair);

			if(!first)
					first=meta;
			if(last)
					graphNodeMappingConnect(last, meta, NULL);
			last=meta;
	}

	//Transfer nodes then kill dummy
	strGraphEdgeMappingP inM CLEANUP(strGraphEdgeMappingPDestroy)=graphNodeMappingIncoming(dummy);
	strGraphEdgeMappingP outM CLEANUP(strGraphEdgeMappingPDestroy)=graphNodeMappingOutgoing(dummy);
	for(long i=0;i!=strGraphEdgePSize(inM);i++)
			graphNodeMappingConnect(graphEdgeMappingIncoming(inM[i]), first, NULL);
	for(long o=0;o!=strGraphEdgePSize(outM);o++)
			graphNodeMappingConnect(last,graphEdgeMappingOutgoing(outM[o]), NULL);

	graphNodeMappingKill(&dummy, NULL, NULL);
	
	if(consumedNodes)
			*consumedNodes=strGraphNodeMappingPClone(nodes);
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
