#include <IR.h>
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

static char *ptr2Str(const void *a) { return base64Enc((void *)&a, sizeof(a)); }
MAP_TYPE_DEF(struct IRVarRef, VarRef);
MAP_TYPE_FUNCS(struct IRVarRef, VarRef);
static __thread mapVarRef varRefs = NULL;
static graphNodeIR createChoose(graphNodeIR insertBefore,
                                strIRPathPair pathPairs) {
	// Make the choose node
	struct IRNodeChoose choose;
	choose.base.attrs = NULL;
	choose.base.type = IR_CHOOSE;
	choose.paths =
	    strIRPathPairAppendData(NULL, pathPairs, strIRPathPairSize(pathPairs));

	__auto_type chooseNode = GRAPHN_ALLOCATE(choose);

	// Create a variable ref
	struct IRNodeValue *firstNode = (void *)graphNodeIRValuePtr(pathPairs[0].ref);
	struct IRNodeValue value;
	value.base.attrs = NULL;
	value.base.type = IR_VALUE;
	// Copy over value
	assert(firstNode->base.type == IR_VALUE);
	value.val = firstNode->val;
	value.val.value.var.SSANum = -1;
	// Create node
	__auto_type valueNode = GRAPHN_ALLOCATE(value);

	graphNodeIRConnect(chooseNode, valueNode, IR_CONN_DEST);

	__auto_type stmtStart = IRGetStmtStart(insertBefore);
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

MAP_TYPE_DEF(strGraphNodeIRP, ChooseIncomings);
MAP_TYPE_FUNCS(strGraphNodeIRP, ChooseIncomings);

struct varAndEnterPair {
	graphNodeIR enter;
	struct IRVar *var;
};
static int jumpOrAssignedVar(void *data, struct __graphNode *node) {
	struct IRVar *expectedVar = ((struct varAndEnterPair *)data)->var;

	struct IRNode *ir = graphNodeIRValuePtr((graphNodeIR)node);
	if (ir->type == IR_VALUE) {
		struct IRNodeValue *val = (void *)ir;
		if (val->val.type == IR_VAL_VAR_REF) {
			if (expectedVar->type == val->val.value.var.var.type) {
				if (expectedVar->type == IR_VAR_MEMBER) {
					// TODO check equal
				} else if (expectedVar->type == IR_VAR_VAR) {
					if (expectedVar->value.var == val->val.value.var.var.value.var)
						goto checkForAssign;
				}
			}
		}
	} else if (ir->type == IR_LABEL)
		return 1;
	else if (ir->type == IR_STATEMENT_START)
		return 1;
	else if (ir->type == IR_ENTRY)
		return 1;

	// Check if enter-node
	if (((struct varAndEnterPair *)data)->enter == node)
		return 1;

	return 0;
checkForAssign:;
	//
	// Check if assigned to choose node
	//
	strGraphEdgeIRP in __attribute__((cleanup(strGraphEdgeIRPDestroy))) =
	    graphNodeIRIncoming((graphNodeIR)node);
	strGraphEdgeIRP dstConns __attribute__((cleanup(strGraphEdgeIRPDestroy))) =
	    IRGetConnsOfType(in, IR_CONN_DEST);
	return 0 != strGraphEdgeIRPSize(dstConns);
}
static graphNodeMapping filterJumpsAndVars(strGraphNodeIRP nodes,
                                           struct IRVar *var) {
	// First find the references to the var and jumps.
	struct varAndEnterPair pair;
	pair.enter = NULL;
	pair.var = var;
	return createFilteredGraph(NULL, nodes, &pair, jumpOrAssignedVar);
}
/**
 * Returns list of new varaible references
 */
static strGraphNodeIRP IRSSAFindChooseNodes(graphNodeMapping start) {
	if (varRefs)
		mapVarRefDestroy(varRefs, NULL);
	varRefs = mapVarRefCreate();

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

		__auto_type in = graphNodeMappingIncomingNodes(nodeValue->node);

		strIRPathPair pairs = NULL;
		// Find nodes connected to frontier
		for (long i = 0; i != strGraphNodePSize(in); i++) {
			__auto_type irNode = *graphNodeMappingValuePtr(in[i]);

			// Ensure is a value
			if (graphNodeIRValuePtr(irNode)->type != IR_VALUE)
				continue;
			// Ensure is a variable
			if (((struct IRNodeValue *)graphNodeIRValuePtr(irNode))->val.type !=
			    IR_VAL_VAR_REF)
				continue;

			struct IRPathPair pair;
			pair.ref = irNode;
			pairs = strIRPathPairAppendItem(pairs, pair);
		}

		strGraphNodeMappingPDestroy(&in);

		// Create the choose
		if (strIRPathPairSize(pairs) != 0) {
			__auto_type choose =
			    createChoose(*graphNodeMappingValuePtr(nodeValue->node), pairs);
			retVal = strGraphNodePAppendItem(retVal, choose);
		}
	}

	return retVal;
}
static int filterVars(void *data, struct __graphNode *node) {
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
static int IRVarCmp(const struct IRVar *a, const struct IRVar *b) {
	if (0 != a->type - b->type)
		return a->type - b->type;

	if (a->type == IR_VAR_VAR) {
		return ptrCmp(a->value.var, b->value.var);
	} else {
		// TODO implement
	}

	return 0;
}
void IRToSSA(strGraphNodeIRP nodes, graphNodeIR enter) {
	if (strGraphNodeIRPSize(nodes) == 0)
		return;
	//
	// Find all the vars
	//
	__auto_type filteredVars =
	    createFilteredGraph(nodes[0], nodes, NULL, filterVars);
	__auto_type filteredVarsNodes = graphNodeMappingAllNodes(filteredVars);
	strIRVar allVars = NULL;
	for (long i = 0; i != strGraphNodeMappingPSize(filteredVarsNodes); i++) {
		__auto_type irNode = *graphNodeMappingValuePtr(filteredVarsNodes[i]);
		struct IRNodeValue *val = (void *)graphNodeIRValuePtr(irNode);
		assert(val->base.type == IR_VALUE);

		// Insert if doesnt exist
		if (NULL == strIRVarSortedFind(allVars, val->val.value.var.var, IRVarCmp)) {
			allVars = strIRVarSortedInsert(allVars, val->val.value.var.var, IRVarCmp);
		}
	}
	//
	// Find SSA choose nodes for all vars
	//
	strGraphNodeIRP newNodes = NULL;
	for (long i = 0; i != strIRVarSize(allVars); i++) {
		// Find enter node in map
		__auto_type filtered2 = filterJumpsAndVars(nodes, &allVars[i]);
		__auto_type allNodes = graphNodeMappingAllNodes(filtered2);
		for (long i = 0; i != strGraphNodeMappingPSize(allNodes); i++) {
			__auto_type ins = graphNodeMappingIncomingNodes(allNodes[i]);
			__auto_type outs = graphNodeMappingOutgoingNodes(allNodes[i]);
			long inDeg = strGraphNodeMappingPSize(ins);
			long outDeg = strGraphNodeMappingPSize(outs);

			__auto_type node = *graphNodeMappingValuePtr(allNodes[i]);
			;
			printf("ptoato\n");
		}
		strGraphNodeIRP chooses = IRSSAFindChooseNodes(filtered2);

		newNodes = strGraphNodeIRPConcat(newNodes, chooses);
	}

end:
	strIRVarDestroy(&allVars);
	graphNodeMappingKillGraph(&filteredVars, NULL, NULL);
	strGraphNodeMappingPDestroy(&filteredVarsNodes);
}
/*
  strGraphEdgeP out
__attribute__((cleanup(strGraphEdgePDestroy)))=__graphNodeOutgoing(all[i]);
      struct IRNodeAssign *assign=(void*)irNode;
      strGraphEdgeP conns __attribute__((cleanup(strGraphEdgePDestroy)))
=IRGetConnsOfType(out,IR_CONN_DEST);
      //Ensure "clean" IR
      assert(strGraphEdgePSize(conns)<=1);
__auto_type parentVar=graphNodeSSANodeValuePtr(in[0])->var;
          //Check if variables are equal.
          if(parentVar->type==currentVar->var->type) {
              if(parentVar->type==IR_VAR_MEMBER) {
                  if(parentVar->
value.var.var.value.member==currentVar->var->value.var.var.value.member) goto
eq; } else if(parentVar->type==IR_VAR_VAR) { if(parentVar->
value.var.var.value.var==currentVar->var->value.var.var.value.var) goto eq;
              }
          }

//
  // Is a mapping of a mapping
  //
  graphNodeMapping domTree=createDomTree(doms);

  //
  // Maps all the nodes that share a domiance frontier
  //
  mapGraphNode domTreeMap=mapChooseIncomingsCreate();
  __auto_type allDomTree=graphNodeMappingAllNodes(domTree);
  //hash them all
  for(long i=0;i!=strGraphNodeMappingPSize(allDomTree);i++) {
      //Is a mapping of a mapping
      char *hash=ptr2Str(*graphNodeMappingValuePtr(allDomTree[i]));
      mapChooseIncomingsInsert(domTreeMap, hash, allDomTree[i]);
      free(hash);
  }
  */
