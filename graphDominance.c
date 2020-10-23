#include <graph.h>
#include <linkedList.h>
#include <str.h>
#include <graphDominance.h>
static int alwaysTrue(const struct __graphNode *node,
                      const struct __graphEdge *edge, const void *data) {
	return 1;
}
static int ptrCmp(const void *a, const void *b) {
	if (a > b)
		return 1;
	else if (b < a)
		return -1;
	else
		return 0;
}
int llDominatorCmp(const void *a, const void *b) {
	const struct graphDominators *A = a;
	const struct graphDominators *B = b;
	return ptrCmp(A->node, B->node);
}
static void visitNode(struct __graphNode *node, void *visited) {
	strGraphNodeP *visited2 = visited;
	if (NULL == strGraphNodePSortedFind(*visited2, node, ptrCmp)) {
		*visited2 = strGraphNodePSortedInsert(*visited2, node, ptrCmp);
	}
}
static struct graphDominators *
llDominatorsFind(llDominators list, const struct __graphNode *node) {
	return llDominatorsValuePtr(
	    llDominatorsFindRight(llDominatorsFirst(list), node, ptrCmp));
}
llDominators graphComputeDominatorsPerNode(struct __graphNode *start) {
	strGraphNodeP allNodes = strGraphNodePAppendItem(NULL, start);
	__graphNodeVisitForward(start, &allNodes, alwaysTrue, visitNode);
	__graphNodeVisitBackward(start, &allNodes, alwaysTrue, visitNode);

	llDominators list = NULL;
	for (long i = 0; i != strGraphNodePSize(allNodes); i++) {
		struct graphDominators tmp;
		tmp.dominators = strGraphNodePAppendItem(NULL, allNodes[i]);
		tmp.node = allNodes[i];

		__auto_type newNode = llDominatorsCreate(tmp);
		list = llDominatorsInsert(list, newNode, llDominatorCmp);
	}

	int changed = 1;
	while (!changed) {
		for (long i = 0; i != strGraphNodePSize(allNodes); i++) {
			if (allNodes[i] == start)
				continue;

			__auto_type currentNode = llDominatorsFind(list, allNodes[i]);
			__auto_type old = strGraphNodePAppendData(
			    NULL, (const struct __graphNode **)currentNode->dominators,
			    strGraphNodePSize(currentNode->dominators));

			__auto_type currentItems = strGraphNodePAppendData(
			    NULL, (const struct __graphNode **)old, strGraphNodePSize(old));
			
			__auto_type preds = __graphNodeIncoming(allNodes[i]);
			for (long i = 0; i != strGraphEdgePSize(preds); i++) {
				__auto_type incoming = __graphEdgeIncoming(preds[i]);

				__auto_type current = llDominatorsFind(list, incoming);
				currentItems = strGraphNodePSetDifference(currentItems,
				                                          current->dominators, ptrCmp);
			}

			if (strGraphNodePSize(currentItems) == strGraphNodePSize(old)) {
				if (0 != memcmp(currentItems, old,
				                strGraphNodePSize(old) * sizeof(struct __graphNode *)))
					changed = 1;
			} else {
				changed = 1;
			}

			strGraphEdgePDestroy(&preds);
			strGraphNodePDestroy(&old);
			strGraphNodePDestroy(&currentItems);
		}
	}

	return list;
}
