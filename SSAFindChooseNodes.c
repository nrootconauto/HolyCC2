#include <IR.h>
#include <graphDominance.h>
#include <hashTable.h>
#include <base64.h>
#include <topoSort.h>
static char *ptr2Str(const void *a) {
		return base64Enc((void*)&a, sizeof(a));
}
MAP_TYPE_DEF(struct IRVarRef, VarRef);
MAP_TYPE_FUNCS(struct IRVarRef, VarRef);
static __thread mapVarRef varRefs=NULL;
void IRSSAFindChooseNodes(strGraphNodeP nodes) {
		if(varRefs)
				mapVarRefDestroy(varRefs, NULL);
		varRefs=mapVarRefCreate();

		strGraphNodeIRP sorted=topoSort(nodes);
		graphComputeDominatorsPerNode();
		for(long i=0;i!=strGraphNodeIRPSize(sorted);i++) {
				
		}
}
