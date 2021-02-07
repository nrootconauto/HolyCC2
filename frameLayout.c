#include <IR.h>
#include <IRLiveness.h>
#include <assert.h>
#include <cleanup.h>
#include <frameLayout.h>
#include <graphColoring.h>
#include <hashTable.h>
#include <stdio.h>
#include <limits.h>
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
		struct IRVar var;
		strGraphNodeIRP refs;
		long offset;
};
STR_TYPE_DEF(struct IRVarRefsPair,IRVarRefs);
STR_TYPE_FUNCS(struct IRVarRefsPair,IRVarRefs);
static int IRVarRefsCmp(const struct IRVarRefsPair *a,const struct IRVarRefsPair *b) {
		return IRVarCmp(&a->var,&b->var);
}
static void IRVarRefsDestroy2(strIRVarRefs *refs) {
		for(long r=0;r!=strIRVarRefsSize(*refs);r++)
				strGraphNodeIRPDestroy(&refs[0][r].refs);
		strIRVarRefsDestroy(refs);
}
static int IRVarRefsSizeCmpRev(const void *a,const void *b) {
		const struct IRVarRefsPair *A=a,*B=b;
		__auto_type bSize= objectSize(B->var.var->type,NULL);
		__auto_type aSize=objectSize(A->var.var->type,NULL);
		return bSize-aSize;
}
static long pack(long baseOffset,long endBound,strIRVarRefs *refs,strIRVarRefs *order) {
		long len=strIRVarRefsSize(*refs);
		struct IRVarRefsPair clone[len];
		memcpy(clone, *refs, sizeof(*clone)*len);
		qsort(clone, len,sizeof(*clone), IRVarRefsSizeCmpRev);

		for(long v=0;v!=len;v++) {
				long largestSize=objectSize(clone[v].var.var->type,NULL);
				long largestAlign=objectAlign(clone[v].var.var->type, NULL);
				long pad=largestSize%largestAlign;
				long end=baseOffset+largestSize+pad;
				//Chcck if fits in the extra space an if it is below the expected end
				if(endBound>=end&&end-baseOffset>=largestSize) {
						__auto_type find=strIRVarRefsSortedFind(*refs, clone[v], IRVarRefsCmp);
						long offset=baseOffset+pad;
						find->offset=offset;
						*order=strIRVarRefsAppendItem(*order, *find);
						*refs=strIRVarRefsRemoveItem(*refs, clone[v], IRVarRefsCmp);
						pack(baseOffset, offset,refs,order);
						pack(end,endBound,refs,order);
						return end;
				}
		}
		return endBound;
}
strFrameEntry IRComputeFrameLayout(graphNodeIR start, long *frameSize) {
	strFrameEntry retVal = NULL;
	long currentOffset=0;
	
	//Find list of frame address
	strIRVarRefs refs CLEANUP(IRVarRefsDestroy2)=NULL;
	strGraphNodeIRP allNodes CLEANUP(strGraphNodeIRPDestroy)=graphNodeIRAllNodes(start);
	for(long n=0;n!=strGraphNodeIRPSize(allNodes);n++) {
			struct IRNodeValue *val=(void*)graphNodeIRValuePtr(allNodes[n]);
			if(val->base.type!=IR_VALUE)
					continue;
			if(val->val.type!=IR_VAL_VAR_REF)
					continue;
			if(isGlobal(allNodes[n], NULL))
					continue;
			
			struct IRVarRefsPair dummy;
			dummy.refs=NULL;
			dummy.var=val->val.value.var;
	loop:;
			__auto_type find=strIRVarRefsSortedFind(refs, dummy, IRVarRefsCmp);
			if(find) {
					find->refs=strGraphNodeIRPSortedInsert(find->refs, allNodes[n], (gnCmpType)ptrPtrCmp);
			} else {
					refs=strIRVarRefsSortedInsert(refs, dummy, IRVarRefsCmp);
					goto loop;
			}
	}

	strIRVarRefs order CLEANUP(IRVarRefsDestroy2)=NULL;
	long offset=0;
	while(strIRVarRefsSize(refs)) {
			assert(offset!=LONG_MAX);
			offset=pack(offset, LONG_MAX, &refs,&order);
	}
	for(long r=0;r!=strIRVarRefsSize(order);r++) {
			currentOffset=order[r].offset+objectSize(order[r].var.var->type, NULL);
			struct frameEntry entry;
			entry.offset=order[r].offset;
			entry.var=order[r].var;
			retVal=strFrameEntryAppendItem(retVal, entry);
	}
	
	if (frameSize)
			*frameSize = currentOffset;
	return retVal;
}
