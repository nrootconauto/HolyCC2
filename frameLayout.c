#include "IR.h"
#include "IRLiveness.h"
#include <assert.h>
#include "cleanup.h"
#include "frameLayout.h"
#include "graphColoring.h"
#include "hashTable.h"
#include <limits.h>
#include <stdio.h>
#include "ptrMap.h"
#define ALLOCATE(x)                                                                                                                                                \
	({                                                                                                                                                               \
		__auto_type len = sizeof(x);                                                                                                                                   \
		void *$retVal = calloc(len,1);																																									\
		memcpy($retVal, &x, len);                                                                                                                                      \
		$retVal;                                                                                                                                                       \
	})
static int ptrPtrCmp(const void *a, const void *b) {
		if(*(void**)a>*(void**)b)
				return 1;
		else if(*(void**)a<*(void**)b)
				return -1;
		return 0;
}
typedef int (*gnCmpType)(const graphNodeIRLive *, const graphNodeIRLive *);
static int isGlobal(graphNodeIR node, const void *data) {
	struct IRNodeValue *val = (void *)graphNodeIRValuePtr(node);
	if (val->base.type == IR_VALUE)
		if (val->val.type == IR_VAL_VAR_REF)
			return val->val.value.var.var->isGlobal;

	return 0;
}

