#include <IR.h>
#include <IRFilter.h>
#include <SSA.h>
#include <cleanup.h>
#include <assert.h>
#include <base64.h>
#include <graphDominance.h>
#include <hashTable.h>
#include <stdio.h>
#include <topoSort.h>
//#define DEBUG_PRINT_ENABLE 1
#include <debugPrint.h>
#define GRAPHN_ALLOCATE(x) ({ __graphNodeCreate(&x, sizeof(x), 0); })
typedef int (*strGN_IRCmpType)(const strGraphNodeIRP *,
                               const strGraphNodeIRP *);
typedef int (*geCmpType)(const graphEdgeIR *, const graphEdgeIR *);
typedef int (*gnCmpType)(const graphNodeIR *, const graphNodeIR *);

static char *ptr2Str(const void *a) { return base64Enc((void *)&a, sizeof(a)); }
static graphNodeIR createChoose(graphNodeIR insertBefore,
                                strGraphNodeIRP pathPairs) {
	// Make the choose node
	struct IRNodeChoose choose;
	choose.base.attrs = NULL;
	choose.base.type = IR_CHOOSE;
	choose.canidates = strGraphNodeIRPAppendData(NULL, pathPairs,
	                                             strGraphNodeIRPSize(pathPairs));

	__auto_type chooseNode = GRAPHN_ALLOCATE(choose);

	// Create a variable ref
	struct IRNodeValue *firstNode = (void *)graphNodeIRValuePtr(insertBefore);
	__auto_type valueNode=IRCreateVarRef(firstNode->val.value.var.value.var);

	graphNodeIRConnect(chooseNode, valueNode, IR_CONN_DEST);

	__auto_type stmtStart = IRStmtStart(IREndOfExpr(insertBefore));
	IRInsertBefore(stmtStart, chooseNode, valueNode, IR_CONN_FLOW);
	return valueNode;
}
static int ptrPtrCmp(const void *a, const void *b) {
	if (*(void **)a > *(void **)b)
		return 1;
	else if (*(void **)a < *(void **)b)
		return -1;
	return 0;
}

static void getAllVisit(struct __graphNode *node, void *all) {
	strGraphNodeP *ptr = all;
	*ptr = strGraphNodePAppendItem(*ptr, node);
}
static int alwaysTrue(const struct __graphNode *n, const struct __graphEdge *e,
                      const void *data) {
	return 1;
}
struct SSANode {
	graphNodeIR assignNode;
	struct IRValue *var;
};
GRAPH_TYPE_DEF(struct SSANode, void *, SSANode);
GRAPH_TYPE_FUNCS(struct SSANode, void *, SSANode);

