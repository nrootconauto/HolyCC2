#include <IR.h>
#include <assert.h>
#include <base64.h>
#include <graphDominance.h>
#include <hashTable.h>
#include <topoSort.h>
#define GRAPHN_ALLOCATE(x) ({ __graphNodeCreate(&x, sizeof(x), 0); })
typedef int(*strGN_IRCmpType)(const strGraphNodeIRP *,const strGraphNodeIRP *);

static char *ptr2Str(const void *a) { return base64Enc((void *)&a, sizeof(a)); }
MAP_TYPE_DEF(struct IRVarRef, VarRef);
MAP_TYPE_FUNCS(struct IRVarRef, VarRef);
static __thread mapVarRef varRefs = NULL;
static graphNodeIR createChoose(graphNodeIR insertAfter,
                                strIRPathPair pathPairs) {
		//Make the choose node
		struct IRNodeChoose choose;
	choose.base.attrs = NULL;
	choose.base.type = IR_CHOOSE;
	choose.paths =
	    strIRPathPairAppendData(NULL, pathPairs, strIRPathPairSize(pathPairs));
	
	__auto_type chooseNode = GRAPHN_ALLOCATE(choose);

	//Create a variable ref
	struct IRNodeValue *firstNode=(void*)graphNodeIRValuePtr(pathPairs[0].ref);
	struct IRNodeValue value;
	value.base.attrs=NULL;
	value.base.type=IR_VALUE;
	//Copy over value
	assert(firstNode->base.type==IR_VALUE);
	value.val=firstNode->val;
	value.val.value.var.SSANum=-1;
	//Create node
	__auto_type valueNode=GRAPHN_ALLOCATE(value);

	graphNodeIRConnect(chooseNode, valueNode, IR_CONN_DEST);
	
	IRInsertAfter(insertAfter, chooseNode, valueNode, IR_CONN_FLOW);
	return valueNode;
}
static int ptrPtrCmp(const void *a, const void *b) {
	if (*(void **)a > *(void **)b)
		return 1;
	else if (*(void **)a < *(void **)b)
		return -1;
	return 0;
}

static void getAllVisit(struct __graphNode *node,void *all) {
		strGraphNodeP *ptr=all;
		*ptr=strGraphNodePAppendItem(*ptr, node);
}
static int alwaysTrue(const struct __graphNode *n,const struct __graphEdge *e,const void *data) {
		return 1;
}
static strGraphNodeP getAllNodes(struct __graphNode *start) {
		strGraphNodeP all=NULL;
		__graphNodeVisitForward(start, &all, alwaysTrue, getAllVisit);
		return all;
}

struct SSANode {
		graphNodeIR assignNode;
		struct IRValue *var;
};
GRAPH_TYPE_DEF(struct SSANode,void*, SSANode);
GRAPH_TYPE_FUNCS(struct SSANode,void*, SSANode);

MAP_TYPE_DEF(strGraphNodeIRP, ChooseIncomings);
MAP_TYPE_FUNCS(strGraphNodeIRP, ChooseIncomings);
static int isJumpOrVarAndNotChoosed(void *data,struct __graphNode *node) {
		struct IRVar *expectedVar=data;

		struct IRNode *ir=graphNodeIRValuePtr((graphNodeIR)node);
		if(ir->type==IR_VALUE) {
				struct IRNodeValue *val=(void*)ir;
				if(val->val.type==IR_VAL_VAR_REF) {
						if(expectedVar->type==val->val.value.var.var.type) {
								if(expectedVar->type==IR_VAR_MEMBER) {
										//TODO check equal
								} else if(expectedVar->type==IR_VAR_VAR) {
										if(expectedVar->value.var==val->val.value.var.var.value.var)
												goto checkForChoosed;
								}
						}
				}
		} else if(ir->type==IR_LABEL)
				return 1;
		else if(ir->type==IR_STATEMENT_START)
				return 1;
		else if(ir->type==IR_ENTRY)
				return 1;

		return 0;
	checkForChoosed:;
		//
		// Check if assigned to choose node
		//
				strGraphEdgeIRP in __attribute__((cleanup(strGraphEdgeIRPDestroy)))= graphNodeIRIncoming((graphNodeIR)node);
				strGraphEdgeIRP dstConns __attribute__((cleanup(strGraphEdgeIRPDestroy)))=IRGetConnsOfType(in, IR_CONN_DEST);
		if(strGraphEdgeIRPSize(dstConns)==0)
				return 1;
		if(graphNodeIRValuePtr(graphEdgeIRIncoming(in[0]))->type==IR_CHOOSE)
				return 0;

		return 1;
}
static graphNodeMapping filterJumpsAndVars(strGraphNodeIRP nodes,struct IRVar *var) {
		//First find the references to the var and jumps.
		return createFilteredGraph(nodes , var,isJumpOrVarAndNotChoosed);
}
/**
	* Returns list of new varaible references
	*/
