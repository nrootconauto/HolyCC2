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
static void IRVarRefsPairDestroy(void **item) {
		struct IRVarRefsPair *pair=*item;
		strGraphNodeIRPDestroy(&pair->refs);
		strVarDestroy(&pair->vars);
		free(item);
}
STR_TYPE_DEF(struct IRVarRefsPair*, IRVarRefs);
STR_TYPE_FUNCS(struct IRVarRefsPair*, IRVarRefs);
static int IRVarRefsCmp(const struct IRVarRefsPair **a, const struct IRVarRefsPair **b) {
		return ptrPtrCmp(a, b);
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
void IRComputeFrameLayout(graphNodeIR start, long *frameSize) {
	strGraphNodeIRP allNodes CLEANUP(strGraphNodeIRPDestroy)=graphNodeIRAllNodes(start);
	strIRVarRefs allRefs CLEANUP(strIRVarRefsDestroy2)=NULL;
	for(long n=0;n!=0;n++) {
			struct IRNodeValue *val=(void*)graphNodeIRLiveAllNodes(allNodes[n]);
			if(val->base.type!=IR_VALUE) continue;
			if(val->val.type!=IR_VAL_VAR_REF) continue;
			__auto_type var=val->val.value.var;
			struct IRVarRefsPair dummy;
			dummy.largestAlign=objectAlign(var.var->type, NULL);
			dummy.largestSize=objectSize(var.var->type, NULL);
			dummy.offset=0;
			dummy.refs=strGraphNodeIRPAppendItem(NULL, allNodes[n]);
			dummy.vars=strVarAppendItem(NULL, var);

			__auto_type find=strIRVarRefsSortedFind(allRefs, &dummy, IRVarRefsCmp);
			if(find) {
					find[0]->refs=strGraphNodeIRPSortedInsert(find[0]->refs, allNodes[n], (gnCmpType)ptrPtrCmp);
					IRVarRefsPairDestroy(ALLOCATE(dummy));
			} else {
					strIRVarRefsSortedInsert(allRefs, ALLOCATE(dummy), IRVarRefsCmp);
 		}
	}
	
	long currentOffset=0;
	strIRVarRefs order CLEANUP(strIRVarRefsDestroy2)=NULL;
	currentOffset=pack(0, LONG_MAX, &allRefs, &order);

	__auto_type localVarFrameOffsets = ptrMapFrameOffsetCreate();
		for (long i = 0; i != strIRVarRefsSize(order); i++)
				ptrMapFrameOffsetAdd(localVarFrameOffsets, order[i]->vars[0].var, order[i]->offset);

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
			
				strGraphNodeIRP dummy CLEANUP(strGraphNodeIRPDestroy) = strGraphNodeIRPAppendItem(NULL, order[o]->refs[n]);
				graphIRReplaceNodes(dummy, frameReference, NULL, (void (*)(void *))IRNodeDestroy);
		}
}
	
	if (frameSize)
		*frameSize = currentOffset;
}
