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
typedef int (*gnCmpType)(const graphNodeIR *, const graphNodeIR *);

static char *ptr2Str(const void *a) { return base64Enc((void *)&a, sizeof(a)); }
MAP_TYPE_DEF(struct IRVarRef, VarRef);
MAP_TYPE_FUNCS(struct IRVarRef, VarRef);
static __thread mapVarRef varRefs = NULL;
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
	struct IRNodeValue *firstNode = (void *)graphNodeIRValuePtr(pathPairs[0]);
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
static int occurOfVar(struct varAndEnterPair *expectedVar, struct __graphNode *node) {
		struct IRNode *ir = graphNodeIRValuePtr((graphNodeIR)node);
		if(ir->type==IR_VALUE) {
				struct IRNodeValue *val=(void*)ir;
				if(val->val.type!=IR_VAL_VAR_REF)
						return 0;
				
				return 0==IRVarCmp(expectedVar->var,&val->val.value.var.var);
		}

		if(node==expectedVar->enter)
				return 1;
		
		return 0;
}
static int isAssignedVar(struct varAndEnterPair *data, struct __graphNode *node) {
	struct IRVar *expectedVar = data->var;

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
	}

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
static graphNodeMapping filterVarAssigns(graphNodeIR enter,strGraphNodeIRP nodes,
                                           struct IRVar *var) {
	// First find the references to the var and jumps.
	struct varAndEnterPair pair;
	pair.enter = enter;
	pair.var = var;
	return createFilteredGraph(enter, nodes, &pair, (int(*)(void*,struct __graphNode*))isAssignedVar);
}
static void __strGraphNodeIRDestroy(void *vec) {

		strGraphNodeIRPDestroy((strGraphNodeIRP *)vec);
}
static char *node2Str(struct __graphNode *node) {
		node=*graphNodeMappingValuePtr(node);
		char buffer[128];
		sprintf(buffer, "%p", node);

		__auto_type find=mapStrGet(nodeNames,  buffer);
		if(!find)
				return NULL;
		
		__auto_type str= *find;
		char *retVal=malloc(strlen(str)+1);
		strcpy(retVal, str);

		return retVal;
}
static void SSAVersionVar(graphNodeIR start,struct IRVar *var) {
		graphPrint(start, node2Str);
		//
		struct varAndEnterPair pair;
		pair.enter=start;
		pair.var=var;
		__auto_type allNodes=graphNodeIRAllNodes(start);
		__auto_type allVars=createFilteredGraph(start, allNodes, &pair, (int(*)(void*,struct __graphNode*))occurOfVar);
		
		__auto_type allPaths=graphAllPathsTo(allVars,NULL);
	
		//Init all var nodes to -1
		for(long i1=0;i1!=strGraphPathSize(allPaths);i1++) {
				for(long i2=0;i2!=strGraphEdgePSize(allPaths[i1]);i2++) {
						//Only use incoming node from edge,
						__auto_type node=*graphNodeMappingValuePtr(__graphEdgeIncoming(allPaths[i1][i2]));
 
						//only if a value
						if(graphNodeIRValuePtr(node)->type!=IR_VALUE)
								continue;
						struct IRNodeValue *val=(void*)graphNodeIRValuePtr(node);
						//if a var
						if(val->val.type!=IR_VAL_VAR_REF)
								continue;
						
						val->val.value.var.SSANum=-1;
				}
		}

		//Actually assign SSA values by depth
		for(;;) {
				int changed=0;
				for(long i1=0;i1!=strGraphPathSize(allPaths);i1++) {
						long version=0;
			
						for(long i2=0;i2!=strGraphEdgePSize(allPaths[i1]);i2++) {
								//Only use incoming node from edge,
								__auto_type node= __graphEdgeIncoming(allPaths[i1][i2]);

								//Continue if not val
								if(graphNodeIRValuePtr(*graphNodeMappingValuePtr(node))->type!=IR_VALUE)
										continue;
								
								__auto_type irVal= (struct IRNodeValue*)graphNodeIRValuePtr(*graphNodeMappingValuePtr(node));
								//if a var
								if(irVal->val.type!=IR_VAL_VAR_REF)
								continue;

								//Choose highest incoming
								__auto_type incoming=graphNodeMappingIncomingNodes(node);
								for(long i=0;i!=strGraphNodePSize(incoming);i++) {
										//Ensure is a value node
										__auto_type sourceNode=*graphNodeMappingValuePtr(incoming[i]);
										if(graphNodeIRValuePtr(sourceNode)->type!=IR_VALUE)
												continue;

										//Use highest incoming
										struct IRNodeValue *val= (void*)graphNodeIRValuePtr(sourceNode);
										//Is a var
										if(val->val.type!=IR_VAL_VAR_REF)
												continue;
										
										assert(val->base.type==IR_VALUE);
										if(val->val.value.var.SSANum>version) {
												version=val->val.value.var.SSANum;
										}
								}
								strGraphNodePDestroy(&incoming);

								//If assign operation increment SSA version
								struct varAndEnterPair pair;
								pair.enter=NULL;
								pair.var=var;
								if(isAssignedVar(&pair, *graphNodeMappingValuePtr(node)))
										version++;
								
								//assign if current is bigger
								if(version>irVal->val.value.var.SSANum) {
										changed=1;
										irVal->val.value.var.SSANum=version;
								}
						}
				}

				//Repeat until unchaged.
				if(!changed)
								break;
		}
}
/**
 * Returns list of new varaible references
 */
