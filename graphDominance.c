#include <graph.h>
#include <graphDominance.h>
#include <linkedList.h>
#include <stdio.h>
#include <str.h>
static int alwaysTrue(const struct __graphNode *node,
                      const struct __graphEdge *edge, const void *data) {
	return 1;
}
static int ptrCmp(const void *a, const void *b) {
	if (*(void **)a > *(void **)b)
		return 1;
	else if (*(void **)a < *(void **)b)
		return -1;
	else
		return 0;
}
int llDominatorCmp(const void *a, const void *b) {
	const struct graphDominators *B = b;
	return ptrCmp(&a, &B->node);
}
int llDominatorCmp2(const void *a, const void *b) {
	const struct graphDominators *A = a;
	const struct graphDominators *B = b;
	return ptrCmp(&A->node, &B->node);
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
	    llDominatorsFindRight(llDominatorsFirst(list), node, llDominatorCmp));
}
static strGraphNodeP uniqueUnion(strGraphNodeP items,
                                 const strGraphNodeP other) {
	for (long i = 0; i != strGraphNodePSize(other); i++) {
		if (NULL == strGraphNodePSortedFind(items, other[i], ptrCmp)) {
			items = strGraphNodePSortedInsert(items, other[i], ptrCmp);
		}
	}
	return items;
}
llDominators graphComputeDominatorsPerNode(struct __graphNode *start) {
	strGraphNodeP allNodes = strGraphNodePAppendItem(NULL, start);
	__graphNodeVisitForward(start, &allNodes, alwaysTrue, visitNode);
	__graphNodeVisitBackward(start, &allNodes, alwaysTrue, visitNode);

	llDominators list = NULL;
	for (long i = 0; i != strGraphNodePSize(allNodes); i++) {
		struct graphDominators tmp;
		tmp.dominators = NULL;
		tmp.node = allNodes[i];
		if (allNodes[i] == start)
			tmp.dominators = strGraphNodePAppendItem(NULL, start);

		__auto_type newNode = llDominatorsCreate(tmp);
		list = llDominatorsInsert(list, newNode, llDominatorCmp2);
	}

	int changed = 1;
	while (changed) {
		changed = 0;
		for (long i = 0; i != strGraphNodePSize(allNodes); i++) {
			if (allNodes[i] == start)
				continue;

			__auto_type currentNode = llDominatorsFind(list, allNodes[i]);
			__auto_type old = strGraphNodePAppendData(
			    NULL, (const struct __graphNode **)currentNode->dominators,
			    strGraphNodePSize(currentNode->dominators));

			strGraphNodeP currentItems = NULL;

			__auto_type preds = __graphNodeIncoming(allNodes[i]);
			int referencesComputedNode = 0;
			for (long i = 0; i != strGraphEdgePSize(preds); i++) {
				__auto_type incoming = __graphEdgeIncoming(preds[i]);

				__auto_type current = llDominatorsFind(list, incoming);

				// Intersection begins with first set,so append to current items
				if (i == 0) {
					currentItems = uniqueUnion(currentItems, current->dominators);
				}

				if (current->dominators != NULL) {
					currentItems = strGraphNodePSetIntersection(
					    currentItems, current->dominators, ptrCmp, NULL);
					referencesComputedNode = 1;
				}
			}

			// Ensure current items include current node
			if (NULL == strGraphNodePSortedFind(currentItems, allNodes[i], ptrCmp))
				currentItems =
				    strGraphNodePSortedInsert(currentItems, allNodes[i], ptrCmp);

			if (referencesComputedNode) {
				currentNode->dominators = currentItems;

				printf("%i->", *(int *)__graphNodeValuePtr(allNodes[i]));
				for (long i = 0; i != strGraphNodePSize(currentItems); i++) {
					int *v = __graphNodeValuePtr(currentItems[i]);
					printf("%i,", *v);
				}
				printf("\n");
			} else
				strGraphNodePDestroy(&currentItems);

			if (strGraphNodePSize(currentItems) == strGraphNodePSize(old)) {
				if (0 !=
				    memcmp(currentItems, old,
				           strGraphNodePSize(old) * sizeof(struct __graphNode *))) {
					printf("CHANGED:\n");
					changed = 1;
				}
			} else {
				changed = 1;
			}

			strGraphEdgePDestroy(&preds);
			strGraphNodePDestroy(&old);
		}
	}

	return list;
}
