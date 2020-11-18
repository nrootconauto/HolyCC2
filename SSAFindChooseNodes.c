#include <IR.h>
#include <graphDominance.h>
#include <hashTable.h>
#include <base64.h>
#include <topoSort.h>
#include <assert.h>
#define GRAPHN_ALLOCATE(x) ({__graphNodeCreate(&x, sizeof(x), 0); })
static char *ptr2Str(const void *a) {
		return base64Enc((void*)&a, sizeof(a));
}
MAP_TYPE_DEF(struct IRVarRef, VarRef);
MAP_TYPE_FUNCS(struct IRVarRef, VarRef);
static __thread mapVarRef varRefs=NULL;
static void insertBefore(graphNodeIR insertBefore,graphNodeIR entry,graphNodeIR exit,enum IRConnType connType) {
		__auto_type incoming=graphNodeIRIncoming(insertBefore);
		
		for(long i=0;i!=strGraphEdgeIRPSize(incoming);i++) {
				//Connect incoming to entry
				graphNodeIRConnect(entry, graphEdgeIRIncoming(incoming[i]), *graphEdgeIRValuePtr(incoming[i]));
				
				//Disconnect for insertBefore
				graphEdgeIRKill(graphEdgeIRIncoming(incoming[i]), insertBefore,NULL,NULL,NULL);
		}
		
		//Connect exit to insertBefore
		graphNodeIRConnect(exit, insertBefore, connType);
		strGraphEdgeIRPDestroy(&incoming);
}
static graphNodeIR createChoose(graphNodeIR insertBefore,strIRPathPair pathPairs,struct IRVarRef *0to) {
				struct IRNodeChoose choose;
				choose.base.attrs=NULL;
				choose.base.type=IR_CHOOSE;
				choose.paths=strIRPathPairAppendData(NULL, pathPairs, strIRPathPairSize(pathPairs));

				__auto_type retVal=GRAPHN_ALLOCATE(choose);
				
}
static int ptrPtrCmp(const void *a,const void *b) {
		if(*(void**)a>*(void**)b)
				return 1;
		else if(*(void**)a<*(void**)b)
				return -1;
		return 0;
}
GRAPH_TYPE_DEF(struct __graphNode *, void*, Dominator);
GRAPH_TYPE_FUNCS(struct __graphNode *, void*, Dominator);

static void connnectIdoms(mapGraphNode nodes,llDominators valids,llDominators BNode) {
		strGraphNodeP B=llDominatorsValuePtr(BNode)->dominators;

		struct __graphNode *bFirst=*strGraphNodePSortedFind(B, llDominatorsValuePtr(BNode)->node, ptrPtrCmp);
		__auto_type incomingNodes=__graphNodeIncomingNodes(bFirst);
		
		//Check if A contains B
		for(long i=0;i!=strGraphNodePSize(incomingNodes);i++) {
				__auto_type find=llDominatorsFind(valids, incomingNodes[i], llDominatorCmp);
				if(find) {
						__auto_type str=ptr2Str(llDominatorsValuePtr(find)->node);
						__auto_type node= mapGraphNodeGet(nodes, str);
						assert(node);
						graphNodeMappingConnect(*node, bFirst, NULL);
				}
		}
}
static graphNodeMapping createDomTree(llDominators doms) {
		mapGraphNode map=mapGraphNodeCreate();

		llDominators  sorted[llDominatorsSize(doms)];;
		long i=0;
		for(__auto_type node=llDominatorsFirst(doms);node!=NULL;node=llDominatorsNext(node)) {
				sorted[i++]=node;
		}
}
void IRSSAFindChooseNodes(strGraphNodeP nodes) {
		if(varRefs)
				mapVarRefDestroy(varRefs, NULL);
		varRefs=mapVarRefCreate();

		__auto_type clone=cloneGraphFromNodes(nodes);
		
		__auto_type doms=graphComputeDominatorsPerNode(clone);
		llDomFrontier fronts=graphDominanceFrontiers(clone, doms);
}