static strGraphNodeIRP IRSSAFindChooseNodes(graphNodeMapping start) {
	if (varRefs)
		mapVarRefDestroy(varRefs, NULL);
	varRefs = mapVarRefCreate();

	__auto_type mappedNodes=graphNodeMappingAllNodes(start);
	__auto_type doms = graphComputeDominatorsPerNode(start);
	__auto_type fronts=graphDominanceFrontiers(start, doms);

	strGraphNodeMappingP retVal=NULL;
	for(__auto_type node=llDomFrontierFirst(fronts);node!=NULL;node=llDomFrontierNext(node)) {
			__auto_type nodeValue= llDomFrontierValuePtr(node);
			__auto_type in=graphNodeMappingIncomingNodes(nodeValue->node);
			
			strIRPathPair pairs=NULL;
			//Find nodes connected to frontier
			for(long i=0;i!=strGraphNodePSize(in);i++) {
					__auto_type irNode=*graphNodeMappingValuePtr(in[i]);

					//Ensure is a value  
					if(graphNodeIRValuePtr(irNode)->type!=IR_VALUE)
							continue;
					// Ensure is a variable
					if(((struct IRNodeValue *)irNode)->val.type!=IR_VAL_VAR_REF)
							continue;
					
					struct IRPathPair pair;
					pair.ref=irNode;
					pairs=strIRPathPairAppendItem(pairs, pair);
			}

			strGraphNodeMappingPDestroy(&in);

			//Create the choose
			assert(strIRPathPairSize(pairs)!=0);
			__auto_type choose=createChoose(*graphNodeMappingValuePtr(nodeValue->node), pairs);

			retVal=strGraphNodePAppendItem(retVal, choose);
	}

	return retVal;
}
static int filterVars(void *data,struct __graphNode *node) {
		__auto_type ir=graphNodeIRValuePtr(node);
		if(ir->type==IR_VALUE) {
				struct IRNodeValue *irValue=(void*)ir;
				if(irValue->val.type==IR_VAL_VAR_REF)
						return 1;
		}

		return 0;
}
STR_TYPE_DEF(struct IRVar, IRVar);
STR_TYPE_FUNCS(struct IRVar, IRVar);
static int ptrCmp(const void *a,const void *b) {
		if(a>b)
				return 1;
		else if(a<b)
				return -1;
		else
				return 0;
}
static int IRVarCmp(const struct IRVar *a,const struct IRVar *b) {
		if(0!=a->type-b->type)
				return a->type-b->type;

		if(a->type==IR_VAR_VAR) {
				return ptrCmp(a->value.var, b->value.var);
		} else {
				//TODO implement
		}

		return 0;
}
void IRToSSA(strGraphNodeIRP nodes) {
		if(strGraphNodeIRPSize(nodes)==0)
				return;
		//
		// Find all the vars
		//
		__auto_type filteredVars=createFilteredGraph(nodes,NULL,filterVars);
		__auto_type filteredVarsNodes=graphNodeMappingAllNodes(filteredVars);
		strIRVar allVars=NULL;
		for(long i=0;i!=strGraphNodeMappingPSize(filteredVarsNodes);i++) {
				struct IRNodeValue *val=(void*)graphNodeIRValuePtr(*graphNodeMappingValuePtr(filteredVarsNodes[i]));
				assert(val->base.type==IR_VALUE);

				//Insert if doesnt exist
				if(NULL==strIRVarSortedFind(allVars, val->val.value.var.var, IRVarCmp)) {
						allVars=strIRVarSortedInsert(allVars, val->val.value.var.var, IRVarCmp);
				}
		}

		//
		// Find SSA choose nodes for all vars
		//
		strGraphNodeIRP newNodes=NULL;
		for(long i=0;i!=strIRVarSize(allVars);i++) {
				__auto_type filtered2= filterJumpsAndVars(nodes, &allVars[i]);				
				strGraphNodeIRP chooses =IRSSAFindChooseNodes(filtered2);

				newNodes=strGraphNodeIRPConcat(newNodes, chooses);
		}
		
		strIRVarDestroy(&allVars);
		graphNodeMappingKillGraph(&filteredVars, NULL, NULL);
		strGraphNodeMappingPDestroy(&filteredVarsNodes);
}
/*
	strGraphEdgeP out __attribute__((cleanup(strGraphEdgePDestroy)))=__graphNodeOutgoing(all[i]);
			struct IRNodeAssign *assign=(void*)irNode;
			strGraphEdgeP conns __attribute__((cleanup(strGraphEdgePDestroy))) =IRGetConnsOfType(out,IR_CONN_DEST);
			//Ensure "clean" IR
			assert(strGraphEdgePSize(conns)<=1);
__auto_type parentVar=graphNodeSSANodeValuePtr(in[0])->var;
					//Check if variables are equal.
					if(parentVar->type==currentVar->var->type) {
							if(parentVar->type==IR_VAR_MEMBER) {
									if(parentVar-> value.var.var.value.member==currentVar->var->value.var.var.value.member)
											goto eq;
							} else if(parentVar->type==IR_VAR_VAR) {
									if(parentVar-> value.var.var.value.var==currentVar->var->value.var.var.value.var)
											goto eq;
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
