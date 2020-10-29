#include <graphColoring.h>
#include <readersWritersLock.h>
#define DEBUG_PRINT_ENABLE 1
#include <debugPrint.h>
static int degree(const struct __graphNode *node) {
	__auto_type out = __graphNodeOutgoing(node);
	__auto_type in = __graphNodeIncoming(node);

	int degree = strGraphEdgePSize(out) + strGraphEdgePSize(in);

	strGraphEdgePDestroy(&in);
	strGraphEdgePDestroy(&out);

	return degree;
}
struct vertexInfo {
	struct __graphNode *node;
	int pri;
	_Atomic int counter;
	int color;
	strGraphNodeP prev;
	strGraphNodeP succ;
	struct rwLock *lock;
};
LL_TYPE_DEF(struct vertexPriority, Data);
LL_TYPE_FUNCS(struct vertexInfo, Data);
static int ptrPtrCmp(const void *a, const void *b) {
	if (*(void **)a > *(void **)b)
		return 1;
	else if (*(void **)a < *(void **)b)
		return -1;
	else
		return 0;
}
static int llVertexColorInsertCmp(const void *a, const void *b) {
	const struct vertexColoring *A = a, *B = b;
	return ptrPtrCmp(&A->node, &B->node);
}
static int llVertexColorGetCmp(const void *a, const void *b) {
	const struct vertexColoring *B = b;
	return ptrPtrCmp(&a, &B->node);
}
static int llDataGetCmp(const void *a, const void *b) {
	const struct vertexInfo *B = b;
	return ptrPtrCmp(&a, &B->node);
}
static int llDataInsertCmp(const void *a, const void *b) {
	const struct vertexInfo *A = a, *B = b;
	return ptrPtrCmp(&A->node, &B->node);
}
struct vertexColoring *llVertexColorGet(const llVertexColor data,
                                        const struct __graphNode *node) {
	return llVertexColorValuePtr(llVertexColorFindRight(
	    llVertexColorFirst(data), node, llVertexColorGetCmp));
}
static void visitNode(struct __graphNode *node, void *data) {
	strGraphNodeP *vec = data;
	if (NULL == strGraphNodePSortedFind(*vec, node, ptrPtrCmp))
		*vec = strGraphNodePSortedInsert(*vec, node, ptrPtrCmp);
}
static int alwaysTrue(const struct __graphNode *node,
                      const struct __graphEdge *edge, const void *data) {
	return 1;
}
static struct vertexInfo *llDataGet(const llData data,
                                    const struct __graphNode *node) {
	return llDataValuePtr(llDataFindRight(llDataFirst(data), node, llDataGetCmp));
}
STR_TYPE_DEF(int, Int);
STR_TYPE_FUNCS(int, Int);
// TODO switch data/item
struct __predPair {
	llData data;
	strGraphNodeP preds;
};
static int predHasColor(const void *item, const void *data) {
	const struct __predPair *pair = data;
	for (long i = 0; i != strGraphNodePSize(pair->preds); i++) {
		__auto_type color = llDataGet(pair->data, pair->preds[i]);
		DEBUG_PRINT("NODE %i has color %i\n",
		            *(int *)__graphNodeValuePtr(pair->preds[i]), color->color);
		if (color->color == *(int *)item) {

			return 1;
		}
	}

	return 0;
}
static int getColor(llData data, struct vertexInfo *nodeInfo) {
	long len = strGraphNodePSize(nodeInfo->prev);
	if (len == 0)
		return 0;

	strInt colors2 = strIntResize(NULL, len + 1);
	for (long i = 0; i != len + 1; i++) {
		if (i < len)
			rwReadStart(llDataGet(data, nodeInfo->prev[i])->lock);

		colors2[i] = i;
	}

	struct __predPair pair;
	pair.preds = nodeInfo->prev;
	colors2 = strIntRemoveIf(colors2, predHasColor, &pair);

	int minColor = colors2[0];
	DEBUG_PRINT("NODE %i has Min color %i\n",
	            *(int *)__graphNodeValuePtr(nodeInfo->node), minColor);

	for (long i = 0; i != len; i++)
		rwReadEnd(llDataGet(data, nodeInfo->prev[i])->lock);

	strIntDestroy(&colors2);
	return minColor;
}
static void jpColor(llData data, struct __graphNode *node) {
	__auto_type nodeData = llDataGet(data, node);

	int color = getColor(data, nodeData);

	rwWriteStart(nodeData->lock);
	nodeData->color = color;
	DEBUG_PRINT("NODE %i's COLOR IS %i\n", *(int *)__graphNodeValuePtr(node),
	            nodeData->color);
	rwWriteEnd(nodeData->lock);

	for (long i = 0; i != strGraphNodePSize(nodeData->succ); i++) {
		__auto_type succ = llDataGet(data, nodeData->succ[i]);
		if (--succ->counter == 0) {
			jpColor(data, nodeData->succ[i]);
		}
	}
}
llVertexColor graphColor(const struct __graphNode *node) {
	__auto_type allNodes =
	    strGraphNodePAppendItem(NULL, (struct __graphNode *)node);
	__graphNodeVisitForward((struct __graphNode *)node, &allNodes, alwaysTrue,
	                        visitNode);
	__graphNodeVisitBackward((struct __graphNode *)node, &allNodes, alwaysTrue,
	                         visitNode);

	llData datas = NULL;
	for (long i = 0; i != strGraphNodePSize(allNodes); i++) {
		struct vertexInfo tmp;
		tmp.pri = degree(allNodes[i]);
		tmp.node = allNodes[i];
		tmp.lock = rwLockCreate();
		tmp.counter = -1;
		tmp.color = -1;
		tmp.succ = NULL;
		tmp.prev = NULL;

		datas = llDataInsert(datas, llDataCreate(tmp), llDataInsertCmp);
	}

	for (long i = 0; i != strGraphNodePSize(allNodes); i++) {
		__auto_type v = llDataGet(datas, allNodes[i]);

		__auto_type out = __graphNodeOutgoing(allNodes[i]);
		__auto_type in = __graphNodeIncoming(allNodes[i]);
		for (long i2 = 0; i2 != strGraphEdgePSize(out); i2++) {
			__auto_type u = llDataGet(datas, __graphEdgeOutgoing(out[i2]));

			if (v->pri > u->pri)
				v->succ = strGraphNodePSortedInsert(v->succ, u->node, ptrPtrCmp);
			else if (v->pri < u->pri)
				v->prev = strGraphNodePSortedInsert(v->succ, u->node, ptrPtrCmp);
		}

		for (long i2 = 0; i2 != strGraphEdgePSize(in); i2++) {
			__auto_type u = llDataGet(datas, __graphEdgeIncoming(in[i2]));

			if (v->pri > u->pri)
				v->succ = strGraphNodePSortedInsert(v->succ, u->node, ptrPtrCmp);
			else if (v->pri < u->pri)
				v->prev = strGraphNodePSortedInsert(v->succ, u->node, ptrPtrCmp);
		}

		v->counter = strGraphNodePSize(v->prev);
		DEBUG_PRINT("NODE %i,succs:%li,preds:%li\n",
		            *(int *)__graphNodeValuePtr(v->node),
		            strGraphNodePSize(v->succ), strGraphNodePSize(v->prev));

		strGraphEdgePDestroy(&out);
		strGraphEdgePDestroy(&in);
	}

	for (long i = 0; i != strGraphNodePSize(allNodes); i++) {
		// Parallel

		if (llDataGet(datas, allNodes[i])->prev == NULL) {
			jpColor(datas, allNodes[i]);
		}
	}

	llVertexColor colors = NULL;
	for (long i = 0; i != strGraphNodePSize(allNodes); i++) {
		struct vertexColoring coloring;
		coloring.color = llDataGet(datas, allNodes[i])->color;
		coloring.node = allNodes[i];

		colors = llVertexColorInsert(colors, llVertexColorCreate(coloring),
		                             llVertexColorInsertCmp);

		rwLockDestroy(llDataGet(datas, allNodes[i])->lock);
	}
	llDataDestroy(&datas, NULL);

	return colors;
}
