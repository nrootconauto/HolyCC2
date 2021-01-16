#include <stdio.h>
#include <IRLiveness.h>
#include <IR.h>
#include <graphColoring.h>
#include <cleanup.h>
#include <hashTable.h>
#include <assert.h>
#include <frameLayout.h>
static long degree(graphNodeIRLive live) {
		strGraphEdgeIRLiveP out CLEANUP(strGraphEdgeIRLivePDestroy)=graphNodeIRLiveOutgoing(live);
		return strGraphEdgeIRLivePSize(out);
}
static int ptrPtrCmp(const void *a, const void *b) {
	if (*(void **)a > *(void **)b)
		return 1;
	else if (*(void **)a < *(void **)b)
		return -1;
	return 0;
}
typedef int(*gnCmpType)(const graphNodeIRLive *,const graphNodeIRLive *);
static int isGlobal(graphNodeIR node,const void *data) {
		struct IRNodeValue *val=(void*)graphNodeIRValuePtr(node);
		if(val->base.type==IR_VALUE)
				if(val->val.type==IR_VAL_VAR_REF)
						return !val->val.value.var.value.var->isGlobal;
		
		return 0;
}
static void strGraphNodeIRLivePDestroy2(strGraphNodeIRLiveP *lives) {
		for(long i=0;i!=strGraphNodeIRPSize(*lives);i++)
				graphNodeIRLiveKillGraph(&lives[0][i], NULL, NULL);
		strGraphNodeIRLivePDestroy(lives);
}
STR_TYPE_DEF(int, Int);
STR_TYPE_FUNCS(int, Int);
static int intCmp(const int *a,const int *b) {
		return *a-*b;
}
static void reColorNodeIfAdj(graphNodeIRLive node,llVertexColor colors,int *colorCount) {
		strGraphEdgeIRLiveP out CLEANUP(strGraphEdgeIRLivePDestroy)=graphNodeIRLiveOutgoing(node);
		int currentColor=llVertexColorGet(colors, node)->color;

		strInt adjColors CLEANUP(strIntDestroy)=NULL;
		for(long i=0;i!=strGraphEdgeIRLivePSize(out);i++) {
				__auto_type outNode=graphEdgeIRLiveOutgoing(out[i]);
				__auto_type adjColor=llVertexColorGet(colors, outNode)->color;
				if(!strIntSortedFind(adjColors, adjColor, intCmp))
						adjColors=strIntSortedInsert(adjColors, adjColor, intCmp);
		}
		if(strIntSortedFind(adjColors, currentColor, intCmp)) {
				//Conflict
				for(long i=0;i!=*colorCount;i++) {
						if(strIntSortedFind(adjColors, i, intCmp)) { 
								continue;
						} else {
								llVertexColorGet(colors, node)->color=i;
								return;
						}
				}
				//Need to add color
				llVertexColorGet(colors, node)->color=*colorCount;
				++*colorCount;
		}
}
MAP_TYPE_DEF(strGraphNodeIRLiveP, LiveNodesByColor);
MAP_TYPE_FUNCS(strGraphNodeIRLiveP, LiveNodesByColor);
static void __strGraphNodeIRLivePDestroy(void *str) {
		strGraphNodeIRLivePDestroy(str);
}
static void mapLiveNodesByColorDestroy2(mapLiveNodesByColor *map) {
		mapLiveNodesByColorDestroy(*map, (void(*)(void*))__strGraphNodeIRLivePDestroy);
}
struct colorPair {
		int a,b;
};
strFrameEntry IRComputeFrameLayout(graphNodeIR start,long *frameSize) {
		strFrameEntry retVal=NULL;
		//These return "first" nodes of graphs
		strGraphNodeIRLiveP graphs CLEANUP(strGraphNodeIRLivePDestroy2)=IRInterferenceGraphFilter(start, NULL, isGlobal);
		for(long i=0;i!=strGraphNodeIRLivePSize(graphs);i++) {
				__auto_type colors=graphColor(graphs[i]);
				strGraphNodeIRLiveP allNodes CLEANUP(strGraphNodeIRLivePDestroy)=graphNodeIRLiveAllNodes(graphs[i]);
				int colorCount=vertexColorCount(colors)+1;
				for(long n=0;n!=strGraphNodeIRLivePSize(allNodes);n++)
						reColorNodeIfAdj(allNodes[n],colors,&colorCount);
				mapLiveNodesByColor byColor CLEANUP(mapLiveNodesByColorDestroy2)=mapLiveNodesByColorCreate();
				for(long n=0;n!=strGraphNodeIRLivePSize(allNodes);n++) {
						int color=llVertexColorGet(colors, allNodes[n])->color;
						long len=snprintf(NULL, 0, "%i", color);
						char buffer[len+1];
						sprintf(buffer, "%i", color);
				loop:;
						__auto_type find=mapLiveNodesByColorGet(byColor, buffer);
						if(!find) {
								mapLiveNodesByColorInsert(byColor, buffer, NULL);
								goto loop;
						}
						*find=strGraphNodeIRLivePSortedInsert(*find,allNodes[n], (gnCmpType)ptrPtrCmp);
				}
				long kCount;
				mapLiveNodesByColorKeys(byColor, NULL, &kCount);
				const char *keys[kCount];
				mapLiveNodesByColorKeys(byColor, keys, &kCount);
				long currentOffset=0;
				for(long k=0;k!=kCount;k++) {
						long largestItemSize=-1;
						__auto_type items=*mapLiveNodesByColorGet(byColor, keys[k]);	
						//Get biggest item of color
						for(long i=0;i!=strGraphNodeIRLivePSize(items);i++) {
								int success;
								long size=8*objectSize(graphNodeIRLiveValuePtr(items[i])->ref.value.var->type,&success);
								assert(success);
								if(largestItemSize<size)
										largestItemSize=size;
								struct frameEntry entry;
								entry.offset=currentOffset;
								entry.var=graphNodeIRLiveValuePtr(items[i])->ref;
								retVal=strFrameEntryAppendItem(retVal, entry);
						}
						currentOffset+=largestItemSize;
				}
				if(frameSize)
						*frameSize=currentOffset;
		}
		return retVal;
}
