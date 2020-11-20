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

	//Create the assign node
	__auto_type assignNode=createAssign(chooseNode, valueNode);
	
	IRInsertAfter(insertAfter, chooseNode, valueNode, IR_CONN_FLOW);
	return assignNode;
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

static strGraphNodeP IRSSAFindChooseNodes(strGraphNodeMappingP nodes) {
	if (varRefs)
		mapVarRefDestroy(varRefs, NULL);
	varRefs = mapVarRefCreate();

	__auto_type clone = cloneGraphFromNodes(nodes);

	__auto_type doms = graphComputeDominatorsPerNode(clone);
	__auto_type mapped=cloneGraphFromNodes(nodes);
	__auto_type fronts=graphDominanceFrontiers(mapped, doms);

	//
	// Maps all the nodes that share a domiance frontier
	//
	mapChooseIncomings  frontsMap=mapChooseIncomingsCreate();

	strGraphNodeMappingP retVal=NULL;
	for(__auto_type node=llDomFrontierFirst(fronts);node!=NULL;node=llDomFrontierNext(node)) {
			__auto_type nodeValue= llDomFrontierValuePtr(node);

			strIRPathPair pairs=NULL;
			for(long i=0;i!=strGraphNodePSize(nodeValue->nodes);i++) {
					__auto_type irNode=*graphNodeMappingValuePtr(nodeValue->nodes[i]);

					//Ensure is a value  
					if(graphNodeIRValuePtr(irNode)->type!=IR_VALUE)
							continue;
					// Ensure is a variable
					if(((struct IRNodeValue *)irNode)->val.type!=IR_VAL_VAR_REF)
							continue;

					//Ensure that said path is immediatly connectected to frontier(only want the most recent values)
					if(!graphNodeMappingConnectedTo(nodeValue->nodes[i],nodeValue->node))
							continue;
					
					struct IRPathPair pair;
					pair.ref=irNode;
					pairs=strIRPathPairAppendItem(pairs, pair);
			}

			//Create the choose
			assert(strIRPathPairSize(pairs)!=0);
			__auto_type choose=createChoose(*graphNodeMappingValuePtr(nodeValue->node), pairs);

			retVal=strGraphNodePAppendItem(retVal, choose);
	}

	
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
			/
	*/
