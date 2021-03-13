#include "cleanup.h"
#include "debugPrint.h"
#include "graphColoring.h"
//
// http://supertech.csail.mit.edu/papers/HasenplaughKaSc14.pdf figure 8
//
static int intCmp(const int *a, const int *b) {
	return *(int *)a - *(int *)b;
}
typedef int (*gnCmpType)(const struct __graphNode **, const struct __graphNode **);
STR_TYPE_DEF(int, Int);
STR_TYPE_FUNCS(int, Int);
struct vertexInfo {
	struct __graphNode *node;
	int color;
	strGraphNodeP adjUncolored;
	strInt adjColors;
};
LL_TYPE_DEF(struct vertexPriority, Data);
LL_TYPE_FUNCS(struct vertexInfo, Data);
static int ptrPtrCmp(const void *a, const void *b) {
		if(*(void**)a>*(void**)b)
				return 1;
		else if(*(void**)a<*(void**)b)
				return -1;
		return 0;
}

static int llVertexColorInsertCmp(const struct vertexColoring *a, const struct vertexColoring *b) {
	const struct vertexColoring *A = a, *B = b;
	return ptrPtrCmp(&A->node, &B->node);
}
static int llVertexColorGetCmp(const void *a, const struct vertexColoring *b) {
	const struct vertexColoring *B = b;
	return ptrPtrCmp(&a, &B->node);
}
static int llDataGetCmp(const void *a, const struct vertexInfo *b) {
	const struct vertexInfo *B = b;
	return ptrPtrCmp(&a, &B->node);
}
static int llDataInsertCmp(const struct vertexInfo *a, const struct vertexInfo *b) {
	const struct vertexInfo *A = a, *B = b;
	return ptrPtrCmp(&A->node, &B->node);
}
struct vertexColoring *llVertexColorGet(const llVertexColor data, const struct __graphNode *node) {
	return llVertexColorValuePtr(llVertexColorFindRight(llVertexColorFirst(data), node, llVertexColorGetCmp));
}
static void visitNode(struct __graphNode *node, void *data) {
	strGraphNodeP *vec = data;
	if (NULL == strGraphNodePSortedFind(*vec, node, (gnCmpType)ptrPtrCmp))
		*vec = strGraphNodePSortedInsert(*vec, node, (gnCmpType)ptrPtrCmp);
}
static int alwaysTrue(const struct __graphNode *node, const struct __graphEdge *edge, const void *data) {
	return 1;
}
static struct vertexInfo *llDataGet(const llData data, const struct __graphNode *node) {
	return llDataValuePtr(llDataFindRight(llDataFirst(data), node, llDataGetCmp));
}
// TODO switch data/item
struct __predPair {
	llData data;
	strGraphNodeP preds;
};
static strGraphNodeP adj(struct __graphNode *node) {
	strGraphEdgeP out CLEANUP(strGraphEdgePDestroy) = __graphNodeOutgoing(node);
	strGraphEdgeP in CLEANUP(strGraphEdgePDestroy) = __graphNodeIncoming(node);

	strGraphNodeP retVal = NULL;

	for (long i = 0; i != strGraphEdgePSize(out); i++) {
		if (strGraphNodePSortedFind(retVal, __graphEdgeOutgoing(out[i]), (gnCmpType)ptrPtrCmp))
			continue;
		if (__graphEdgeOutgoing(out[i]) == node)
			continue;

		retVal = strGraphNodePSortedInsert(retVal, __graphEdgeOutgoing(out[i]), (gnCmpType)ptrPtrCmp);
	}

	for (long i = 0; i != strGraphEdgePSize(in); i++) {
		if (strGraphNodePSortedFind(retVal, __graphEdgeIncoming(in[i]), (gnCmpType)ptrPtrCmp))
			continue;
		if (__graphEdgeIncoming(in[i]) == node)
			continue;

		retVal = strGraphNodePSortedInsert(retVal, __graphEdgeIncoming(in[i]), (gnCmpType)ptrPtrCmp);
	}

	return retVal;
}
static int removeIfNodeEq(const void *a, const struct __graphNode **b) {
	const struct __graphNode **A = (void *)a, **B = (void *)b;
	return (*A == *B);
}
llVertexColor graphColor(const struct __graphNode *node) {
	__auto_type allNodes = __graphNodeVisitAll(node);
	__auto_type allNodesLen = strGraphNodePSize(allNodes);
	strGraphNodeP Q[allNodesLen][allNodesLen];
	for (long i = 0; i != allNodesLen; i++)
		for (long i2 = 0; i2 != allNodesLen; i2++)
			Q[i][i2] = NULL;

	llData datas = NULL;
	for (long i = 0; i != allNodesLen; i++) {
		struct vertexInfo tmp;
		tmp.node = allNodes[i];
		tmp.color = -1;
		tmp.adjColors = NULL;
		tmp.adjUncolored = adj(allNodes[i]);

		DEBUG_PRINT("NODE: %i,has %li adjs\n", *(int *)__graphNodeValuePtr(allNodes[i]), strGraphNodePSize(tmp.adjUncolored));

		datas = llDataInsert(datas, llDataCreate(tmp), llDataInsertCmp);

		__auto_type ptr = &Q[0][strGraphNodePSize(tmp.adjUncolored)];
		*ptr = strGraphNodePAppendItem(*ptr, allNodes[i]);
	}

	int s = 0;
	while (s >= 0) {
		long maxLen = 0, maxIndex = 0;
		for (long i = 0; i != allNodesLen; i++) {
			__auto_type len = strGraphNodePSize(Q[s][i]);
			if (len > maxLen) {
				maxLen = len;
				maxIndex = i;
			}
		}

		// Pop
		__auto_type v = Q[s][maxIndex][maxLen - 1];
		Q[s][maxIndex] = strGraphNodePResize(Q[s][maxIndex], maxLen - 1);

		__auto_type vData = llDataGet(datas, v);

		strInt valids = strIntResize(NULL, strIntSize(vData->adjColors) + 1);
		for (long i = 0; i != strIntSize(valids); i++)
			valids[i] = i + 1;
		valids = strIntSetDifference(valids, vData->adjColors, intCmp);

		vData->color = valids[0];
		DEBUG_PRINT("NODE: %i,has color %i \n", *(int *)__graphNodeValuePtr(vData->node), valids[0]);

		for (long i = 0; i != strGraphNodePSize(vData->adjUncolored); i++) {
			__auto_type uData = llDataGet(datas, vData->adjUncolored[i]);

			__auto_type ptr = &Q[strIntSize(uData->adjColors)][strGraphNodePSize(uData->adjUncolored)];
			*ptr = strGraphNodePRemoveIf(*ptr, &uData->node, removeIfNodeEq);

			DEBUG_PRINT("NODE: %i removed at %li ,%li\n", *(int *)__graphNodeValuePtr(uData->node), strIntSize(uData->adjColors), strGraphNodePSize(uData->adjUncolored));

			uData->adjColors = strIntSortedInsert(uData->adjColors, vData->color, intCmp);
			uData->adjUncolored = strGraphNodePRemoveIf(uData->adjUncolored, &vData->node, removeIfNodeEq);

			ptr = &Q[strIntSize(uData->adjColors)][strGraphNodePSize(uData->adjUncolored)];
			*ptr = strGraphNodePAppendItem(*ptr, uData->node);

			DEBUG_PRINT("NODE: %i inserted at %li ,%li\n", *(int *)__graphNodeValuePtr(uData->node), strIntSize(uData->adjColors),
			            strGraphNodePSize(uData->adjUncolored));

			long newS = strIntSize(uData->adjColors);
			s = (s > newS) ? s : newS;
		}
		while (s >= 0) {
			int isNull = 1;
			for (long i = 0; i != allNodesLen; i++) {
				if (strGraphNodePSize(Q[s][i]) != 0) {
					isNull = 0;
					break;
				}
			}
			if (isNull)
				s--;
			else
				break;
		}
	}

	llVertexColor colors = NULL;
	for (long i = 0; i != strGraphNodePSize(allNodes); i++) {
		struct vertexColoring coloring;
		coloring.color = llDataGet(datas, allNodes[i])->color;
		coloring.node = allNodes[i];

		colors = llVertexColorInsert(colors, llVertexColorCreate(coloring), llVertexColorInsertCmp);

		strGraphNodePDestroy(&llDataGet(datas, allNodes[i])->adjUncolored);
	}
	llDataDestroy(&datas, NULL);

	for (long i = 0; i != allNodesLen; i++)
		for (long i2 = 0; i2 != allNodesLen; i2++)
			strGraphNodePDestroy(&Q[i][i2]);

	return colors;
}
long vertexColorCount(const llVertexColor colors) {
	__auto_type first = llVertexColorFirst(colors);
	strInt unique = NULL;
	for (__auto_type node = first; node != NULL; node = llVertexColorNext(node))
		if (NULL == strIntSortedFind(unique, llVertexColorValuePtr(node)->color, intCmp)) {
			unique = strIntSortedInsert(unique, llVertexColorValuePtr(node)->color, intCmp);
		}
	long count = strIntSize(unique);
	return count;
}
