#include <IR.h>
#include <IRFilter.h>
#include <SSA.h>
#include <assert.h>
#include <base64.h>
#include <graphDominance.h>
#include <hashTable.h>
#include <stdio.h>
#include <topoSort.h>
#define GRAPHN_ALLOCATE(x) ({ __graphNodeCreate(&x, sizeof(x), 0); })
typedef int (*strGN_IRCmpType)(const strGraphNodeIRP *,
                               const strGraphNodeIRP *);
typedef int (*geCmpType)(const graphEdgeIR *, const graphEdgeIR *);
typedef int (*gnCmpType)(const graphNodeIR *, const graphNodeIR *);

static char *ptr2Str(const void *a) { return base64Enc((void *)&a, sizeof(a)); }
MAP_TYPE_DEF(struct IRVar *, VarRef);
MAP_TYPE_FUNCS(struct IRVar *, VarRef);
static mapVarRef varRefs = NULL;
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
	__auto_type allAssignPaths = graphAllPathsTo(varAssignG, NULL);

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
	for (long i = 0; i != strGraphPathSize(allAssignPaths); i++) {
		__auto_type p = allAssignPaths[i];
		un = strGraphEdgePSetUnion(un, allAssignPaths[i],
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
void IRToSSA(graphNodeIR enter) {
	__auto_type nodes = graphNodeIRAllNodes(enter);
	if (strGraphNodeIRPSize(nodes) == 0)
		return;
	//
	// Find all the vars
	//
	__auto_type filteredVars = IRFilter(enter, filterVars, NULL);
	__auto_type filteredVarsNodes = graphNodeMappingAllNodes(filteredVars);
	strIRVar allVars = NULL;
	for (long i = 0; i != strGraphNodeMappingPSize(filteredVarsNodes); i++) {
		__auto_type irNode = *graphNodeMappingValuePtr(filteredVarsNodes[i]);
		struct IRNodeValue *val = (void *)graphNodeIRValuePtr(irNode);
		// Could be enter node
		if (val->base.type != IR_VALUE)
			continue;

		// Insert if doesnt exist
		if (NULL == strIRVarSortedFind(allVars, val->val.value.var, IRVarCmp)) {
			allVars = strIRVarSortedInsert(allVars, val->val.value.var, IRVarCmp);
		}
	}
	/*{
	char *fn=tmpnam(NULL);
	IRGraphMap2GraphViz(filteredVars, "filter", fn, NULL,NULL,NULL,NULL);
	char buffer[1024];
	sprintf(buffer, "dot -Tsvg %s >/tmp/dot.svg && firefox /tmp/dot.svg &", fn);
	system(buffer);
	}*/

	//
	// Find SSA choose nodes for all vars
	//
	strGraphNodeIRP newNodes = NULL;
	for (long i = 0; i != strIRVarSize(allVars); i++) {
		nodes = graphNodeIRAllNodes(enter);

		// Find enter node in mapa
		__auto_type filtered2 = filterVarRefs(enter, nodes, &allVars[i]);
		__auto_type allNodes = graphNodeMappingAllNodes(filtered2);
		// Compute choose nodes
		strGraphNodeIRP chooses = IRSSACompute(filtered2, &allVars[i]);
		// Number the versions
		SSAVersionVar(enter, &allVars[i]);

		newNodes = strGraphNodeIRPConcat(newNodes, chooses);
	}

end:
	graphNodeMappingKillGraph(&filteredVars, NULL, NULL);
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
static strGraphPath chooseNodeCandidatePathsToChoose(graphNodeIR canidate,
                                                     graphNodeIR choose) {
	// Find paths that are either dead ends that stop at other candiate starts,or
	// path to choose,will filter out dead ends later.
	struct __pathToChoosePredPair pair;
	pair.choose = choose;
	pair.start = canidate;
	__auto_type pathsToChoose =
	    graphAllPathsToPredicate(canidate, &pair, __pathToChoosePred);

	// Filter out dead ends
	pathsToChoose =
	    strGraphPathRemoveIf(pathsToChoose, choose, __lastIsNotChoose);

	return pathsToChoose;
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
void IRSSAReplaceChooseWithAssigns(graphNodeIR node,
                                   strGraphNodeIRP *replaced) {
	assert(graphNodeIRValuePtr(node)->type == IR_CHOOSE);
	struct IRNodeChoose *choose = (void *)graphNodeIRValuePtr(node);

	__auto_type outgoing = graphNodeIROutgoingNodes(node);
	// Outgoing that must be assign (TODO validate this)

	// Make (unique) collection of canidates that point to unique varaibles.
	__auto_type clone = strGraphNodeIRPClone(choose->canidates);
	qsort(clone, strGraphNodeIRPSize(clone), sizeof(*clone),
	      (int (*)(const void *, const void *))gnIRCmpVar);
	clone = strGraphNodeIRPUnique(clone, gnIRCmpVar, NULL);

	for (long i = 0; i != strGraphNodeIRPSize(clone); i++) {
		__auto_type paths = chooseNodeCandidatePathsToChoose(clone[i], node);
		assert(strGraphPathSize(paths) != 0);

		// Insert an assign before final edge leading up to choose
		strGraphEdgeIRP insertedAt = NULL;
		for (long i2 = 0; i2 != strGraphPathSize(paths); i2++) {
			graphEdgeIR last;
			paths[i2] = strGraphEdgeIRPPop(paths[i2], &last);

			// Ensure edge isn't already accounted for
			if (NULL !=
			    strGraphEdgeIRPSortedFind(insertedAt, last, (geCmpType)ptrPtrCmp))
				continue;

			insertedAt =
			    strGraphEdgeIRPSortedInsert(insertedAt, last, (geCmpType)ptrPtrCmp);

			// Start with current node ,assign to result,then use that as the parent
			// node and so on
			graphNodeIR parent = IRCloneNode(clone[i], IR_CLONE_NODE, NULL);
			__auto_type startAt = parent;

			for (long i3 = 0; i3 != strGraphNodeIRPSize(outgoing); i3++) {
				__auto_type clone = IRCloneNode(outgoing[i3], IR_CLONE_NODE, NULL);
				graphNodeIRConnect(parent, clone, IR_CONN_DEST);

				parent = clone;
			}

			// Replace edge with startAt-> parent
			graphNodeIRConnect(graphEdgeIRIncoming(last), startAt,
			                   *graphEdgeIRValuePtr(last));
			graphNodeIRConnect(parent, graphEdgeIROutgoing(last), IR_CONN_FLOW);
			graphEdgeIRKill(graphEdgeIRIncoming(last), graphEdgeIROutgoing(last),
			                graphEdgeIRValuePtr(last), __edgeIsEqual, NULL);
		}
	}

	__auto_type endOfExpression = IREndOfExpr(node);

	strGraphNodeIRP exprNodes =
	       IRStatementNodes(IRStmtStart(node), endOfExpression);
	__auto_type dummy = IRCreateLabel();
	graphReplaceWithNode(exprNodes, dummy, NULL, NULL, sizeof(graphEdgeIR));
	transparentKill(dummy);

	if (replaced)
		*replaced = exprNodes;
	else
		strGraphNodeIRPDestroy(&exprNodes);
}
