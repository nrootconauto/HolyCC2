#include <IR.h>
#include <IRFilter.h>
#include <SSA.h>
#include <assert.h>
#include <base64.h>
#include <cleanup.h>
#include <graphDominance.h>
#include <hashTable.h>
#include <limits.h>
#include <stdio.h>
#include <topoSort.h>
#define DEBUG_PRINT_ENABLE 1
#include <debugPrint.h>
static void debugPrintGraph(graphNodeMapping map) {
#if DEBUG_PRINT_ENABLE
	char *fn = tmpnam(NULL);
	IRGraphMap2GraphViz(map, "filter", fn, NULL, NULL, NULL, NULL);
	char buffer[1024];
	sprintf(buffer, "dot -Tsvg %s >/tmp/dot.svg && firefox /tmp/dot.svg &", fn);
	system(buffer);
#endif
}
#define GRAPHN_ALLOCATE(x) ({ __graphNodeCreate(&x, sizeof(x), 0); })
typedef int (*strGN_IRCmpType)(const strGraphNodeIRP *, const strGraphNodeIRP *);
typedef int (*geCmpType)(const graphEdgeIR *, const graphEdgeIR *);
typedef int (*gnCmpType)(const graphNodeIR *, const graphNodeIR *);

static char *ptr2Str(const void *a) {
	return base64Enc((void *)&a, sizeof(a));
}
static graphNodeIR createChoose(graphNodeIR insertBefore, strGraphNodeIRP pathPairs, struct parserVar *var) {
	// Make the choose node
	struct IRNodeChoose choose;
	choose.base.attrs = NULL;
	choose.base.type = IR_CHOOSE;
	choose.canidates = strGraphNodeIRPAppendData(NULL, pathPairs, strGraphNodeIRPSize(pathPairs));

	__auto_type chooseNode = GRAPHN_ALLOCATE(choose);

	// Create a variable ref
	struct IRNodeValue *firstNode = (void *)graphNodeIRValuePtr(insertBefore);
	__auto_type valueNode = IRCreateVarRef(var);

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
	
	if (ir->type == IR_VALUE) {
		struct IRNodeValue *val = (void *)ir;
		if (val->val.type != IR_VAL_VAR_REF)
			return 0;

		return 0 == IRVarCmpIgnoreVersion(expectedVar->var, &val->val.value.var);
	}

	return 0;
}
static int __isAssignedVar(struct varAndEnterPair *data, struct __graphNode *node, int ignoreChoose) {
	struct IRVar *expectedVar = data->var;

	if (ignoreChoose) {
		strGraphNodeIRP incoming CLEANUP(strGraphNodeIRPDestroy) = graphNodeIRIncomingNodes(node);
		if (1 == strGraphNodeIRPSize(incoming)) {
			if (graphNodeIRValuePtr(incoming[0])->type == IR_CHOOSE)
				return 0;
		}
	}

	struct IRNode *ir = graphNodeIRValuePtr((graphNodeIR)node);
	if (ir->type == IR_VALUE) {
		struct IRNodeValue *val = (void *)ir;
		if (val->val.type == IR_VAL_VAR_REF) {
			if (expectedVar->var == val->val.value.var.var)
				goto checkForAssign;
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
	strGraphEdgeIRP in CLEANUP(strGraphEdgeIRPDestroy) = graphNodeIRIncoming((graphNodeIR)node);
	strGraphEdgeIRP dstConns CLEANUP(strGraphEdgeIRPDestroy) = IRGetConnsOfType(in, IR_CONN_DEST);
	return 0 != strGraphEdgeIRPSize(dstConns);
}
static int isAssignedVar(graphNodeIR node, const void *data) {
	const struct varAndEnterPair *pair = data;
	return __isAssignedVar((void *)pair, node, 0);
}
static graphNodeMapping filterVarRefs(graphNodeIR enter, strGraphNodeIRP nodes, struct IRVar *var) {
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
static void versionAllVarsBetween(strGraphEdgeMappingP path, strGraphNodeP versionStarts) {
	// Ignore NULL path(path to self)
	if (!path)
		return;

	struct IRNodeValue *firstNode = (void *)graphNodeIRValuePtr(*graphNodeMappingValuePtr(graphEdgeMappingIncoming(path[0])));

	// Choose version
	long version = 0;
	// Choose version from  firstNode  (which points to assign) if is a variable
	if (firstNode->base.type == IR_VALUE) {
		if (firstNode->val.type == IR_VAL_VAR_REF) {
			version = firstNode->val.value.var.SSANum;
		}
	}

	for (long edgeI = 0; edgeI < strGraphEdgeMappingPSize(path); edgeI++) {
		__auto_type to = *graphNodeMappingValuePtr(graphEdgeMappingOutgoing(path[edgeI]));

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
static void strGraphPathDestroy2(strGraphPath *paths) {
	for (long p = 0; p != strGraphPathSize(*paths); p++)
		strGraphEdgePDestroy(&paths[0][p]);
	strGraphPathDestroy(paths);
}
static void graphNodeMappingDestroy2(graphNodeMapping *map) {
	graphNodeMappingKill(map, NULL, NULL);
}
static __thread graphNodeMapping __nullPathOrVersioned_IgnoreMapping=NULL;
static int nullPathOrVersioned(graphNodeMapping mapping,strGraphNodeIRP versioned) {
		strGraphEdgeMappingP out CLEANUP(strGraphEdgeMappingPDestroy)=graphNodeMappingOutgoing(mapping);
		if(strGraphEdgeMappingPSize(out)==0)
				return 1;
		if(mapping==__nullPathOrVersioned_IgnoreMapping)
				return 0;
		return NULL!=strGraphNodeIRPSortedFind(versioned, *graphNodeMappingValuePtr(mapping), (gnCmpType)ptrPtrCmp);
} 
static void SSAVersionVar(graphNodeIR start, struct IRVar *var) {
	//
	struct varAndEnterPair pair;
	pair.enter = start;
	pair.var = var;

	if(pair.var->var->name)
			if(0==strcmp("c",var->var->name))
					printf("dsgg");
	// Get references to all vars
 	graphNodeMapping varRefsG CLEANUP(graphNodeMappingDestroy2) = IRFilter(start, occurOfVar, &pair);
	strGraphNodeMappingP allVarRefs CLEANUP(strGraphNodeMappingPDestroy) = graphNodeMappingAllNodes(varRefsG);
	
	//debugPrintGraph(varRefsG);
	// Hash the vars
	ptrMapGraphNode IR2MappingNode = ptrMapGraphNodeCreate();
	for (long i = 0; i != strGraphNodeMappingPSize(allVarRefs); i++) {
		graphNodeIR sourceNode = *graphNodeMappingValuePtr(allVarRefs[i]);

		ptrMapGraphNodeAdd(IR2MappingNode, sourceNode, allVarRefs[i]);
	}

	graphNodeMapping varAssignG CLEANUP(graphNodeMappingDestroy2) = IRFilter(start, isAssignedVar, &pair);

	if (!varAssignG) {
		ptrMapGraphNodeDestroy(IR2MappingNode, NULL);
		return;
	}

	strGraphNodeMappingP allVarAssigns CLEANUP(strGraphNodeMappingPDestroy) = graphNodeMappingAllNodes(varAssignG);

	//debugPrintGraph(varAssignG);
	// Number the assigns
	long version = 1;
	for (long i = 0; i != strGraphNodeMappingPSize(allVarAssigns); i++) {
		__auto_type node = graphNodeIRValuePtr(*graphNodeMappingValuePtr(allVarAssigns[i]));
		// Ensure is a var
		if (node->type != IR_VALUE)
			continue;
		struct IRNodeValue *val = (void *)node;
		if (val->val.type != IR_VAL_VAR_REF)
			continue;

		val->val.value.var.SSANum = version++;
	}
	
	//
	// Get list of start nodes,we want to not stop at other versions starts while
	// going from one versions start to
	//
	strGraphNodeIRP versionStarts CLEANUP(strGraphNodeIRPDestroy) = NULL;
	for (long i = 0; i != strGraphNodeMappingPSize(allVarAssigns); i++) {
		__auto_type node = *graphNodeMappingValuePtr(allVarAssigns[i]);
		if(node==start)
				continue;
		versionStarts = strGraphNodeIRPSortedInsert(versionStarts, node, (gnCmpType)ptrPtrCmp);
	}

	for (long i = 0; i != strGraphNodeIRPSize(versionStarts); i++) {
		__auto_type mappedStart = *ptrMapGraphNodeGet(IR2MappingNode, versionStarts[i]);
		// Find all paths from start->end
		__nullPathOrVersioned_IgnoreMapping=mappedStart;
		//debugPrintGraph(mappedStart);
		strGraphEdgeMappingP  allPaths CLEANUP(strGraphEdgeMappingPDestroy) = graphAllEdgesBetween(mappedStart, versionStarts, (int(*)(const struct  __graphNode*,const void*))nullPathOrVersioned);
		for (long pathI = 0; pathI != strGraphEdgeMappingPSize(allPaths); pathI++) {
			// Insert versions between assigns
				__auto_type outIR=*graphNodeMappingValuePtr(graphEdgeMappingOutgoing(allPaths[pathI]));
				if(strGraphNodeIRPSortedFind(versionStarts, outIR, (gnCmpType)ptrPtrCmp))
						continue;
				struct IRNodeValue *in=(void*)graphNodeIRValuePtr(*graphNodeMappingValuePtr(mappedStart));
				struct IRNodeValue *o=(void*)graphNodeIRValuePtr(outIR);
				o->val.value.var.SSANum=in->val.value.var.SSANum;
		}
	}
	//debugPrintGraph(varRefsG);
}
STR_TYPE_DEF(struct IRVar, IRVar);
STR_TYPE_FUNCS(struct IRVar, IRVar);
struct varBlob {
	strIRVar read;
	strIRVar write;
};
PTR_MAP_FUNCS(graphNodeIR, graphNodeMapping, IR2Mapping);
PTR_MAP_FUNCS(graphNodeIR, struct varBlob, VarBlobByExprEnd);
/**
 * Returns list of new varaible references
 */
static strGraphNodeIRP IRSSACompute(graphNodeMapping start, struct IRVar *var, ptrMapVarBlobByExprEnd blobs) {
	//
	__auto_type frontiersToMaster = ptrMapChooseIncomingsCreate();
	__auto_type nodeKey2Ptr = ptrMapGraphNodeCreate();

	strGraphNodeMappingP mappedNodes CLEANUP(strGraphNodeMappingPDestroy) = graphNodeMappingAllNodes(start);
	__auto_type doms = graphComputeDominatorsPerNode(start); // TODO free
	__auto_type first = llDominatorsValuePtr(llDominatorsFind(doms, start, llDominatorCmp));
	__auto_type fronts = graphDominanceFrontiers(start, doms);

	strGraphNodeMappingP retVal = NULL;
	for (__auto_type node = llDomFrontierFirst(fronts); node != NULL; node = llDomFrontierNext(node)) {
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
				__auto_type incomingMapped = graphNodeMappingIncomingNodes(nodeValue->nodes[i]);
				strGraphNodeIRP incomingSources = NULL;
				for (long i = 0; i != strGraphNodeMappingPSize(incomingMapped); i++) {
					__auto_type sourceNode = *graphNodeMappingValuePtr(incomingMapped[i]);

					// Insert if doesnt exit
					if (!strGraphNodeIRPSortedFind(incomingSources, sourceNode, (gnCmpType)(ptrPtrCmp)))
						incomingSources = strGraphNodeIRPSortedInsert(incomingSources, sourceNode, (gnCmpType)(ptrPtrCmp));
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
	long kCount = ptrMapChooseIncomingsSize(frontiersToMaster);
	struct __graphNode *keys[kCount];
	ptrMapChooseIncomingsKeys(frontiersToMaster, keys);

	for (long i = 0; i != kCount; i++) {
		__auto_type masters = *ptrMapChooseIncomingsGet(frontiersToMaster, keys[i]);
		__auto_type masterNode = *ptrMapGraphNodeGet(nodeKey2Ptr, keys[i]);
		// Filter variables that correspond to var
		strGraphNodeIRP chooseFrom CLEANUP(strGraphNodeIRPDestroy) = NULL;
		for (long m = 0; m != strGraphNodeIRPSize(masters); m++) {
			strGraphNodeIRP exprNodes CLEANUP(strGraphNodeIRPDestroy) = IRStmtNodes(IREndOfExpr(masters[m]));
			for (long i = 0; i != strGraphNodeIRPSize(exprNodes); i++) {
				if (graphNodeIRValuePtr(exprNodes[i])->type != IR_VALUE)
					continue;

				struct IRNodeValue *val = (void *)graphNodeIRValuePtr(exprNodes[i]);
				if (val->val.type != IR_VAL_VAR_REF)
					continue;

				if (val->val.value.var.var == var->var)
					chooseFrom = strGraphNodeIRPSortedInsert(chooseFrom, exprNodes[i], (gnCmpType)ptrPtrCmp);
			}
		}
		createChoose(masterNode, chooseFrom, var->var);
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
static int ptrCmp(const void *a, const void *b) {
	if (a > b)
		return 1;
	else if (a < b)
		return -1;
	else
		return 0;
}
static void varBlobDestroy(struct varBlob *blob) {
	strIRVarDestroy(&blob->read);
	strIRVarDestroy(&blob->write);
}
static void transparentKillMapping(graphNodeMapping node) {
	strGraphEdgeMappingP incoming CLEANUP(strGraphEdgeMappingPDestroy) = graphNodeMappingIncoming(node);
	strGraphEdgeMappingP outgoing CLEANUP(strGraphEdgeMappingPDestroy) = graphNodeMappingOutgoing(node);
	for (long i1 = 0; i1 != strGraphEdgeMappingPSize(incoming); i1++)
		for (long i2 = 0; i2 != strGraphEdgeMappingPSize(outgoing); i2++)
			graphNodeMappingConnect(graphEdgeMappingIncoming(incoming[i1]), graphEdgeMappingOutgoing(outgoing[i2]), NULL);

	graphNodeMappingKill(&node, NULL, NULL);
}
static ptrMapVarBlobByExprEnd map2VarBlobs(graphNodeIR input, graphNodeMapping *retVal, strIRVar *allVars) {
	if (allVars)
		*allVars = NULL;
	ptrMapVarBlobByExprEnd m2VBlob = ptrMapVarBlobByExprEndCreate();

	graphNodeMapping mapping = graphNodeCreateMapping(input, 0);
	strGraphNodeMappingP allNodes CLEANUP(strGraphNodeMappingPDestroy) = graphNodeMappingAllNodes(mapping);
	ptrMapIR2Mapping ir2m = ptrMapIR2MappingCreate();
	for (long i = 0; i != strGraphNodeMappingPSize(allNodes); i++)
		ptrMapIR2MappingAdd(ir2m, *graphNodeMappingValuePtr(allNodes[i]), allNodes[i]);

	// Replace expressions with variable blobs
	strGraphNodeMappingP consumed CLEANUP(strGraphNodeMappingPDestroy) = NULL;
	for (long i = 0; i != strGraphNodeMappingPSize(allNodes); i++) {
		if (strGraphNodeMappingPSortedFind(consumed, allNodes[i], (gnCmpType)ptrPtrCmp))
			continue;
		// Preserve input node
		__auto_type enter = *graphNodeMappingValuePtr(allNodes[i]);
		if (*graphNodeMappingValuePtr(allNodes[i]) == input)
			continue;

		__auto_type node = *graphNodeMappingValuePtr(allNodes[i]);
		__auto_type end = IREndOfExpr(node);
		// Not moves means not in expression of last node of expr
		if (end == NULL)
			continue;
		if (end == node) {
			if (graphNodeIRValuePtr(node)->type != IR_VALUE)
				continue;
		}

		strGraphNodeIRP nodes CLEANUP(strGraphNodeIRPDestroy) = IRStmtNodes(end);
		DEBUG_PRINT("BLOB AT %p\n", end);
		struct varBlob blob;
		blob.read = NULL;
		blob.write = NULL;
		for (long n = 0; n != strGraphNodeIRPSize(nodes); n++) {
			struct IRNodeValue *val = (void *)graphNodeIRValuePtr(nodes[n]);
			if (val->base.type != IR_VALUE)
				continue;
			if (val->val.type != IR_VAL_VAR_REF)
				continue;

			strGraphEdgeIRP in CLEANUP(strGraphEdgeIRPDestroy) = graphNodeIRIncoming(nodes[n]);
			strGraphEdgeIRP inAssign CLEANUP(strGraphEdgeIRPDestroy) = IRGetConnsOfType(in, IR_CONN_DEST);
			if (strGraphEdgeIRPSize(inAssign)) {
				DEBUG_PRINT("WRITE:%p\n", nodes[n]);
				if (!strIRVarSortedFind(blob.write, val->val.value.var, IRVarCmp))
					blob.write = strIRVarSortedInsert(blob.write, val->val.value.var, IRVarCmp);
			} else {
				DEBUG_PRINT("READ:%p\n", nodes[n]);
				if (!strIRVarSortedFind(blob.read, val->val.value.var, IRVarCmp))
					blob.read = strIRVarSortedInsert(blob.read, val->val.value.var, IRVarCmp);
			}
			if (allVars) {
				strIRVar dummy CLEANUP(strIRVarDestroy) = strIRVarAppendItem(NULL, val->val.value.var);
				*allVars = strIRVarSetUnion(*allVars, dummy, IRVarCmp);
			}
		}

		strGraphNodeMappingP toReplace CLEANUP(strGraphNodeMappingPDestroy) = NULL;
		for (long i = 0; i != strGraphNodeIRPSize(nodes); i++) {
			toReplace = strGraphNodeMappingPSortedInsert(toReplace, *ptrMapIR2MappingGet(ir2m, nodes[i]), (gnCmpType)ptrPtrCmp);
			if (strGraphNodeMappingPSortedFind(consumed, *ptrMapIR2MappingGet(ir2m, nodes[i]), (gnCmpType)ptrPtrCmp))
				printf("fuffie\n");
		}

		// Dont replace start node
		__auto_type firstNodeM = *ptrMapIR2MappingGet(ir2m, input);
		strGraphNodeMappingP dummy CLEANUP(strGraphNodeMappingPDestroy) = strGraphNodeIRPAppendItem(NULL, firstNodeM);
		toReplace = strGraphNodeMappingPSetDifference(toReplace, dummy, (gnCmpType)ptrPtrCmp);
		qsort(toReplace, strGraphNodeMappingPSize(toReplace), sizeof(*toReplace), ptrPtrCmp);
		graphReplaceWithNode(toReplace, graphNodeMappingCreate(end, 0), NULL, NULL, sizeof(*graphNodeMappingValuePtr(NULL)));

		consumed = strGraphNodeMappingPSetUnion(consumed, toReplace, (gnCmpType)ptrPtrCmp);

		ptrMapVarBlobByExprEndAdd(m2VBlob, end, blob);
	}

	// Transparently remove all non-expression and non-start nodes
	__auto_type firstNodeM = *ptrMapIR2MappingGet(ir2m, input);
	consumed = strGraphNodeMappingPSortedInsert(consumed, firstNodeM, (gnCmpType)ptrPtrCmp);
	allNodes = strGraphNodeMappingPSetDifference(allNodes, consumed, (gnCmpType)ptrPtrCmp);
	for (long i = 0; i != strGraphNodeMappingPSize(allNodes); i++)
		transparentKillMapping(allNodes[i]);

	if (retVal) {
		*retVal = firstNodeM;
	} else {
		graphNodeMappingDestroy2(&mapping);
	}

	ptrMapIR2MappingDestroy(ir2m, NULL);
	return m2VBlob;
}
void filterVBlobMap4Var(graphNodeMapping start, struct IRVar *var, ptrMapVarBlobByExprEnd blobs) {
	strGraphNodeMappingP allNodes = graphNodeMappingAllNodes(start);
	for (long i = 0; i != strGraphNodeMappingPSize(allNodes); i++) {
		if (allNodes[i] == start)
			continue;
		__auto_type find = ptrMapVarBlobByExprEndGet(blobs, *graphNodeMappingValuePtr(allNodes[i]));
		if (strIRVarSortedFind(find->read, *var, IRVarCmp))
			continue;
		if (strIRVarSortedFind(find->write, *var, IRVarCmp))
			continue;
		transparentKillMapping(allNodes[i]);
	}
}
MAP_TYPE_DEF(strGraphNodeIRP, StrGNIR);
MAP_TYPE_FUNCS(strGraphNodeIRP, StrGNIR);
static void transparentKill(graphNodeIR node) {
	__auto_type incoming = graphNodeIRIncoming(node);
	__auto_type outgoing = graphNodeIROutgoing(node);
	for (long i1 = 0; i1 != strGraphEdgeIRPSize(incoming); i1++)
		for (long i2 = 0; i2 != strGraphEdgeIRPSize(outgoing); i2++)
			graphNodeIRConnect(graphEdgeIRIncoming(incoming[i1]), graphEdgeIROutgoing(outgoing[i2]), *graphEdgeIRValuePtr(incoming[i1]));

	graphNodeIRKill(&node, NULL, NULL);
}
static int __removeSameyChooses(graphNodeIR chooseNode, mapStrGNIR byNum) {
	struct IRNodeChoose *choose = (void *)graphNodeIRValuePtr(chooseNode);
	struct IRNodeValue *val = (void *)graphNodeIRValuePtr(choose->canidates[0]);
	;

	strGraphNodeIRP out CLEANUP(strGraphNodeIRPDestroy) = graphNodeIROutgoingNodes(chooseNode);
	__auto_type chooseAssign = out[0];
	struct IRNodeValue *val2 = (void *)graphNodeIRValuePtr(out[0]);
	long toReplace = val2->val.value.var.SSANum;

	long first = LONG_MIN;
	for (long c = 0; c < strGraphNodeIRPSize(choose->canidates); c++) {
		struct IRNodeValue *val = (void *)graphNodeIRValuePtr(choose->canidates[c]);
		long num = val->val.value.var.SSANum;
		if (num == toReplace) {
			continue;
		}
		if (first == LONG_MIN) {
			first = num;
			continue;
		}
		if (first != num)
			return 0;
	}
	if (first == LONG_MIN)
		first = toReplace;

	char bufferSrc[32];
	sprintf(bufferSrc, "%li", toReplace);
	__auto_type findSrc = *mapStrGNIRGet(byNum, bufferSrc);

	char bufferDst[32];
	sprintf(bufferDst, "%li", first);
	__auto_type findDest = mapStrGNIRGet(byNum, bufferDst);
	if (first != toReplace) {
		for (long r = 0; r != strGraphNodeIRPSize(findSrc); r++) {
			struct IRNodeValue *val = (void *)graphNodeIRValuePtr(findSrc[r]);
			val->val.value.var.SSANum = first;
			*findDest = strGraphNodeIRPSortedInsert(*findDest, findSrc[r], (gnCmpType)ptrPtrCmp);
		}
	}
	strGraphNodeIRPDestroy(mapStrGNIRGet(byNum, bufferSrc));

	transparentKill(chooseNode);
	// Assigned is the node being assigned into by the choose node,if no expressions reading/writing the node,can safley destroy it
	strGraphEdgeIRP assignedIn CLEANUP(strGraphEdgeIRPDestroy) = graphNodeIRIncoming(chooseAssign);
	strGraphEdgeIRP assignedOut CLEANUP(strGraphEdgeIRPDestroy) = graphNodeIROutgoing(chooseAssign);
	for (long i = 0; i != strGraphEdgeIRPSize(assignedIn); i++)
		if (IRIsExprEdge(*graphEdgeIRValuePtr(assignedIn[i])))
			goto end;
	for (long o = 0; o != strGraphEdgeIRPSize(assignedOut); o++)
		if (IRIsExprEdge(*graphEdgeIRValuePtr(assignedOut[o])))
			goto end;

	//Remove chooseAssign from byNum
	struct IRNodeValue* chooseAsnVal=(void*)graphNodeIRValuePtr(chooseAssign);
	if(chooseAsnVal->base.type==IR_VALUE) {
			if(chooseAsnVal->val.type==IR_VAL_VAR_REF) {
					__auto_type num=val->val.value.var.SSANum;
					char buffer[32];
					sprintf(buffer, "%li", first);
					__auto_type find=mapStrGNIRGet(byNum, buffer);
					*find=strGraphNodeIRPRemoveItem(*find, chooseAssign, (gnCmpType)ptrPtrCmp);
			}
	}

	transparentKill(chooseAssign);
end:
	return 1;
}
static void removeSameyChooses(graphNodeIR start, struct parserVar *var) {
	strGraphNodeIRP allNodes CLEANUP(strGraphNodeIRPDestroy) = graphNodeIRAllNodes(start);
	strGraphNodeIRP allChooses4Var CLEANUP(strGraphNodeIRPDestroy) = NULL;

	mapStrGNIR byNum = mapStrGNIRCreate();
	;
	for (long n = 0; n != strGraphNodeIRPSize(allNodes); n++) {
		if (graphNodeIRValuePtr(allNodes[n])->type == IR_CHOOSE) {
			strGraphNodeIRP out CLEANUP(strGraphNodeIRPDestroy) = graphNodeIROutgoingNodes(allNodes[n]);
			struct IRNodeValue *val = (void *)graphNodeIRValuePtr(out[0]);
			if (var == val->val.value.var.var)
				allChooses4Var = strGraphNodeIRPSortedInsert(allChooses4Var, allNodes[n], (gnCmpType)ptrPtrCmp);
		} else if (graphNodeIRValuePtr(allNodes[n])->type == IR_VALUE) {
			struct IRNodeValue *val = (void *)graphNodeIRValuePtr(allNodes[n]);
			if (var != val->val.value.var.var)
				continue;
			char buffer[32];
			sprintf(buffer, "%li", val->val.value.var.SSANum);
		loop:
			if (mapStrGNIRGet(byNum, buffer)) {
				__auto_type find = mapStrGNIRGet(byNum, buffer);
				*find = strGraphNodeIRPSortedInsert(*find, allNodes[n], (gnCmpType)ptrPtrCmp);
			} else {
				mapStrGNIRInsert(byNum, buffer, NULL);
				goto loop;
			}
		}
	}

removedLoop:;
	for (long c = 0; c != strGraphNodeIRPSize(allChooses4Var); c++) {
		if (__removeSameyChooses(allChooses4Var[c], byNum)) {
			allChooses4Var = strGraphNodeIRPRemoveItem(allChooses4Var, allChooses4Var[c], (gnCmpType)ptrPtrCmp);
			goto removedLoop;
		}
	}
}
void IRToSSA(graphNodeIR enter) {
	__auto_type nodes = graphNodeIRAllNodes(enter);
	if (strGraphNodeIRPSize(nodes) == 0)
		return;
	//
	// Find all the vars
	//
	strIRVar allVars CLEANUP(strIRVarDestroy) = NULL;
	graphNodeMapping filtered;
	ptrMapVarBlobByExprEnd blobs = map2VarBlobs(enter, &filtered, &allVars);

	//
	// Find SSA choose nodes for all vars
	//
	strGraphNodeIRP newNodes = NULL;
	for (long i = 0; i != strIRVarSize(allVars); i++) {
		__auto_type clone = graphNodeMappingClone(filtered);
		filterVBlobMap4Var(clone, &allVars[i], blobs);
#if DEBUG_PRINT_ENABLE
		//debugPrintGraph(clone);
#endif
		// Compute choose nodes
		strGraphNodeIRP chooses = IRSSACompute(clone, &allVars[i], blobs);
		graphNodeMappingKill(&clone, NULL, NULL);
		// Number the versions
		SSAVersionVar(enter, &allVars[i]);

		removeSameyChooses(enter, allVars[i].var);

		newNodes = strGraphNodeIRPConcat(newNodes, chooses);
	}

end:
	graphNodeMappingKillGraph(&filtered, NULL, NULL);
	ptrMapVarBlobByExprEndDestroy(blobs, (void (*)(void *))varBlobDestroy);
}
struct __pathToChoosePredPair {
	graphNodeIR start;
	graphNodeIR choose;
};
static int __lastIsNotChoose(const void *data, const strGraphEdgeIRP *path) {
	__auto_type last = path[0][strGraphEdgeIRPSize(*path) - 1];
	return graphNodeIRValuePtr(graphEdgeIROutgoing(last))->type != IR_CHOOSE;
}
static int __pathToChoosePred(const struct __graphNode *node, const void *data) {
	const struct __pathToChoosePredPair *pair = data;
	// Dont stop at start node
	if (node == pair->start)
		return 0;

	// Stop at other canidate's path
	__auto_type canidates = ((struct IRNodeChoose *)graphNodeIRValuePtr(pair->choose))->canidates;
	if (NULL != strGraphNodeIRPSortedFind(canidates, (graphNodeIR)node, (gnCmpType)ptrPtrCmp))
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
/**
 * Make a custom method for finding paths to choose, doing a raw search for all paths  to choose would be gaint(would go through expressions),
 * so do a depth first search moving through end of expression. untill we find the choose node
 */
struct IRPath {
	graphNodeIR start, end;
	graphEdgeIR edge;
};
static int IRPathCmp(const struct IRPath *a, const struct IRPath *b) {
	int cmp = ptrPtrCmp(a->start, b->start);
	if (cmp != 0)
		return cmp;
	cmp = ptrPtrCmp(a->end, b->end);
	if (cmp != 0)
		return cmp;
	return ptrPtrCmp(a->edge, b->edge);
}
STR_TYPE_DEF(struct IRPath, IRPath);
STR_TYPE_FUNCS(struct IRPath, IRPath);
STR_TYPE_DEF(strIRPath, IRPaths);
STR_TYPE_FUNCS(strIRPath, IRPaths);
static void strIRPathsDestroy2(strIRPaths *paths) {
	for (long i = 0; i != strIRPathsSize(*paths); i++)
		strIRPathDestroy(&paths[0][i]);
	strIRPathsDestroy(paths);
}
static void __paths2Choose(strGraphNodeIRP stopAtNodes,graphNodeIR node, graphNodeIR choose, strIRPath *currentPath, strIRPaths *paths) {
			__auto_type end = IREndOfExpr(node);
		for(long e=0;e<strIRPathSize(currentPath[0])-1;e++) {
				if(currentPath[0][e].start==node)
						return;
				if(currentPath[0][e].end==node)
						return;
				if(currentPath[0][e].start==end)
						return;
				if(currentPath[0][e].end==end)
						return;
		}

		if(strGraphNodeIRPSortedFind(stopAtNodes,node,(gnCmpType)ptrPtrCmp))
				return;
		
		if(end!=node) {
				strGraphNodeIRP nodes CLEANUP(strGraphNodeIRPDestroy)=IRStmtNodes(end);
				for(long n=0;n!=strGraphNodeIRPSize(nodes);n++) {
						if(strGraphNodeIRPSortedFind(stopAtNodes,nodes[n],(gnCmpType)ptrPtrCmp))
								return;
				}
		}

		if (strIRPathSize(*currentPath) != 0)
		node = (!end) ? node : end;
	
	if (IRStmtStart(node) == choose) {
		for (long p = 0; p != strIRPathsSize(*paths); p++) {
			if (strIRPathSize(*currentPath) != strIRPathSize(paths[0][p]))
				continue;
			// Dont repeat path
			if (0 == memcmp(paths[0][p], *currentPath, strIRPathSize(*currentPath) * sizeof(*currentPath)))
				return;
		}
		*paths = strIRPathsAppendItem(*paths, strIRPathClone(*currentPath));
		return;
	};
	strGraphEdgeIRP out CLEANUP(strGraphEdgeIRPDestroy) = graphNodeIROutgoing(node);
	for (long i = 0; i != strGraphEdgeIRPSize(out); i++) {
		__auto_type outNode = graphEdgeIROutgoing(out[i]);
		struct IRPath edge = {node, outNode, out[i]};
		*currentPath = strIRPathAppendItem(*currentPath, edge);
		__paths2Choose(stopAtNodes,outNode, choose, currentPath, paths);
		*currentPath = strIRPathPop(*currentPath, NULL);
	}
}
static void paths2Choose(strGraphNodeIRP stopAt,strIRPaths *paths, graphNodeIR node, graphNodeIR choose) {
	strIRPath path CLEANUP(strIRPathDestroy) = NULL;
	__paths2Choose(stopAt,node, choose, &path, paths);
}
static int edgeEqual(void *a, void *b) {
	return *(const enum IRConnType *)a == *(const enum IRConnType *)b;
}
static strIRPath sharedEndPath(strIRPaths paths) {
	strIRPath retVal = NULL;
	if (strIRPathsSize(paths) == 1)
		return NULL;
	for (long off = 1;; off++) {
		if (strIRPathSize(paths[0]) - off <= 0)
			goto end;
		struct IRPath firstPath = paths[0][strIRPathSize(paths[0]) - off];
		for (long p = 1; p < strIRPathsSize(paths); p++) {
			if (strIRPathSize(paths[p]) - off <= 0)
				goto end;
			if (0 != IRPathCmp(&firstPath, &paths[p][strIRPathSize(paths[p]) - off]))
				goto end;
		}

		retVal = strIRPathAppendItem(retVal, firstPath);
	}
end:
	return retVal;
}
PTR_MAP_FUNCS(graphEdgeIR, strIRPaths, IRPathsByGN);
static ptrMapIRPathsByGN groupPathsByEnd(ptrMapIRPathsByGN *retVal, strIRPaths paths) {
	for (long p = 0; p != strIRPathsSize(paths); p++) {
		__auto_type last = paths[p][strIRPathSize(paths[p]) - 1];
	loop:;
		__auto_type find = ptrMapIRPathsByGNGet(*retVal, last.edge);
		if (!find) {
			ptrMapIRPathsByGNAdd(*retVal, last.edge, NULL);
			goto loop;
		}
		*find = strIRPathsAppendItem(*find, paths[p]);
	}
	return *retVal;
}
void IRSSAReplaceChooseWithAssigns(graphNodeIR node, strGraphNodeIRP *replaced) {
#if DEBUG_PRINT_ENABLE
	__auto_type map = graphNodeCreateMapping(node, 1);
	//debugPrintGraph(map);
#endif

	assert(graphNodeIRValuePtr(node)->type == IR_CHOOSE);
	struct IRNodeChoose *choose = (void *)graphNodeIRValuePtr(node);

	strGraphNodeIRP outgoing CLEANUP(strGraphNodeIRPDestroy) = graphNodeIROutgoingNodes(node);
	assert(graphNodeIRValuePtr(outgoing[0])->type == IR_VALUE);
	struct IRNodeValue *value = (void *)graphNodeIRValuePtr(outgoing[0]);
	assert(value->val.type == IR_VAL_VAR_REF);
	struct IRVar *var = &value->val.value.var;

	__auto_type assignInto = outgoing[0];
	// Outgoing that must be assign (TODO validate this)

	strIRPaths paths CLEANUP(strIRPathsDestroy2) = NULL;
	for (long i = 0; i != strGraphNodeIRPSize(choose->canidates); i++) {
		struct IRNodeValue *canVal = (void *)graphNodeIRValuePtr(choose->canidates[i]);
		if (0 == IRVarCmp(&canVal->val.value.var, var))
			continue;
		strGraphNodeIRP stopAt CLEANUP(strGraphNodeIRPDestroy)=strGraphNodeIRPClone(choose->canidates);
		stopAt=strGraphNodeIRPRemoveItem(stopAt, choose->canidates[i], (gnCmpType)ptrPtrCmp);
		paths2Choose(stopAt,&paths, choose->canidates[i], node);
	}

	strIRPaths order CLEANUP(strIRPathsDestroy) = NULL;
	ptrMapIRPathsByGN pathsByEnd = ptrMapIRPathsByGNCreate();
	groupPathsByEnd(&pathsByEnd, paths);
	for (; ptrMapIRPathsByGNSize(pathsByEnd);) {
		long keyCount = ptrMapIRPathsByGNSize(pathsByEnd);
		strGraphEdgeIRP dumpTo CLEANUP(strGraphEdgeIRPDestroy) = strGraphEdgeIRPResize(NULL, keyCount);
		ptrMapIRPathsByGNKeys(pathsByEnd, dumpTo);

		// This stores the new ends for the next loop
		ptrMapIRPathsByGN pathsByEnd2 = ptrMapIRPathsByGNCreate();
		for (long k = 0; k != keyCount; k++) {
			strIRPaths paths CLEANUP(strIRPathsDestroy) = *ptrMapIRPathsByGNGet(pathsByEnd, dumpTo[k]);

			// Do set union of paths
			strIRPath shared CLEANUP(strIRPathDestroy) = sharedEndPath(paths);
			qsort(shared, strIRPathSize(shared), (sizeof *shared), (int (*)(const void *, const void *))IRPathCmp);

			// Remove common end path
			for (long i = 0; i != strIRPathsSize(paths); i++) {
				long removed = 0;
			loop:
				if (i >= strIRPathsSize(paths))
					break;
				for (long e = strIRPathSize(paths[i]) - 1; e >= 0; e--) {
					if (strIRPathSortedFind(shared, paths[i][e], IRPathCmp)) {
						if (strIRPathSize(paths[i]) == 1)
							break;
						struct IRPath popped;
						paths[i] = strIRPathPop(paths[i], &popped);
						removed++;
					} else
						break;
				}
				if (!removed) {
					long len = strIRPathsSize(paths);
					order = strIRPathsAppendItem(order, paths[i]);
					memmove(&paths[i], &paths[i + 1], (len - 1 - i) * (sizeof paths[i]));
					paths = strIRPathsResize(paths, len - 1);
					goto loop;
				}
			}
			groupPathsByEnd(&pathsByEnd2, paths);
		}
		ptrMapIRPathsByGNDestroy(pathsByEnd, NULL);
		pathsByEnd = pathsByEnd2;
	}

	//
	// Filter out paths that appear  in the path of another path
	//
	strGraphEdgeIRP consumedEdges CLEANUP(strGraphEdgeIRPDestroy) = NULL;

	for (long o = 0; o != strIRPathsSize(order); o++) {
		// First item in path is always node targeted by choose
		__auto_type assign = IRCreateAssign(IRCloneNode(order[o][0].start, IR_CLONE_NODE, NULL), IRCloneNode(assignInto, IR_CLONE_NODE, NULL));
		__auto_type endEdge = order[o][strIRPathSize(order[o]) - 1].edge;
		if (strGraphEdgeIRPSortedFind(consumedEdges, endEdge, (geCmpType)ptrPtrCmp))
			continue;
		consumedEdges = strGraphEdgeIRPSortedInsert(consumedEdges, endEdge, (geCmpType)ptrPtrCmp);
		__auto_type edgeOut = graphEdgeIROutgoing(endEdge);
		__auto_type edgeIn = graphEdgeIRIncoming(endEdge);
		/**
		 * If edge is a flow edge,just insert "inbetween" the edge,
		 * otherwise if is a source edge,we need to bring the end value
		 * to the end of the connection(expression edge may end at if or switch),so
		 * make a copy of the the end value and paste it at the end.( To
		 * simulate a direct connection)
		 *
		 * For example,if we need to insert an assign at a=10 at here
		 *   if(a=10) a;
		 * The graph will need to look like this
		 *     10
		 *     ||
		 *     \/
		 *  a(ver 1)
		 * // Inserted //
		 *     ||
		 *     \/
		 *  a(ver 1)
		 *     ||
		 *     \/
		 *  a(ver2)
		 *     ||
		 *     \/
		 *  a(ver 1) //Copy of a(ver 1)
		 * // End of Inserted //
		 *    ||
		 *    \/
		 *    if
		 */
		// Insert assign in the edge
		__auto_type edgeValue = *graphEdgeIRValuePtr(endEdge);
		switch (edgeValue) {
		case IR_CONN_COND: {
			// See long comment above
			__auto_type inNode = graphEdgeIRIncoming(endEdge);
			// If edge doesn't start with a value,make once
			if (graphNodeIRValuePtr(inNode)->type != IR_VALUE) {
				__auto_type var = IRCreateVirtVar(IRNodeType(inNode));
				__auto_type varNode1 = IRCreateVarRef(var);
				IRInsertAfter(inNode, varNode1, varNode1, IR_CONN_DEST);

				// Find new edge corresponding to old edge as old edge is destroyed
				strGraphEdgeIRP in CLEANUP(strGraphEdgeIRPDestroy) = graphNodeIROutgoing(varNode1);
				strGraphEdgeIRP inOfType CLEANUP(strGraphEdgeIRPDestroy) = IRGetConnsOfType(in, edgeValue);
				assert(strGraphEdgeIRPSize(inOfType) == 1);
				endEdge = inOfType[0];
				inNode = graphEdgeIRIncoming(endEdge);
			}
			__auto_type clone = IRCloneNode(inNode, IR_CLONE_NODE, NULL);
			graphEdgeIRKill(graphEdgeIRIncoming(endEdge), edgeOut, &edgeValue, edgeEqual, NULL);
			graphNodeIRConnect(edgeIn, IRStmtStart(assign), IR_CONN_FLOW);
			graphNodeIRConnect(IREndOfExpr(assign), clone, IR_CONN_FLOW);
			graphNodeIRConnect(clone, edgeOut, edgeValue);
			break;
		}
		default: {
			// Insert inbetween
			graphEdgeIRKill(graphEdgeIRIncoming(endEdge), edgeOut, &edgeValue, edgeEqual, NULL);
			graphNodeIRConnect(edgeIn, IRStmtStart(assign), edgeValue);
			graphNodeIRConnect(IREndOfExpr(assign), edgeOut, IR_CONN_FLOW);
			break;
		}
		}

#if DEBUG_PRINT_ENABLE
		__auto_type map = graphNodeCreateMapping(edgeOut, 1);
		//debugPrintGraph(map);
#endif
	}

	__auto_type endOfExpression = IREndOfExpr(node);
	// Returned
	strGraphNodeIRP exprNodes = IRStatementNodes(IRStmtStart(node), endOfExpression);
	__auto_type dummy = IRCreateLabel();
	graphReplaceWithNode(exprNodes, dummy, NULL, NULL, sizeof(graphEdgeIR));
	transparentKill(dummy);

	if (replaced)
		*replaced = exprNodes;
	else
		strGraphNodeIRPDestroy(&exprNodes);
}