static strGraphNodeIRP IRSSACompute(graphNodeMapping start,struct IRVar *var) {
		graphPrint(start,node2Str);
		//
	__auto_type frontiersToMaster = mapChooseIncomingsCreate();
	__auto_type nodeKey2Ptr = mapGraphNodeCreate();

	__auto_type mappedNodes = graphNodeMappingAllNodes(start);
	__auto_type doms = graphComputeDominatorsPerNode(start);
	for(__auto_type dom=llDominatorsFirst(doms);dom!=NULL;dom=llDominatorsNext(dom)) {
			__auto_type domV=llDominatorsValuePtr(dom);
			__auto_type v=*graphNodeMappingValuePtr(domV->node);
			for(long i=0;i!=strGraphNodeMappingPSize(domV->dominators);i++) {
					__auto_type d=*graphNodeMappingValuePtr(domV->dominators[i]);
					int x=1+1;
			}
	}
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
			char *hash = ptr2Str(frontier);
		loop:;
			__auto_type find = mapChooseIncomingsGet(frontiersToMaster, hash);
			if (!find) {
				mapChooseIncomingsInsert(frontiersToMaster, hash, NULL);
				mapGraphNodeInsert(nodeKey2Ptr, hash, frontier);
				goto loop;
			}
			free(hash);

			__auto_type master = *graphNodeMappingValuePtr(nodeValue->node);
			if (!strGraphNodeIRPSortedFind(*find, master, (gnCmpType)ptrPtrCmp))
				*find =
				    strGraphNodeIRPSortedInsert(*find, master, (gnCmpType)ptrPtrCmp);
		}
	}

	//
	// Insert the choose nodes
	//
	long kCount;
	mapChooseIncomingsKeys(frontiersToMaster, NULL, &kCount);
	const char *keys[kCount];
	mapChooseIncomingsKeys(frontiersToMaster, keys, NULL);

	for (long i = 0; i != kCount; i++) {
		__auto_type masters = *mapChooseIncomingsGet(frontiersToMaster, keys[i]);
		__auto_type masterNode = *mapGraphNodeGet(nodeKey2Ptr, keys[i]);
		createChoose(masterNode, masters);
	}
	
	mapChooseIncomingsDestroy(frontiersToMaster, __strGraphNodeIRDestroy);
	mapGraphNodeDestroy(nodeKey2Ptr, NULL);
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
		// Find enter node in mapa
			__auto_type filtered2 = filterVarAssigns(enter,nodes, &allVars[i]);
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
		//Compute choose nodes
		strGraphNodeIRP chooses = IRSSACompute(filtered2,&allVars[i]);
		//Number the versions
		SSAVersionVar(enter,&allVars[i]);

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
