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
	struct IRNodeChoose choose;
	choose.base.attrs = NULL;
	choose.base.type = IR_CHOOSE;
	choose.paths =
	    strIRPathPairAppendData(NULL, pathPairs, strIRPathPairSize(pathPairs));
	
	__auto_type retVal = GRAPHN_ALLOCATE(choose);

	IRInsertAfter(insertAfter,)
	return retVal;
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

void IRSSAFindChooseNodes(strGraphNodeMappingP nodes) {
	if (varRefs)
		mapVarRefDestroy(varRefs, NULL);
	varRefs = mapVarRefCreate();

	__auto_type clone = cloneGraphFromNodes(nodes);

	__auto_type doms = graphComputeDominatorsPerNode(clone);
	__auto_type domTree=createDomTree(doms); 
	//
	// Look here
	// ```
	//  A:1
	//  |
	//  \/
	//  A:2
	//  |
	//  \/
	//  A:3
	// ```
	// We only want to place choose node for A:3/1,A:3 is the final value and A:1 is the first nodeso it is the only one we will need to choose.
	// ONLY ELIMATE ITEMS WHOOSE PARENT HAS 1 OUTGOING MEMBER AND 1 INCOMIGN MEMBER,this way the parent must dominate the child always and cant take any other paths
	__auto_type all=getAllNodes(domTree);
	for(long i=0;i!=strGraphNodePSize(all);i++) {
			__auto_type currentVar=graphNodeSSANodeValuePtr(all[i]);

			//Check for one outgoing
			strGraphNodeP out=__graphNodeIncomingNodes(all[i]);
			if(strGraphNodePSize(out)!=1) {
					strGraphNodePDestroy(&out);
					continue;
			}

			//Check if variable
			__auto_type irNode=graphNodeIRValuePtr(*graphNodeMappingValuePtr(all[i]));
			if(irNode->type!=IR_VALUE) {
					goto end;
			}
			
			//Check for one incoming
			strGraphNodeP in=__graphNodeIncomingNodes(all[i]);
			if(strGraphNodePSize(in)!=1)
					goto end;
	eq:
			// Remove current node but pass incoming connection outwards.("Transparent" remove)
			graphNodeSSANodeConnect(in[0], out[0], NULL);
	end:
			strGraphNodePDestroy(&in);
			strGraphNodePDestroy(&out);
	}

	//Now the we have a trimmed tree.Recompute the dominators
	llDominatorsDestroy(&doms, NULL);
	doms = graphComputeDominatorsPerNode(clone);

	__auto_type fronts=graphDominanceFrontiers(domTree, doms);

	//
	// Maps all the nodes that share a domiance frontier
	//
	mapChooseIncomings  frontsMap=mapChooseIncomingsCreate();
	
	for(__auto_type node=llDomFrontierFirst(fronts);node!=NULL;node=llDomFrontierNext(node)) {
			__auto_type nodeValue= llDomFrontierValuePtr(node);

			strIRPathPair pairs=NULL;
			for(long i=0;i!=strGraphNodePSize(nodeValue->nodes);i++) {
					__auto_type irNode=graphNodeIRValuePtr(*graphNodeMappingValuePtr(nodeValue->nodes[i]));

					//Ensure is a value  
					if(irNode->type!=IR_VALUE)
							continue;
					// Ensure is a variable
					if(((struct IRNodeValue *)irNode)->val.type!=IR_VAL_VAR_REF)
							continue;
					
					struct IRPathPair pair;
					pair.path=*graphNodeMappingValuePtr(nodeValue->nodes[i]);
					pair.ref=&((struct IRNodeValue *)irNode)->val.value.var;

					pairs=strIRPathPairAppendItem(pairs, pair);
			}
			createChoose(graphNodeIR insertBefore, strIRPathPair pathPairs, struct IRVarRef *to)

					
			free(hash);
	}

	mapChooseIncomingsDestroy(frontsMap, (void(*)(void*))strGraphNodeIRPDestroy);
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
