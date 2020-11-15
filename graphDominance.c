#include <graph.h>
#include <graphDominance.h>
#include <linkedList.h>
#include <stdio.h>
#include <str.h>
#include <stdlib.h>
static int alwaysTrue(const struct __graphNode *node,
                      const struct __graphEdge *edge, const void *data) {
	return 1;
}
static int ptrPtrCmp(const void *a, const void *b) {
	if (*(void **)a > *(void **)b)
		return 1;
	else if (*(void **)a < *(void **)b)
		return -1;
	else
		return 0;
}
int llDominatorCmp(const void *a, const void *b) {
	const struct graphDominators *B = b;
	return ptrPtrCmp(&a, &B->node);
}
int llDomFrontierCmp(const void *a, const void *b) {
	const struct graphDomFrontier *B = b;
	return ptrPtrCmp(&a, &B->node);
}
static int llDomFrontierCmpInsert(const void *a, const void *b) {
	const struct graphDomFrontier *A = a;
	const struct graphDomFrontier *B = b;
	return ptrPtrCmp(&A->node, &B->node);
}
static int llDominatorCmpInsert(const void *a, const void *b) {
	const struct graphDominators *A = a;
	const struct graphDominators *B = b;
	return ptrPtrCmp(&A->node, &B->node);
}
static void visitNode(struct __graphNode *node, void *visited) {
	strGraphNodeP *visited2 = visited;
	if (NULL == strGraphNodePSortedFind(*visited2, node, ptrPtrCmp)) {
		*visited2 = strGraphNodePSortedInsert(*visited2, node, ptrPtrCmp);
	}
}
static struct graphDominators *
llDominatorsFind2(llDominators list, const struct __graphNode *node) {
	return llDominatorsValuePtr(
	    llDominatorsFindRight(llDominatorsFirst(list), node, llDominatorCmp));
}
static strGraphNodeP uniqueUnion(strGraphNodeP items,
                                 const strGraphNodeP other) {
	for (long i = 0; i != strGraphNodePSize(other); i++) {
		if (NULL == strGraphNodePSortedFind(items, other[i], ptrPtrCmp)) {
			items = strGraphNodePSortedInsert(items, other[i], ptrPtrCmp);
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
		else
				tmp.dominators=strGraphNodePAppendData(NULL, (void*)allNodes, strGraphNodePSize(allNodes));
		__auto_type newNode = llDominatorsCreate(tmp);
		list = llDominatorsInsert(list, newNode, llDominatorCmpInsert);
	}

	int changed = 1;
	while (changed) {
		changed = 0;
		for (long i = 0; i != strGraphNodePSize(allNodes); i++) {
			if (allNodes[i] == start)
				continue;

			__auto_type currentNode = llDominatorsFind2(list, allNodes[i]);
			__auto_type old = strGraphNodePAppendData(
			    NULL, (const struct __graphNode **)currentNode->dominators,
			    strGraphNodePSize(currentNode->dominators));

			strGraphNodeP currentItems = NULL;

			__auto_type preds = __graphNodeIncoming(allNodes[i]);
			int referencesComputedNode = 0;
			for (long i = 0; i != strGraphEdgePSize(preds); i++) {
				__auto_type incoming = __graphEdgeIncoming(preds[i]);

				__auto_type current = llDominatorsFind2(list, incoming);

				// Intersection begins with first set,so append to current items
				if (i == 0) {
					currentItems = uniqueUnion(currentItems, current->dominators);
				}

				if (current->dominators != NULL) {
					currentItems = strGraphNodePSetIntersection(
					    currentItems, current->dominators, ptrPtrCmp, NULL);
					referencesComputedNode = 1;
				}
			}

			// Ensure current items include current node
			if (NULL == strGraphNodePSortedFind(currentItems, allNodes[i], ptrPtrCmp))
				currentItems =
				    strGraphNodePSortedInsert(currentItems, allNodes[i], ptrPtrCmp);

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

	strGraphNodePDestroy(&allNodes);
	return list;
}
static int domsLenCmp(const void *a, const void *b) {
	const struct graphDominators *A = a, *B = b;
	__auto_type aLen = strGraphNodePSize(A->dominators);
	__auto_type bLen = strGraphNodePSize(B->dominators);
	if (aLen > bLen)
		return 1;
	if (aLen < bLen)
		return -1;
	return 0;
}
static int nodeEqual(const void *b, const void *data) {
	return (struct __graphNode *)data == *(struct __graphNode **)b;
}
struct __graphNode *graphDominatorIdom(const llDominators doms,
                                       struct __graphNode *node) {
	__auto_type entry = llDominatorsFind2(doms, node);
	__auto_type clone = strGraphNodePAppendData(
	    NULL, (const struct __graphNode **)entry->dominators,
	    strGraphNodePSize(entry->dominators));
	//Remove node from self
	clone = strGraphNodePRemoveIf(clone, nodeEqual, node);

	for (int changed = 1; changed;) {
		changed = 0;
		for (long i = 0; i != strGraphNodePSize(clone); i++) {
			for (long i2 = 0; i2 != strGraphNodePSize(clone); i2++) {
				if (clone[i2] == node)
					continue;

				__auto_type i2Doms = llDominatorsFind2(doms, clone[i2]);
				/**
				 * clone[i] in [doms]
				 */
				printf("clone[i]:%i,clone[i2]:%i\n",
				       *(int *)__graphNodeValuePtr(clone[i]),
				       *(int *)__graphNodeValuePtr(clone[i2]));
				__auto_type find =
				    strGraphNodePSortedFind(i2Doms->dominators, clone[i], ptrPtrCmp);
				if (find != NULL) {
					/**
					 * Every node dominates itself,so ignore removing node that dominates
					 * itself
					 */
					if (*find == clone[i2])
						continue;

					clone = strGraphNodePRemoveIf(clone, nodeEqual, clone[i]);
					changed = 1;
					goto next;
				}
			}
		}
	next:;
	}
	__auto_type len = strGraphNodePSize(clone);
	qsort(clone, len, sizeof(struct __graphNode *), domsLenCmp);
	__auto_type retVal = clone[len - 1];

	printf("IDOM OF %i is %i\n", *(int *)__graphNodeValuePtr(node),
	       *(int *)__graphNodeValuePtr(retVal));
	strGraphNodePDestroy(&clone);
	return retVal;
}
llDomFrontier graphDominanceFrontiers(struct __graphNode *start,
                                      const llDominators doms) {
	strGraphNodeP allNodes = strGraphNodePAppendItem(NULL, start);
	__graphNodeVisitForward(start, &allNodes, alwaysTrue, visitNode);
	__graphNodeVisitBackward(start, &allNodes, alwaysTrue, visitNode);

	llDomFrontier fronts = NULL;
	for (long i = 0; i != strGraphNodePSize(allNodes); i++) {
		struct graphDomFrontier tmp;
		tmp.dominators = NULL;
		tmp.node = allNodes[i];

		fronts = llDomFrontierInsert(fronts, llDomFrontierCreate(tmp),
		                             llDomFrontierCmpInsert);
	}

	for (long b = 0; b != strGraphNodePSize(allNodes); b++) {
		__auto_type preds = __graphNodeIncoming(allNodes[b]);
		if (strGraphEdgePSize(preds) >= 2) {
			for (long p = 0; p != strGraphEdgePSize(preds); p++) {
				__auto_type runner = __graphEdgeIncoming(preds[p]);

				while (runner != graphDominatorIdom(doms, allNodes[b])) {
					// Add b to runners frontier
					__auto_type find = llDomFrontierFindRight(llDomFrontierFirst(fronts),
					                                          runner, llDomFrontierCmp);
					__auto_type value = llDomFrontierValuePtr(find);
					value->dominators =
					    strGraphNodePAppendItem(value->dominators, allNodes[b]);
					printf("RUNNER %i += %i\n", *(int *)__graphNodeValuePtr(runner),
					       *(int *)__graphNodeValuePtr(allNodes[b]));

					// runner = iDom(runner)
					find = llDomFrontierFindRight(llDomFrontierFirst(fronts),
					                              graphDominatorIdom(doms, runner),
					                              llDomFrontierCmp);
					value = llDomFrontierValuePtr(find);
					runner = value->node;
				}
			}
		}

		strGraphEdgePDestroy(&preds);
	}

	strGraphNodePDestroy(&allNodes);
	return fronts;
}
