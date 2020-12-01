#include <assert.h>
#include <base64.h>
#include <graph.h>
#include <graphDominance.h>
#include <linkedList.h>
#include <stdio.h>
#include <stdlib.h>
#include <str.h>
typedef int (*geCmpType)(const struct __graphEdge **,
                         const struct __graphEdge **);
typedef int (*gnCmpType)(const struct __graphNode **,
                         const struct __graphNode **);

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
int llDominatorCmp(const void *a, const struct graphDominators *b) {
	return ptrPtrCmp(&a, &b->node);
}
int llDomFrontierCmp(const void *a, const struct graphDomFrontier *B) {
	return ptrPtrCmp(&a, &B->node);
}
static int llDomFrontierCmpInsert(const struct graphDomFrontier *A,
                                  const struct graphDomFrontier *B) {
	return ptrPtrCmp(&A->node, &B->node);
}
static int llDominatorCmpInsert(const struct graphDominators *A,
                                const struct graphDominators *B) {
	return ptrPtrCmp(&A->node, &B->node);
}
static void visitNode(struct __graphNode *node, void *visited) {
	strGraphNodeP *visited2 = visited;
	if (NULL == strGraphNodePSortedFind(*visited2, node, (gnCmpType)ptrPtrCmp)) {
		*visited2 =
		    strGraphNodePSortedInsert(*visited2, node, (gnCmpType)ptrPtrCmp);
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
		if (NULL ==
		    strGraphNodePSortedFind(items, other[i], (gnCmpType)ptrPtrCmp)) {
			items = strGraphNodePSortedInsert(items, other[i], (gnCmpType)ptrPtrCmp);
		}
	}
	return items;
}
llDominators graphComputeDominatorsPerNode(struct __graphNode *start) {
	strGraphNodeP allNodes = __graphNodeVisitAll(start);
	llDominators list = NULL;
	for (long i = 0; i != strGraphNodePSize(allNodes); i++) {
		struct graphDominators tmp;
		tmp.dominators = NULL;
		tmp.node = allNodes[i];
		if (allNodes[i] == start)
			tmp.dominators = strGraphNodePAppendItem(NULL, start);
		else
			tmp.dominators = strGraphNodePAppendData(NULL, (void *)allNodes,
			                                         strGraphNodePSize(allNodes));
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
			for (long i = 0; i != strGraphEdgePSize(preds); i++) {
				__auto_type incoming = __graphEdgeIncoming(preds[i]);

				__auto_type current = llDominatorsFind2(list, incoming);

				// Intersection begins with first set,so append to current items
				if (i == 0) {
					currentItems =
					    strGraphNodePAppendData(NULL, (void *)current->dominators,
					                            strGraphNodePSize(current->dominators));
				}

				if (current->dominators != NULL) {
					currentItems = strGraphNodePSetIntersection(
					    currentItems, current->dominators, (gnCmpType)ptrPtrCmp, NULL);
				}
			}

			// Ensure current items include current node
			if (NULL == strGraphNodePSortedFind(currentItems, allNodes[i],
			                                    (gnCmpType)ptrPtrCmp))
				currentItems = strGraphNodePSortedInsert(currentItems, allNodes[i],
				                                         (gnCmpType)ptrPtrCmp);

			currentNode->dominators = currentItems;

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
static int nodeEqual(const void *data, const struct __graphNode **b) {
	return (struct __graphNode *)data == *b;
}
static strGraphNodeP graphDominatorIdoms(const llDominators doms,
                                         struct __graphNode *node) {
	__auto_type entry = llDominatorsFind2(doms, node);
	__auto_type clone = strGraphNodePAppendData(
	    NULL, (const struct __graphNode **)entry->dominators,
	    strGraphNodePSize(entry->dominators));
	// Remove node from self
	clone = strGraphNodePRemoveIf(clone, node, nodeEqual);

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
				__auto_type find = strGraphNodePSortedFind(i2Doms->dominators, clone[i],
				                                           (gnCmpType)ptrPtrCmp);
				if (find != NULL) {
					/**
					 * Every node dominates itself,so ignore removing node that dominates
					 * itself
					 */
					if (*find == clone[i2])
						continue;

					clone = strGraphNodePRemoveIf(clone, clone[i], nodeEqual);
					changed = 1;
					goto next;
				}
			}
		}
	next:;
	}
	__auto_type len = strGraphNodePSize(clone);
	qsort(clone, len, sizeof(struct __graphNode *), domsLenCmp);

	return clone;
}
struct __graphNode *graphDominatorIdom(const llDominators doms,
                                       struct __graphNode *node) {
	strGraphNodeP idoms __attribute__((cleanup(strGraphNodePDestroy))) =
	    graphDominatorIdoms(doms, node);
	if (strGraphNodePSize(idoms) == 0)
		return NULL;

	return idoms[strGraphNodePSize(idoms) - 1];
}
llDomFrontier graphDominanceFrontiers(struct __graphNode *start,
                                      const llDominators doms) {
	strGraphNodeP allNodes = __graphNodeVisitAll(start);

	llDomFrontier fronts = NULL;
	for (long i = 0; i != strGraphNodePSize(allNodes); i++) {
		struct graphDomFrontier tmp;
		tmp.nodes = NULL;
		tmp.node = allNodes[i];

		fronts = llDomFrontierInsert(fronts, llDomFrontierCreate(tmp),
		                             llDomFrontierCmpInsert);
	}

	for (long b = 0; b != strGraphNodePSize(allNodes); b++) {
		__auto_type preds = __graphNodeIncoming(allNodes[b]);
		if (strGraphEdgePSize(preds) >= 2) {
			for (long p = 0; p != strGraphEdgePSize(preds); p++) {
				__auto_type runner = __graphEdgeIncoming(preds[p]);

				__auto_type idom = graphDominatorIdom(doms, allNodes[b]);
				__auto_type mapped = graphNodeMappingValuePtr(idom);
				__auto_type mappedb = graphNodeMappingValuePtr(allNodes[b]);
				while (runner != graphDominatorIdom(doms, allNodes[b])) {
					// Add b to runners frontier
					__auto_type find = llDomFrontierFindRight(llDomFrontierFirst(fronts),
					                                          runner, llDomFrontierCmp);
					__auto_type value = llDomFrontierValuePtr(find);

					if (!strGraphNodePSortedFind(value->nodes, allNodes[b],
					                             (gnCmpType)ptrPtrCmp)) {
						value->nodes = strGraphNodePSortedInsert(value->nodes, allNodes[b],
						                                         (gnCmpType)ptrPtrCmp);
						printf("RUNNER %i += %i\n", *(int *)__graphNodeValuePtr(runner),
						       *(int *)__graphNodeValuePtr(allNodes[b]));
					}

					// runner = iDom(runner)
					find = llDomFrontierFindRight(llDomFrontierFirst(fronts),
					                              graphDominatorIdom(doms, runner),
					                              llDomFrontierCmp);
					if (find == NULL) {
						runner = NULL;
					} else {
						value = llDomFrontierValuePtr(find);
						runner = value->node;
					}
				}
			}
		}

		strGraphEdgePDestroy(&preds);
	}

	strGraphNodePDestroy(&allNodes);
	return fronts;
}
static char *ptr2Str(const void *a) { return base64Enc((void *)&a, sizeof(a)); }
static void connnectIdoms(mapGraphNode nodes, llDominators valids,
                          llDominators BNode) {
	strGraphNodeP B = llDominatorsValuePtr(BNode)->dominators;

	struct __graphNode *bFirst = *strGraphNodePSortedFind(
	    B, llDominatorsValuePtr(BNode)->node, (gnCmpType)ptrPtrCmp);

	char *str = ptr2Str(bFirst);
	graphNodeMapping bNodeMapped = *mapGraphNodeGet(nodes, str);
	free(str);

	__auto_type idoms = graphDominatorIdoms(valids, bFirst);

	// Connect immediate dominators of B
	for (long i = 0; i != strGraphNodePSize(idoms); i++) {
		__auto_type str = ptr2Str(idoms[i]);
		__auto_type node = mapGraphNodeGet(nodes, str);
		assert(node);
		graphNodeMappingConnect(*node, bNodeMapped, NULL);
	}
}
graphNodeMapping dominatorsTreeCreate(llDominators doms) {
	mapGraphNode map = mapGraphNodeCreate();

	for (__auto_type node = llDominatorsFirst(doms); node != NULL;
	     node = llDominatorsNext(node)) {
		__auto_type node2 = llDominatorsValuePtr(node)->node;
		char *str = ptr2Str(node2);
		mapGraphNodeInsert(map, str, graphNodeMappingCreate(node2, 0));
		free(str);
	}

	// Connect idoms
	for (__auto_type node = llDominatorsFirst(doms); node != NULL;
	     node = llDominatorsNext(node))
		connnectIdoms(map, doms, node);

	// See Below
	long count;
	mapGraphNodeKeys(map, NULL, &count);
	const char *toCheck[count];
	mapGraphNodeKeys(map, (void *)toCheck, NULL);

	// Find the firstNode to have no incoming nodes,this is the master node(of one
	// of the master nodes).
	graphNodeMapping firstNode = NULL;
	for (long i = 0; i != count; i++) {
		__auto_type find = *mapGraphNodeGet(map, toCheck[i]);

		// Check if no incoming
		__auto_type in = graphNodeMappingIncomingNodes(find);
		long len = strGraphNodeMappingPSize(in);
		strGraphNodeMappingPDestroy(&in);
		if (len == 0) {
			firstNode = find;
			break;
		}
	}
	if (count) {
		assert(firstNode);
	}

	mapGraphNodeDestroy(map, NULL);

	return firstNode;
}