PTR_MAP_FUNCS(struct __graphNode *, strGraphNodeP, ChooseIncomings);
struct varAndEnterPair {
	graphNodeIR enter;
	struct IRVar *var;
};
static int occurOfVar(graphNodeIR node, const void *pair) {
	const struct varAndEnterPair *expectedVar = pair;
	struct IRNode *ir = graphNodeIRValuePtr((graphNodeIR)node);
	if (node == expectedVar->enter)
		return 1;

	if (ir->type == IR_VALUE) {
		struct IRNodeValue *val = (void *)ir;
		if (val->val.type != IR_VAL_VAR_REF)
			return 0;

		return 0 == IRVarCmpIgnoreVersion(expectedVar->var, &val->val.value.var);
	}

	return 0;
}
static int __isAssignedVar(struct varAndEnterPair *data,
                           struct __graphNode *node, int ignoreChoose) {
	struct IRVar *expectedVar = data->var;

	if (ignoreChoose) {
		strGraphNodeIRP incoming = graphNodeIRIncomingNodes(node);
		if (1 == strGraphNodeIRPSize(incoming)) {
			if (graphNodeIRValuePtr(incoming[0])->type == IR_CHOOSE)
				return 0;
		}
	}

	struct IRNode *ir = graphNodeIRValuePtr((graphNodeIR)node);
	if (ir->type == IR_VALUE) {
		struct IRNodeValue *val = (void *)ir;
		if (val->val.type == IR_VAL_VAR_REF) {
			if (expectedVar->type == val->val.value.var.type) {
				if (expectedVar->type == IR_VAR_MEMBER) {
					// TODO check equal
				} else if (expectedVar->type == IR_VAR_VAR) {
					if (expectedVar->value.var == val->val.value.var.value.var)
						goto checkForAssign;
				}
			}
		}
	}

	// Check if enter-node
	if (((struct varAndEnterPair *)data)->enter == node)
		return 1;

	return 0;
checkForAssign:;
	//
	// Check if assigned to choose node
	//
	strGraphEdgeIRP in = graphNodeIRIncoming((graphNodeIR)node);
	strGraphEdgeIRP dstConns = IRGetConnsOfType(in, IR_CONN_DEST);
	return 0 != strGraphEdgeIRPSize(dstConns);
}
static int isAssignedVar(graphNodeIR node, const void *data) {
	const struct varAndEnterPair *pair = data;
	return __isAssignedVar((void *)pair, node, 0);
}
static graphNodeMapping filterVarRefs(graphNodeIR enter, strGraphNodeIRP nodes,
                                      struct IRVar *var) {
	// First find the references to the var and jumps.
	struct varAndEnterPair pair;
	pair.enter = enter;
	pair.var = var;
	return IRFilter(enter, occurOfVar, &pair);
}
static char *node2Str(struct __graphNode *node) {
	node = *graphNodeMappingValuePtr(node);
	char buffer[128];
	sprintf(buffer, "%p", node);

	__auto_type find = mapStrGet(nodeNames, buffer);
	if (!find)
		return NULL;

	__auto_type str = *find;
	char *retVal = malloc(strlen(str) + 1);
	strcpy(retVal, str);

	return retVal;
}
static void versionAllVarsBetween(strGraphEdgeMappingP path,
                                  strGraphNodeP versionStarts) {
	// Ignore NULL path(path to self)
	if (!path)
		return;

	struct IRNodeValue *firstNode = (void *)graphNodeIRValuePtr(
	    *graphNodeMappingValuePtr(graphEdgeMappingIncoming(path[0])));

	// Choose version
	long version = 0;
	// Choose version from  firstNode  (which points to assign) if is a variable
	if (firstNode->base.type == IR_VALUE) {
		if (firstNode->val.type == IR_VAL_VAR_REF) {
			version = firstNode->val.value.var.SSANum;
		}
	}

	for (long edgeI = 0; edgeI < strGraphEdgeMappingPSize(path); edgeI++) {
		__auto_type to =
		    *graphNodeMappingValuePtr(graphEdgeMappingOutgoing(path[edgeI]));

		// Dont descent into other versions' domains
		if (strGraphNodeIRPSortedFind(versionStarts, to, (gnCmpType)ptrPtrCmp))
			break;

		struct IRNodeValue *val = (void *)graphNodeIRValuePtr(to);
		// Ensure is variable
		if (val->base.type != IR_VALUE)
			continue;
		if (val->val.type != IR_VAL_VAR_REF)
			continue;

		// Assign version
		val->val.value.var.SSANum = version;
	}
}
static void SSAVersionVar(graphNodeIR start, struct IRVar *var) {
	//
	struct varAndEnterPair pair;
	pair.enter = start;
	pair.var = var;

	// Get references to all vars
	__auto_type varRefsG = IRFilter(start, occurOfVar, &pair);
	__auto_type allVarRefs = graphNodeMappingAllNodes(varRefsG);
	// graphPrint(varRefsG, node2Str);
	// Hash the vars
	ptrMapGraphNode IR2MappingNode = ptrMapGraphNodeCreate();
	for (long i = 0; i != strGraphNodeMappingPSize(allVarRefs); i++) {
		graphNodeIR sourceNode = *graphNodeMappingValuePtr(allVarRefs[i]);
		
		ptrMapGraphNodeAdd(IR2MappingNode, sourceNode, allVarRefs[i]);
	}

	__auto_type varAssignG = IRFilter(start, isAssignedVar, &pair);

	if (!varAssignG) {
		ptrMapGraphNodeDestroy(IR2MappingNode, NULL);
		return;
	}

	__auto_type allVarAssigns = graphNodeMappingAllNodes(varAssignG);

	// graphPrint(varAssignG, node2Str);
	// Number the assigns
	long version = 1;
	for (long i = 0; i != strGraphNodeMappingPSize(allVarAssigns); i++) {
		__auto_type node =
		    graphNodeIRValuePtr(*graphNodeMappingValuePtr(allVarAssigns[i]));
		// Ensure is a var
		if (node->type != IR_VALUE)
			continue;
		struct IRNodeValue *val = (void *)node;
		if (val->val.type != IR_VAL_VAR_REF)
			continue;

		val->val.value.var.SSANum = version++;
	}

	//
	// First do a set union of all paths though the assigned vars. Then number the
	// items between each edge's node(is a mapped graph) of the assign graph.
	//
	strGraphEdgeMappingP un = NULL;
	strGraphNodeMappingP allMappingNodes CLEANUP(strGraphNodeMappingPDestroy)=NULL;
	for (long i = 0; i != strGraphNodeMappingPSize(allMappingNodes); i++) {
			strGraphEdgeMappingP allEdges CLEANUP(strGraphEdgeMappingPDestroy)=graphNodeMappingOutgoing(allMappingNodes[i]);
		un = strGraphEdgePSetUnion(un, allEdges,
		                           (int (*)(const struct __graphEdge **,
		                                    const struct __graphEdge **))ptrPtrCmp);
	}

	//
	// Get list of start nodes,we want to not stop at other versions starts while
	// going from one versions start to
	//
	strGraphNodeIRP versionStarts = NULL;
	for (long i = 0; i != strGraphNodeMappingPSize(allVarAssigns); i++) {
		__auto_type node = *graphNodeMappingValuePtr(allVarAssigns[i]);
		versionStarts =
		    strGraphNodeIRPSortedInsert(versionStarts, node, (gnCmpType)ptrPtrCmp);
	}

	for (long i = 0; i != strGraphEdgeMappingPSize(un); i++) {
		__auto_type start =
		    *graphNodeMappingValuePtr(graphEdgeMappingIncoming(un[i]));
		__auto_type end =
		    *graphNodeMappingValuePtr(graphEdgeMappingOutgoing(un[i]));

		__auto_type mappedStart = *ptrMapGraphNodeGet(IR2MappingNode, start);
		__auto_type mappedEnd = *ptrMapGraphNodeGet(IR2MappingNode, end);

		// Find all paths from start->end
		__auto_type allPaths = graphAllPathsTo(mappedStart, mappedEnd);
		for (long pathI = 0; pathI != strGraphPathSize(allPaths); pathI++) {
			// Insert versions between assigns

			//!!! Last edge points to next assign and we dont want to overwrite assign
			allPaths[pathI] = strGraphEdgePPop(allPaths[pathI], NULL);

			versionAllVarsBetween(allPaths[pathI], versionStarts);
		}
	}
	//
	// Find exit nodes
	//
	for (long i = 0; i != strGraphNodeMappingPSize(versionStarts); i++) {
		if (versionStarts[i] == start)
			continue;

		__auto_type mappedNode = *ptrMapGraphNodeGet(IR2MappingNode, versionStarts[i]);

		// Find all null paths from assign
		__auto_type nullPaths = graphAllPathsTo(mappedNode, NULL);

		// Version vars from assign to exit
		for (long i2 = 0; i2 != strGraphPathSize(nullPaths); i2++) {
			versionAllVarsBetween(nullPaths[i2], versionStarts);
		}
	}
}
/**
 * Returns list of new varaible references
 */