struct IRVarRefsPair {
		long largestSize;
		long largestAlign;
		strGraphNodeIRP refs;
		strVar vars;
		long offset;
};
static void IRVarRefsPairDestroy(struct IRVarRefsPair **item) {
		struct IRVarRefsPair *pair=*item;
		strGraphNodeIRPDestroy(&pair->refs);
		strVarDestroy(&pair->vars);
		free(*item);
}
STR_TYPE_DEF(struct IRVarRefsPair*, IRVarRefs);
STR_TYPE_FUNCS(struct IRVarRefsPair*, IRVarRefs);
static int IRVarRefsCmp(const struct IRVarRefsPair **a, const struct IRVarRefsPair **b) {
		return ptrPtrCmp(a, b);
}
static struct IRVarRefsPair *IRVarRefsFindVar(strIRVarRefs refs,struct IRVar *var) {
		for(long v=0;v!=strIRVarRefsSize(refs);v++) {
				__auto_type find=strVarSortedFind(refs[v]->vars, *var, IRVarCmp);
				if(find)
						return refs[v];
		}
		return NULL;
}
static void strIRVarRefsDestroy2(strIRVarRefs *refs) {
	for (long r = 0; r != strIRVarRefsSize(*refs); r++)
		strGraphNodeIRPDestroy(&refs[0][r]->refs);
	strIRVarRefsDestroy(refs);
}
static int IRVarRefsSizeCmpRev(const void *a, const void *b) {
		const struct IRVarRefsPair **A = (void*)a, **B = (void*)b;
	__auto_type bSize = B[0]->largestSize;
	__auto_type aSize = A[0]->largestSize;
	return bSize - aSize;
}
static long pack(long baseOffset, long endBound, strIRVarRefs *refs, strIRVarRefs *order) {
	long len = strIRVarRefsSize(*refs);
	struct IRVarRefsPair *clone[len];
	memcpy(clone, *refs, sizeof(*clone) * len);
	qsort(clone, len, sizeof(*clone), IRVarRefsSizeCmpRev);

	for (long v = 0; v != len; v++) {
		long largestSize = clone[0]->largestSize;
		long largestAlign = clone[0]->largestAlign;
		long pad = largestSize % largestAlign;
		long end = baseOffset + largestSize + pad;
		// Chcck if fits in the extra space an if it is below the expected end
		if (endBound >= end && end - baseOffset >= largestSize) {
			__auto_type find = strIRVarRefsSortedFind(*refs, clone[v], IRVarRefsCmp);
			long offset = baseOffset + pad;
			find[0]->offset = offset;
			*order = strIRVarRefsAppendItem(*order, *find);
			*refs = strIRVarRefsRemoveItem(*refs, clone[v], IRVarRefsCmp);
			pack(baseOffset, offset, refs, order);
			pack(end, endBound, refs, order);
			return end;
		}
	}
	return endBound;
}
PTR_MAP_FUNCS(struct parserVar*, strGraphNodeIRP, VarRefs);
STR_TYPE_DEF(llVertexColor,VertexColors);
STR_TYPE_FUNCS(llVertexColor,VertexColors);
MAP_TYPE_DEF(struct IRVarRefsPair*, RefsPair);
MAP_TYPE_FUNCS(struct IRVarRefsPair*, RefsPair);
static void strVertexColorsDestroy2(strVertexColors *str) {
		for(long l=0;l!=strVertexColorsSize(*str);l++)
				llVertexColorDestroy(&str[0][l], NULL);
		strVertexColorsDestroy(str);
}
static int namedVarFilter(graphNodeIR node,const void *data) {
		struct IRNodeValue *val=(void*)graphNodeIRValuePtr(node);
		if(val->base.type==IR_VALUE) {
				strVar Data=(void*)data;
				if(strVarSortedFind(Data, val->val.value.var ,IRVarCmp))
						return 0;
		}
		return 1;
}
static void IRVarRefsMerge(struct IRVarRefsPair *a,struct IRVarRefsPair *b) {
		a->refs=strGraphNodeIRPSetUnion(a->refs, b->refs, (gnCmpType)ptrPtrCmp);
		a->vars=strVarSetUnion(a->vars, b->vars, IRVarCmp);
		a->largestSize=(a->largestSize>b->largestSize)?a->largestSize:b->largestSize;
		a->largestAlign=(a->largestAlign>b->largestAlign)?a->largestAlign:b->largestAlign;
}
static int recolorSoNoConflict(llVertexColor colors,graphNodeIRLive node) {
		strGraphNodeIRLiveP in =graphNodeIRLiveIncomingNodes(node);
		strGraphNodeIRLiveP out CLEANUP(strGraphNodeIRLivePDestroy)=graphNodeIRLiveOutgoingNodes(node);
		strGraphNodeIRLiveP adj CLEANUP(strGraphNodeIRLivePDestroy)=strGraphNodeIRLivePSetUnion(in, out, (gnCmpType)ptrPtrCmp);
		int adjColors[strGraphNodeIRPSize(adj)];
		int max=INT_MIN;
		for(long a=0;a!=strGraphNodeIRLivePSize(adj);a++) {
				adjColors[a]=llVertexColorGet(colors, adj[a])->color;
				max=(max>adjColors[a])?max:adjColors[a];
		}
		for(long c=0;c!=max;++c) {
				for(long f=0;f!=strGraphNodeIRPSize(adj);f++)
						if(adjColors[f]==c)
								goto next;
				llVertexColorGet(colors, node)->color=c;
				return c;
		next:;
		}
		llVertexColorGet(colors, node)->color=max+1;
		return max+1;
}
static void IRAttrVariableDestroy(struct IRAttr *a) {
		struct IRAttrVariable *var=(void*)a;
		variableDestroy(var->var.var);
}
void IRComputeFrameLayout(graphNodeIR start, long *frameSize,ptrMapFrameOffset *offsets) {
	strGraphNodeIRP allNodes CLEANUP(strGraphNodeIRPDestroy)=graphNodeIRAllNodes(start);
	strIRVarRefs allRefs CLEANUP(strIRVarRefsDestroy2)=NULL;
	for(long n=0;n!=strGraphNodeIRPSize(allNodes);n++) {
			struct IRNodeValue *val=(void*)graphNodeIRValuePtr(allNodes[n]);
			if(val->base.type!=IR_VALUE) continue;
			if(val->val.type!=IR_VAL_VAR_REF) continue;
			__auto_type var=val->val.value.var;
			struct IRVarRefsPair dummy;
			dummy.largestAlign=objectAlign(var.var->type, NULL);
			dummy.largestSize=objectSize(var.var->type, NULL);
			dummy.offset=0;
			dummy.refs=strGraphNodeIRPAppendItem(NULL, allNodes[n]);
			dummy.vars=strVarAppendItem(NULL, var);

			__auto_type find=IRVarRefsFindVar(allRefs,&var);
			if(find) {
					IRVarRefsMerge(find,&dummy);
					struct IRVarRefsPair *alloced=ALLOCATE(dummy);
					IRVarRefsPairDestroy(&alloced);
			} else {
					allRefs=strIRVarRefsSortedInsert(allRefs, ALLOCATE(dummy), IRVarRefsCmp);
 		}
	}
	strGraphNodeIRLiveP graphs CLEANUP(strGraphNodeIRLivePDestroy)= IRInterferenceGraphFilter(start,NULL,NULL);
	__auto_type byColor=mapRefsPairCreate();
	loop:
	if(strGraphNodeIRLivePSize(graphs)) {
			strGraphNodeIRLiveP colorBlob CLEANUP(strGraphNodeIRPDestroy)=graphIRLiveAllAccesableNodes(graphs[0]);
			graphIsolateFromUnaccessable(colorBlob[0]);
			graphs=strGraphNodeIRLivePSetDifference(graphs, colorBlob, (gnCmpType)ptrPtrCmp); 

			llVertexColor colors=graphColor(colorBlob[0]);
			for(long n=0;n!=strGraphNodeIRLivePSize(colorBlob);n++) {
					long color=llVertexColorGet(colors, colorBlob[n])->color;
					color=recolorSoNoConflict(colors,colorBlob[n]);
					char buffer[512];
					sprintf(buffer, "%li", color);

					// Ensure variable exists in allRefs
					__auto_type refPairFind=IRVarRefsFindVar(allRefs,&graphNodeIRLiveValuePtr(colorBlob[n])->ref);
					if(!refPairFind)
							continue;
					
					__auto_type find=mapRefsPairGet(byColor, buffer);
					if(!find) {
							mapRefsPairInsert(byColor, buffer, refPairFind);
					} else if(*find!=refPairFind){
							IRVarRefsMerge(*find,refPairFind);
							IRVarRefsPairDestroy(&refPairFind);
					}
					allRefs=strIRVarRefsRemoveItem(allRefs, refPairFind, IRVarRefsCmp);
			}
			llVertexColorDestroy(&colors, NULL);
	}
	//strIRVarRefsDestroy2(&allRefs);
	
	long kCount;
	mapRefsPairKeys(byColor, NULL, &kCount);
	const char *keys[kCount];
	mapRefsPairKeys(byColor, keys,NULL);
	for(long k=0;k!=kCount;k++)
			allRefs=strIRVarRefsSortedInsert(allRefs, *mapRefsPairGet(byColor, keys[k]), IRVarRefsCmp);
	
	mapRefsPairDestroy(byColor, NULL);
	
	long currentOffset=0;
	strIRVarRefs order CLEANUP(strIRVarRefsDestroy2)=NULL;
	currentOffset=pack(0, LONG_MAX, &allRefs, &order);

	__auto_type localVarFrameOffsets = ptrMapFrameOffsetCreate();
	for(long o=0;o!=strIRVarRefsSize(order);o++) {
			for(long v=0;v!=strVarSize(order[o]->vars);v++)
					ptrMapFrameOffsetAdd(localVarFrameOffsets, order[o]->vars[v].var, order[o]->offset+order[o]->largestSize);
	}
	
		for (long o = 0; o != strIRVarRefsSize(order); o++) {
				for(long n=0;n!=strGraphNodeIRPSize(order[o]->refs);n++) {
						struct IRNodeValue *ir = (void *)graphNodeIRValuePtr(order[o]->refs[n]);
				if (ir->base.type != IR_VALUE)
						continue;
				if (ir->val.type != IR_VAL_VAR_REF)
						continue;
				if (ir->val.value.var.var->isGlobal)
						continue;
				
				__auto_type find = ptrMapFrameOffsetGet(localVarFrameOffsets, ir->val.value.var.var);
				assert(find);
				__auto_type frameReference = IRCreateFrameAddress(*find, ir->val.value.var.var->type);
				struct IRAttrVariable attr;
				attr.base.name=IR_ATTR_VARIABLE;
				attr.base.destroy=IRAttrVariableDestroy;
				attr.var=ir->val.value.var,ir->val.value.var.var->refCount++;
				IRAttrReplace(frameReference, __llCreate(&attr, sizeof(attr)));
				
				strGraphNodeIRP dummy CLEANUP(strGraphNodeIRPDestroy) = strGraphNodeIRPAppendItem(NULL, order[o]->refs[n]);
				graphIRReplaceNodes(dummy, frameReference, NULL, (void (*)(void *))IRNodeDestroy);
		}
}
	
		if (frameSize) {
				*frameSize = 0;
				if(strIRVarRefsSize(order)) {
						__auto_type top=order[strIRVarRefsSize(order)-1];
						*frameSize=top->offset+top->largestSize;						
				}
		}
		
		if(offsets)
				*offsets=localVarFrameOffsets;
}
