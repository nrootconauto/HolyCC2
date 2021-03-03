#include <IR.h>
#include <IRLiveness.h>
#include <assert.h>
#include <cleanup.h>
#include <frameLayout.h>
#include <graphColoring.h>
#include <hashTable.h>
#include <limits.h>
#include <stdio.h>
#include <ptrMap.h>
#define ALLOCATE(x)                                                                                                                                                \
	({                                                                                                                                                               \
		__auto_type len = sizeof(x);                                                                                                                                   \
		void *$retVal = malloc(len);                                                                                                                                   \
		memcpy($retVal, &x, len);                                                                                                                                      \
		$retVal;                                                                                                                                                       \
	})
static int ptrPtrCmp(const void *a, const void *b) {
	if (*(void **)a > *(void **)b)
		return 1;
	else if (*(void **)a < *(void **)b)
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
strFrameEntry IRComputeFrameLayout(graphNodeIR start, long *frameSize) {
	strFrameEntry retVal = NULL;
	long currentOffset = 0;

	// Find list of frame address
	ptrMapVarRefs refs=ptrMapVarRefsCreate();
	strGraphNodeIRP allNodes CLEANUP(strGraphNodeIRPDestroy) = graphNodeIRAllNodes(start);
	for (long n = 0; n != strGraphNodeIRPSize(allNodes); n++) {
		struct IRNodeValue *val = (void *)graphNodeIRValuePtr(allNodes[n]);
		if (val->base.type != IR_VALUE)
			continue;
		if (val->val.type != IR_VAL_VAR_REF)
			continue;
		if (isGlobal(allNodes[n], NULL))
			continue;
	loop:;
		__auto_type find = ptrMapVarRefsGet(refs, val->val.value.var.var);
		if (find) {
				*find=strGraphNodeIRPSortedInsert(*find,  allNodes[n], (gnCmpType)ptrPtrCmp);
		} else {
				ptrMapVarRefsAdd(refs,val->val.value.var.var, NULL);
				goto loop;
		}
	}

	strGraphNodeIRLiveP graphs CLEANUP(strGraphNodeIRLivePDestroy)=IRInterferenceGraphFilter(start, NULL, NULL);
	strVertexColors graphColorings CLEANUP(strVertexColorsDestroy2)=NULL;
	mapRefsPair byColor=mapRefsPairCreate();
	for(long g=0;g!=strGraphNodeIRLivePSize(graphs);g++) {
			llVertexColor colors =graphColor(graphs[g]);
			for(llVertexColor cur=llVertexColorFirst(colors);cur!=NULL;cur=llVertexColorNext(cur)) {
					__auto_type liveNode=graphNodeIRLiveValuePtr(llVertexColorValuePtr(cur)->node);
					__auto_type findRefs=ptrMapVarRefsGet(refs,liveNode->ref.var);
					if(!findRefs)
							continue;
					
					char buffer[32];
					sprintf(buffer, "%i", llVertexColorValuePtr(cur)->color);
			colorLoop:;
					__auto_type find=mapRefsPairGet(byColor,  buffer);
					if(find) {
							long align=objectAlign(liveNode->ref.var->type,NULL);
							long size=objectSize(liveNode->ref.var->type,NULL);
							find[0]->largestAlign=(find[0]->largestAlign<align)?align:find[0]->largestAlign;
							find[0]->largestSize=(find[0]->largestSize<size)?size:find[0]->largestAlign;
							
							find[0]->refs=strGraphNodeIRPSetUnion(find[0]->refs, *findRefs, (gnCmpType)ptrPtrCmp);

							find[0]->vars=strVarSortedInsert(find[0]->vars, liveNode->ref, IRVarCmp);
					} else {
							struct IRVarRefsPair pair;
							pair.refs=NULL;
							pair.vars=NULL;
							pair.largestAlign=0;
							pair.largestSize=0;
							mapRefsPairInsert(byColor, buffer, ALLOCATE(pair));
							goto colorLoop;
					}
			}

			graphNodeIRLiveKillGraph(&graphs[g], NULL, NULL);
	}
	
	strIRVarRefs order CLEANUP(strIRVarRefsDestroy2) = NULL;
	long offset = 0;

	strIRVarRefs allRefs CLEANUP(strIRVarRefsDestroy2)=NULL;
	long count;
	mapRefsPairKeys(byColor,NULL, &count);
	const char *keys[count];
	mapRefsPairKeys(byColor,keys, NULL);
	for(long k=0;k!=count;k++) {
			allRefs=strIRVarRefsSortedInsert(allRefs, *mapRefsPairGet(byColor, keys[k]),IRVarRefsCmp);
	}

	mapRefsPairDestroy(byColor, NULL);
	
	
	while (strIRVarRefsSize(allRefs)) {
		assert(offset != LONG_MAX);
		offset = pack(offset, LONG_MAX, &allRefs, &order);
	}
	for (long r = 0; r != strIRVarRefsSize(order); r++) {
			for(long v=0;v!=strVarSize(order[r]->vars);v++) {
					currentOffset = order[r]->offset + objectSize(order[r]->vars[v].var->type, NULL);
					struct frameEntry entry;
					entry.offset = order[r]->offset + objectSize(order[r]->vars[v].var->type, NULL);
					entry.var = order[r]->vars[v];
					retVal = strFrameEntryAppendItem(retVal, entry);
			}
	}

	if (frameSize)
		*frameSize = currentOffset;
	return retVal;
}