static strGraphNodeIRP IRSSACompute(graphNodeMapping start, struct IRVar *var) {
	// graphPrint(start, node2Str);
	//
	__auto_type frontiersToMaster = ptrMapChooseIncomingsCreate();
	__auto_type nodeKey2Ptr = ptrMapGraphNodeCreate();

	__auto_type mappedNodes = graphNodeMappingAllNodes(start);
	__auto_type doms = graphComputeDominatorsPerNode(start);
	__auto_type first =
	    llDominatorsValuePtr(llDominatorsFind(doms, start, llDominatorCmp));
	__auto_type fronts = graphDominanceFrontiers(start, doms);

	strGraphNodeMappingP retVal = NULL;
	for (__auto_type node = llDomFrontierFirst(fronts); node != NULL;
	     node = llDomFrontierNext(node)) {
		__auto_type nodeValue = llDomFrontierValuePtr(node);

		// Not all nodes have dominators,so if no dominators continue
		if (!strGraphNodePSize(nodeValue->nodes))
			continue;

		//
		// Register the items at the frontier node
		//

		for (long i = 0; i != strGraphNodePSize(nodeValue->nodes); i++) {
			// Register the master node if doesnt exist
			__auto_type frontier = *graphNodeMappingValuePtr(nodeValue->nodes[i]);
		loop:;
			__auto_type find = ptrMapChooseIncomingsGet(frontiersToMaster, frontier);
			if (!find) {
				// Make a list of incoming nodes to frontier
				__auto_type incomingMapped =
				    graphNodeMappingIncomingNodes(nodeValue->nodes[i]);
				strGraphNodeIRP incomingSources = NULL;
				for (long i = 0; i != strGraphNodeMappingPSize(incomingMapped); i++) {
					__auto_type sourceNode = *graphNodeMappingValuePtr(incomingMapped[i]);

					// Insert if doesnt exit
					if (!strGraphNodeIRPSortedFind(incomingSources, sourceNode,
					                               (gnCmpType)(ptrPtrCmp)))
						incomingSources = strGraphNodeIRPSortedInsert(
						    incomingSources, sourceNode, (gnCmpType)(ptrPtrCmp));
				}

				ptrMapChooseIncomingsAdd(frontiersToMaster, frontier, incomingSources);
				ptrMapGraphNodeAdd(nodeKey2Ptr, frontier, frontier);
				goto loop;
			}
		}
	}

	//
	// Insert the choose nodes
	//
	long kCount=ptrMapChooseIncomingsSize(frontiersToMaster);
	struct __graphNode *keys[kCount];
	ptrMapChooseIncomingsKeys(frontiersToMaster, keys);

	for (long i = 0; i != kCount; i++) {
		__auto_type masters = *ptrMapChooseIncomingsGet(frontiersToMaster, keys[i]);
		__auto_type masterNode =
		    *ptrMapGraphNodeGet(nodeKey2Ptr, keys[i]); // TODO change to frontier
		createChoose(masterNode, masters);
	}

	ptrMapChooseIncomingsDestroy(frontiersToMaster, NULL);
	ptrMapGraphNodeDestroy(nodeKey2Ptr, NULL);
	return retVal;
}
static int filterVars(graphNodeIR node, const void *data) {
	__auto_type ir = graphNodeIRValuePtr(node);
	if (ir->type == IR_VALUE) {
		struct IRNodeValue *irValue = (void *)ir;
		if (irValue->val.type == IR_VAL_VAR_REF)
			return 1;
	}

	return 0;
}
STR_TYPE_DEF(struct IRVar, IRVar);
STR_TYPE_FUNCS(struct IRVar, IRVar);
static int ptrCmp(const void *a, const void *b) {
	if (a > b)
		return 1;
	else if (a < b)
		return -1;
	else
		return 0;
}
struct varBlob {
		strIRVar read;
		strIRVar write;
};
PTR_MAP_FUNCS(graphNodeIR, struct varBlob, VarBlobByExprEnd);
static void varBlobDestroy(struct varBlob *blob) {
		strIRVarDestroy(&blob->read);
		strIRVarDestroy(&blob->write);
}
static void transparentKillMapping(graphNodeMapping node) {
			__auto_type incoming = graphNodeMappingIncoming(node);
	__auto_type outgoing = graphNodeMappingOutgoing(node);
	for (long i1 = 0; i1 != strGraphEdgeMappingPSize(incoming); i1++)
			for (long i2 = 0; i2 != strGraphEdgeMappingPSize(outgoing); i2++)
					graphNodeMappingConnect(graphEdgeMappingIncoming(incoming[i1]),
																													graphEdgeMappingOutgoing(outgoing[i2]), NULL);

	graphNodeMappingKill(&node, NULL, NULL);
}
PTR_MAP_FUNCS(graphNodeIR, graphNodeMapping, IR2Mapping);
static ptrMapVarBlobByExprEnd map2VarBlobs(graphNodeIR input,graphNodeMapping *retVal,strIRVar *allVars) {
		if(allVars)
				*allVars=NULL;
		ptrMapVarBlobByExprEnd m2VBlob=ptrMapVarBlobByExprEndCreate();

		__auto_type mapping=graphNodeCreateMapping(input, 0);
		strGraphNodeMappingP allNodes CLEANUP(strGraphNodeMappingPDestroy)=graphNodeMappingAllNodes(mapping);
		ptrMapIR2Mapping ir2m=ptrMapIR2MappingCreate();
		for(long i=0;i!=strGraphNodeMappingPSize(allNodes);i++)
				ptrMapIR2MappingAdd(ir2m, *graphNodeMappingValuePtr(allNodes[i]), allNodes[i]);

		//Replace expressions with variable blobs
		strGraphNodeMappingP consumed CLEANUP(strGraphNodeMappingPDestroy)=NULL;
		for(long i=0;i!=strGraphNodeMappingPSize(allNodes);i++) {
				if(strGraphNodeMappingPSortedFind(consumed, allNodes[i], (gnCmpType)ptrPtrCmp))
						continue;
				//Preserve input node
				__auto_type enter=*graphNodeMappingValuePtr(allNodes[i]);
				if(*graphNodeMappingValuePtr(allNodes[i])==input)
						continue;
				
				__auto_type node=*graphNodeMappingValuePtr(allNodes[i]);
				__auto_type end=IREndOfExpr(node);
				//Not moves means not in expression of last node of expr
				if(end==NULL)
						continue;
				if(end==node) {
						if(graphNodeIRValuePtr(node)->type!=IR_VALUE)
								continue;
				}
				
				strGraphNodeIRP nodes CLEANUP(strGraphNodeIRPDestroy)=IRStmtNodes(end);
				DEBUG_PRINT("BLOB AT %p\n", end);
				struct varBlob blob;
				blob.read=NULL;
				blob.write=NULL;
				for(long n=0;n!=strGraphNodeIRPSize(nodes);n++) {
						struct IRNodeValue *val=(void*)graphNodeIRValuePtr(nodes[n]);
						if(val->base.type!=IR_VALUE)
								continue;
						if(val->val.type!=IR_VAL_VAR_REF)
								continue;

						strGraphEdgeIRP in CLEANUP(strGraphEdgeIRPDestroy)=graphNodeIRIncoming(nodes[n]);
						strGraphEdgeIRP inAssign CLEANUP(strGraphEdgeIRPDestroy)=IRGetConnsOfType(in, IR_CONN_DEST);
						if(strGraphEdgeIRPSize(inAssign)) {
								DEBUG_PRINT("WRITE:%p\n", nodes[n]);
								if(!strIRVarSortedFind(blob.write, val->val.value.var, IRVarCmp))
										blob.write=strIRVarSortedInsert(blob.write, val->val.value.var, IRVarCmp);
						} else {
								DEBUG_PRINT("READ:%p\n", nodes[n]);
								if(!strIRVarSortedFind(blob.read, val->val.value.var, IRVarCmp))
										blob.read=strIRVarSortedInsert(blob.read, val->val.value.var, IRVarCmp);
						}
						if(allVars) {
								strIRVar dummy CLEANUP(strIRVarDestroy)=strIRVarAppendItem(NULL, val->val.value.var);
								*allVars=strIRVarSetUnion(*allVars, dummy, IRVarCmp);
						}
				}

				strGraphNodeMappingP toReplace CLEANUP(strGraphNodeMappingPDestroy)=NULL;
				for(long i=0;i!=strGraphNodeIRPSize(nodes);i++) {
						toReplace=strGraphNodeMappingPSortedInsert(toReplace, *ptrMapIR2MappingGet(ir2m, nodes[i]), (gnCmpType)ptrPtrCmp);
						if(strGraphNodeMappingPSortedFind(consumed, *ptrMapIR2MappingGet(ir2m, nodes[i]), (gnCmpType)ptrPtrCmp))
								printf("fuffie\n");
				}

				//Dont replace start node
				__auto_type firstNodeM=*ptrMapIR2MappingGet(ir2m, input);
				strGraphNodeMappingP dummy CLEANUP(strGraphNodeMappingPDestroy)=strGraphNodeIRPAppendItem(NULL, firstNodeM);
				toReplace=strGraphNodeMappingPSetDifference(toReplace, dummy, (gnCmpType)ptrPtrCmp);
				qsort(toReplace, strGraphNodeMappingPSize(toReplace), sizeof(*toReplace), ptrPtrCmp);
				graphReplaceWithNode(toReplace, graphNodeMappingCreate(end, 0), NULL, NULL, sizeof(*graphNodeMappingValuePtr(NULL)));

				consumed=strGraphNodeMappingPSetUnion(consumed, toReplace, (gnCmpType)ptrPtrCmp);

				ptrMapVarBlobByExprEndAdd(m2VBlob, end, blob);
		}

		//Transparently remove all non-expression and non-start nodes
		__auto_type firstNodeM=*ptrMapIR2MappingGet(ir2m, input);
		consumed=strGraphNodeMappingPSortedInsert(consumed, firstNodeM, (gnCmpType)ptrPtrCmp);
		allNodes=strGraphNodeMappingPSetDifference(allNodes, consumed, (gnCmpType)ptrPtrCmp);
		for(long i=0;i!=strGraphNodeMappingPSize(allNodes);i++)
				transparentKillMapping(allNodes[i]);
		
		if(retVal)
				*retVal=firstNodeM;

		ptrMapIR2MappingDestroy(ir2m, NULL);
		return m2VBlob;
}
void filterVBlobMap4Var(graphNodeMapping start,struct IRVar *var,ptrMapVarBlobByExprEnd blobs) {
		strGraphNodeMappingP allNodes=graphNodeMappingAllNodes(start);
		for(long i=0;i!=strGraphNodeMappingPSize(allNodes);i++) {
				if(allNodes[i]==start)
						continue;
				__auto_type find=ptrMapVarBlobByExprEndGet(blobs, *graphNodeMappingValuePtr(allNodes[i]));
				if(strIRVarSortedFind(find->read, *var, IRVarCmp))
						continue;
				if(strIRVarSortedFind(find->write, *var, IRVarCmp))
						continue;
				transparentKillMapping(allNodes[i]);
		}
}
void IRToSSA(graphNodeIR enter) {
	__auto_type nodes = graphNodeIRAllNodes(enter);
	if (strGraphNodeIRPSize(nodes) == 0)
		return;
	//
	// Find all the vars
	//
	strIRVar allVars CLEANUP(strIRVarDestroy)= NULL;
	graphNodeMapping filtered;
	ptrMapVarBlobByExprEnd blobs=map2VarBlobs(enter,  &filtered, &allVars);
	/*	{
			char *fn=tmpnam(NULL);
			IRGraphMap2GraphViz(filtered, "filter", fn, NULL,NULL,NULL,NULL);
			char buffer[1024];
			sprintf(buffer, "dot -Tsvg %s >/tmp/dot.svg && firefox /tmp/dot.svg &", fn);
			system(buffer);
			}*/

	//
	// Find SSA choose nodes for all vars
	//
	strGraphNodeIRP newNodes = NULL;
	for (long i = 0; i != strIRVarSize(allVars); i++) {
			__auto_type clone=graphNodeMappingClone(filtered);
			filterVBlobMap4Var(clone, &allVars[i], blobs);
			// Compute choose nodes
		strGraphNodeIRP chooses = IRSSACompute(clone, &allVars[i]);
		// Number the versions
		SSAVersionVar(enter, &allVars[i]);

		newNodes = strGraphNodeIRPConcat(newNodes, chooses);
	}

end:
	graphNodeMappingKillGraph(&filtered, NULL, NULL);
	ptrMapVarBlobByExprEndDestroy(blobs, (void(*)(void*))varBlobDestroy);
}
struct __pathToChoosePredPair {
	graphNodeIR start;
	graphNodeIR choose;
};
static int __lastIsNotChoose(const void *data, const strGraphEdgeIRP *path) {
	__auto_type last = path[0][strGraphEdgeIRPSize(*path) - 1];
	return graphNodeIRValuePtr(graphEdgeIROutgoing(last))->type != IR_CHOOSE;
}
static int __pathToChoosePred(const struct __graphNode *node,
                              const void *data) {
	const struct __pathToChoosePredPair *pair = data;
	// Dont stop at start node
	if (node == pair->start)
		return 0;

	// Stop at other canidate's path
	__auto_type canidates =
	    ((struct IRNodeChoose *)graphNodeIRValuePtr(pair->choose))->canidates;
	if (NULL != strGraphNodeIRPSortedFind(canidates, (graphNodeIR)node,
	                                      (gnCmpType)ptrPtrCmp))
		return 1;

	// Stop at choose;
	if (node == pair->choose)
		return 1;

	return 0;
}
static int __edgeIsEqual(void *a, void *b) {
	return *(graphEdgeIR *)a == *(graphEdgeIR *)b;
}
static int gnIRCmpVar(const graphNodeIR *a, const graphNodeIR *b) {
	struct IRNodeValue *A = (void *)graphNodeIRValuePtr(*a);
	struct IRNodeValue *B = (void *)graphNodeIRValuePtr(*b);

	return IRVarCmp(&A->val.value.var, &B->val.value.var);
}
static void transparentKill(graphNodeIR node) {
	__auto_type incoming = graphNodeIRIncoming(node);
	__auto_type outgoing = graphNodeIROutgoing(node);
	for (long i1 = 0; i1 != strGraphEdgeIRPSize(incoming); i1++)
		for (long i2 = 0; i2 != strGraphEdgeIRPSize(outgoing); i2++)
			graphNodeIRConnect(graphEdgeIRIncoming(incoming[i1]),
			                   graphEdgeIROutgoing(outgoing[i2]), IR_CONN_FLOW);

	graphNodeIRKill(&node, NULL, NULL);
}
/**
	* Make a custom method for finding paths to choose, doing a raw search for all paths  to choose would be gaint(would go through expressions),
	* so do a depth first search moving through end of expression. untill we find the choose node
*/
struct IRPath {
		graphNodeIR start,end;
};
static int IRPathCmp(const struct IRPath *a,const struct IRPath *b) {
		int cmp=ptrPtrCmp(a->start, b->start);
		if(cmp!=0)
				return cmp;
		return ptrPtrCmp(a->end, b->end);
}
STR_TYPE_DEF(struct IRPath,IRPath);
STR_TYPE_FUNCS(struct IRPath,IRPath);
STR_TYPE_DEF(strIRPath,IRPaths);
STR_TYPE_FUNCS(strIRPath,IRPaths);
static void strIRPathDestroy2(strIRPaths *paths) {
		for(long i=0;i!=strIRPathsSize(*paths);i++)
				strIRPathDestroy(&paths[0][i]);
		strIRPathsDestroy(paths);
}
static void __paths2Choose(graphNodeIR node,graphNodeIR choose,strIRPath *currentPath,strIRPaths *paths) {
		__auto_type end=IREndOfExpr(node);
		end=(!end)?node:node;
		if(IRStmtStart(end)==choose) {
				*paths=strIRPathsAppendItem(*paths, strIRPathClone(*currentPath));
				return;
		}
		strGraphNodeIRP out=graphNodeIROutgoingNodes(node);
		for(long i=0;i!=strGraphNodeIRPSize(out);i++) {
				struct IRPath edge={node,out[i]};
				*currentPath=strIRPathAppendItem(*currentPath, edge);
				__paths2Choose(out[i],choose,currentPath,paths);
				*currentPath=strIRPathPop(*currentPath, NULL);
		}
}
static strIRPaths paths2Choose(graphNodeIR node,graphNodeIR choose) {
		strIRPath path CLEANUP(strIRPathDestroy)=NULL;
		strIRPaths paths=NULL;
		__paths2Choose(node,choose,&path,&paths);
		return paths;
}

void IRSSAReplaceChooseWithAssigns(graphNodeIR node,
                                   strGraphNodeIRP *replaced) {
		{
			char *fn=tmpnam(NULL);
			IRGraphMap2GraphViz(node, "filter", fn, NULL,NULL,NULL,NULL);
			char buffer[1024];
			sprintf(buffer, "dot -Tsvg %s >/tmp/dot.svg && firefox /tmp/dot.svg &", fn);
			system(buffer);
	}

		assert(graphNodeIRValuePtr(node)->type == IR_CHOOSE);
	struct IRNodeChoose *choose = (void *)graphNodeIRValuePtr(node);

	strGraphNodeIRP outgoing CLEANUP(strGraphNodeIRPDestroy) = graphNodeIROutgoingNodes(node);
	assert(graphNodeIRValuePtr(outgoing[0])->type==IR_VALUE);
	struct IRNodeValue *value=(void*)graphNodeIRValuePtr(outgoing[0]);
	assert(value->val.type==IR_VAL_VAR_REF);
	struct IRVar *var=&value->val.value.var;
	// Outgoing that must be assign (TODO validate this)

	strIRPaths paths CLEANUP(strIRPathDestroy2)=NULL;
	for(long i=0;i!=strGraphNodeIRPSize(choose->canidates);i++) {
			strIRPaths paths2 =paths2Choose(choose->canidates[i], node);
			paths=strIRPathsConcat(paths, paths2);
	}

	//Find shared "end" portion of paths,remove the shared portion
	for(long offset=0;strIRPathsSize(paths);offset++) {
	filterLoop:;
			if(strIRPathsSize(paths)==1) {
					__auto_type top=strIRPathSize(paths[0])-1;
					//Insert assign after of paths[p]
					__auto_type assign=IRCreateAssign(paths[0][0].start, IRCloneNode(paths[0][0].start, IR_CLONE_NODE, NULL));
					IRInsertAfter(IREndOfExpr(paths[0][top].end), IRStmtStart(assign), IREndOfExpr(assign), IR_CONN_FLOW);
					goto removeItem;
			}
			
			int firstRun=1;
			struct IRPath *endPath=NULL;
			for(long p=0;p!=strIRPathsSize(paths);p++) {
					if(offset>=strIRPathSize(paths[p])) {
							//Insert assign after of paths[p]
							__auto_type assign=IRCreateAssign(paths[p][0].start, IRCloneNode(paths[p][0].start, IR_CLONE_NODE, NULL));
							IRInsertAfter(IREndOfExpr(paths[p][0].start), IRStmtStart(assign), IREndOfExpr(assign), IR_CONN_FLOW);
							goto removeItem;
					} else if(firstRun) {
							__auto_type top=strIRPathSize(paths[p])-1;
							__auto_type endPath2=&paths[p][top];
							if(0!=IRPathCmp(endPath, endPath2)) {
									//Insert assign after of paths[p]
									__auto_type assign=IRCreateAssign(paths[p][0].start, IRCloneNode(paths[p][0].start, IR_CLONE_NODE, NULL));
									IRInsertAfter(IREndOfExpr(paths[p][top].end), IRStmtStart(assign), IREndOfExpr(assign), IR_CONN_FLOW);
									goto removeItem;
							}
							firstRun=0;
					}	 else if(!firstRun) {
							endPath=&paths[p][strIRPathSize(paths[p])-1];
					}
					continue;
			removeItem:
					strIRPathDestroy(&paths[p]);
					memmove(paths[p], paths[p+1], strIRPathsSize(paths)-1);
					goto filterLoop;
			}
			break;
	}
	
	__auto_type endOfExpression = IREndOfExpr(node);
	strGraphNodeIRP exprNodes CLEANUP(strGraphNodeIRPDestroy)=
	       IRStatementNodes(IRStmtStart(node), endOfExpression);
	__auto_type dummy = IRCreateLabel();
	graphReplaceWithNode(exprNodes, dummy, NULL, NULL, sizeof(graphEdgeIR));
	transparentKill(dummy);

	{
			char *fn=tmpnam(NULL);
			IRGraphMap2GraphViz(dummy, "filter", fn, NULL,NULL,NULL,NULL);
			char buffer[1024];
			sprintf(buffer, "dot -Tsvg %s >/tmp/dot.svg && firefox /tmp/dot.svg &", fn);
			system(buffer);
	}
	
	if (replaced)
		*replaced = exprNodes;
	else
		strGraphNodeIRPDestroy(&exprNodes);
}
