#include <base64.h>
#include <graph.h>
#include <hashTable.h>
#include <readersWritersLock.h>
#include <stdbool.h>
#include <stdlib.h>
#include <str.h>
#define DEBUG_PRINT_ENABLE 1
#include <debugPrint.h>
typedef int (*geCmpType)(const struct __graphEdge **,
                         const struct __graphEdge **);
typedef int (*gnCmpType)(const struct __graphNode **,
                         const struct __graphNode **);
struct __graphNode;
struct __graphEdge {
	struct __graphNode *from;
	struct __graphNode *to;
	unsigned int valuePresent : 1;
};

struct __graphNode {
	strGraphEdgeP incoming;
	strGraphEdgeP outgoing;
	int version;
	struct rwLock *lock;
	unsigned int killable : 1;
};
static int ptrCompare(const void *a, const void *b) {
	const void **A = (const void **)a, **B = (const void **)b;
	return *A - *B;
}
struct __graphNode *__graphNodeCreate(void *value, long itemSize, int version) {
	struct __graphNode *retVal = malloc(sizeof(struct __graphNode) + itemSize);
	memcpy((void *)retVal + sizeof(struct __graphNode), value, itemSize);
	retVal->incoming = NULL;
	retVal->outgoing = NULL;
	retVal->version = version;
	retVal->lock = rwLockCreate();
	return retVal;
}
enum dir { DIR_FORWARD, DIR_BACKWARD };
static void forwardVisit(struct __graphNode *node,
                         void (*visit)(struct __graphNode *, void *),
                         void *data) {
	visit(node, data);
}
static strGraphEdgeP __graphEdgeByDirection(const struct __graphNode *node,
                                            enum dir d) {
	__auto_type vec = (d == DIR_FORWARD) ? node->outgoing : node->incoming;
	return vec;
}
STR_TYPE_DEF(long, Long);
STR_TYPE_FUNCS(long, Long);
static void __graphNodeVisitDirPred(struct __graphNode *node, void *data,
                                    int(pred)(const struct __graphNode *,
                                              const struct __graphEdge *,
                                              const void *),
                                    void (*visit)(struct __graphNode *, void *),
                                    enum dir d) {
	strGraphNodeP visited __attribute__((cleanup(strGraphNodePDestroy)));
	visited = NULL;
	strGraphNodeP toVisit __attribute__((cleanup(strGraphNodePDestroy)));
	toVisit = NULL;
	strGraphNodeP stack __attribute__((cleanup(strGraphNodePDestroy)));
	stack = NULL;
	strLong stackIndexes __attribute__((cleanup(strLongDestroy)));
	stackIndexes = NULL;
	//
	__auto_type connections =
	    (d == DIR_FORWARD) ? node->outgoing : node->incoming;
	if (connections == NULL)
		return;
	// Start at node
	stack = strGraphNodePAppendItem(stack, node);
	stackIndexes = strLongAppendItem(stackIndexes, 0);
	//
	while (strGraphNodePSize(stack) != 0) {
		__auto_type topNode = stack[strGraphNodePSize(stack) - 1];
		__auto_type topIndex = stackIndexes[strLongSize(stackIndexes) - 1];

		__auto_type edges = __graphEdgeByDirection(topNode, d);
		if (topIndex == strGraphEdgePSize(edges))
			goto next;

		__auto_type connection =
		    (d == DIR_FORWARD) ? edges[topIndex]->to : edges[topIndex]->from;
		//
		int cond = 1;
		if (pred != NULL)
			cond = pred(connection, edges[topIndex], data);
		//
		if (cond) {
			__auto_type find =
			    strGraphNodePSortedFind(visited, connection, (gnCmpType)ptrCompare);
			if (find == NULL) {
				visited = strGraphNodePSortedInsert(visited, connection,
				                                    (gnCmpType)ptrCompare);
				// Push to node-to-visit
				toVisit = strGraphNodePSortedInsert(toVisit, connection,
				                                    (gnCmpType)ptrCompare);
				// Push node and index
				stack = strGraphNodePAppendItem(stack, connection);
				__auto_type connection2 =
				    (d == DIR_FORWARD) ? connection->outgoing : connection->incoming;
				// Push new index
				stackIndexes = strLongAppendItem(stackIndexes, 0);
				if (connection2 == NULL)
					goto next;
			} else
				goto next;
		} else {
			long nextIndex;
		next:;
			nextIndex = stackIndexes[strLongSize(stackIndexes) - 1] + 1;
			// Go to next node on top
			__auto_type top = stack[strGraphNodePSize(stack) - 1];
			__auto_type edges = (d == DIR_FORWARD) ? top->outgoing : top->incoming;
			if (nextIndex >= strGraphEdgePSize(edges)) {
				// Past all nodes,so pop
				stackIndexes = strLongPop(stackIndexes, NULL);
				stack = strGraphNodePPop(stack, NULL);
				if (strGraphNodePSize(stack) == 0 || strLongSize(stackIndexes) == 0)
					break;

				//
				// Go to next item,goto next to check if at top of "new" stack
				//
				stackIndexes[strLongSize(stackIndexes) - 1]++;
				continue;
			} else
				stackIndexes[strLongSize(stackIndexes) - 1]++;
		}
	}
	if (visit != NULL) {
		for (int i = 0; i != strGraphNodePSize(toVisit); i++) {
			visit(toVisit[i], data);
		}
	}
}
void __graphNodeVisitForward(struct __graphNode *node, void *data,
                             int(pred)(const struct __graphNode *,
                                       const struct __graphEdge *,
                                       const void *),
                             void (*visit)(struct __graphNode *, void *)) {
	__graphNodeVisitDirPred(node, data, pred, visit, DIR_FORWARD);
}
void __graphNodeVisitBackward(struct __graphNode *node, void *data,
                              int(pred)(const struct __graphNode *,
                                        const struct __graphEdge *,
                                        const void *),
                              void (*visit)(struct __graphNode *, void *)) {
	__graphNodeVisitDirPred(node, data, pred, visit, DIR_BACKWARD);
}
static int edgeToPred(const void *a, const struct __graphEdge **b) {
	const struct __graphEdge *B = *(void **)b;
	const struct __graphNode *A = a;
	return B->to == A;
}
static int edgeFromPred(const void *a, const struct __graphEdge **b) {
	const struct __graphEdge *B = *b;
	const struct __graphNode *A = a;
	return B->from == A;
}
static void __graphEdgeKillAllPred(struct __graphNode *from,
                                   struct __graphNode *to, void *data,
                                   int (*pred)(void *, void *),
                                   void (*kill)(void *)) {
	__auto_type out = from->outgoing;
	for (long i = 0; i != strGraphEdgePSize(out); i++) {
		__auto_type edges = __graphEdgeByDirection(from, DIR_FORWARD);
		__auto_type value = __graphEdgeValuePtr(out[i]);
		//
		if (to != NULL)
			if (out[i]->to != to)
				goto endLoop;
		if (from != NULL)
			if (out[i]->from != from)
				goto endLoop;
		//
		int kill2 = 0;
		if (kill == NULL) {
			kill2 = 1;
		} else {
			kill2 = pred(data, value);
		}
		if (kill2) {
			//
			if (kill != NULL) {
				if (out[i]->valuePresent)
					kill(value);
			}
			//
		}
	endLoop:;
	}

	from->outgoing = strGraphEdgePRemoveIf(from->outgoing, to, edgeToPred);
	to->incoming = strGraphEdgePRemoveIf(to->incoming, from, edgeFromPred);
}
void __graphEdgeKill(struct __graphNode *in, struct __graphNode *out,
                     void *data, int (*pred)(void *, void *),
                     void (*kill)(void *)) {
	// out's incoming's elements point to __graphEdge(which are destroyed when
	// in->outgoing is destroyed below)

	rwWriteStart(in->lock);
	rwWriteStart(out->lock);
	__graphEdgeKillAllPred(in, out, data, pred, kill);
	__graphEdgeKillAllPred(out, in, data, pred, kill);
	rwWriteEnd(in->lock);
	rwWriteEnd(out->lock);

	//

	//
}
static int alwaysTrue(void *a, void *b) { return 1; }
static void __graphNodeDetach(struct __graphNode *in, struct __graphNode *out,
                              void (*killEdge)(void *)) {
	__graphEdgeKill(in, out, NULL, alwaysTrue, killEdge);
}

static void __graphNodeKillConnections(struct __graphNode *a,
                                       struct __graphNode *b,
                                       void (*killEdge)(void *)) {
	__graphNodeDetach(a, b, killEdge);
	__graphNodeDetach(b, a, killEdge);
}
static int llpredAlwaysTrue(const void *a, const void *b) { return 0; }
void __graphNodeKill(struct __graphNode *node, void (*killNode)(void *item),
                     void (*killEdge)(void *item)) {
	strGraphEdgeP connectionPtrs = NULL;
	//
	rwReadStart(node->lock);
	for (int i = 0; i != 2; i++) {
		__auto_type connections = (i == 0) ? node->outgoing : node->incoming;
		connectionPtrs = strGraphEdgePSetUnion(connectionPtrs, connections,
		                                       (geCmpType)ptrCompare);
	}
	rwReadEnd(node->lock);
	//
	for (int i = 0; i != strGraphEdgePSize(connectionPtrs); i++) {
		__auto_type connection1 = connectionPtrs[i];
		__auto_type notNode =
		    (connection1->from == node)
		        ? connection1->to
		        : connection1
		              ->from; // If is a self reference,will defualt to self,so
		                      // connections to self will also be removed
		__graphNodeKillConnections(node, notNode, killEdge);
		__graphNodeKillConnections(notNode, node, killEdge);
	}
	//

	node->incoming = NULL;
	node->outgoing = NULL;
	rwLockDestroy(node->lock);
	//
	strGraphEdgePDestroy(&connectionPtrs);
	if (killNode != NULL)
		killNode(node + sizeof(struct __graphNode));
	free(node);
}
static int __graphPredNotVisited(const struct __graphNode *node,
                                 const struct __graphEdge *edge,
                                 const void *data) {
	const strGraphNodeP *visited = data;
	return NULL == strGraphNodePSortedFind(*visited, node, (gnCmpType)ptrCompare);
}

static void __graphVisitAppend(struct __graphNode *node, void *data) {
	strGraphNodeP *allNodes = data;
	*allNodes = strGraphNodePSortedInsert(*allNodes, node, (gnCmpType)ptrCompare);
}
strGraphNodeP __graphNodeVisitAll(const struct __graphNode *start) {
	if (!start)
		return NULL;

	strGraphNodeP visited = strGraphNodePAppendItem(NULL, (void *)start);

loop:;
	long oldSize = strGraphNodePSize(visited);
	// Dont use visited as for loop as it will be modified use clone
	__auto_type clone = strGraphNodePAppendData(NULL, (void *)visited,
	                                            strGraphNodePSize(visited));
	for (long i = 0; i != oldSize; i++) {
		// Visit forward
		__graphNodeVisitForward((struct __graphNode *)clone[i], &visited,
		                        __graphPredNotVisited, __graphVisitAppend);

		// Visit backward
		__graphNodeVisitBackward((struct __graphNode *)clone[i], &visited,
		                         __graphPredNotVisited, __graphVisitAppend);
	}

	strGraphNodePDestroy(&clone);
	// If added new node(s), re-run
	__auto_type newSize = strGraphNodePSize(visited);
	if (newSize != oldSize)
		goto loop;

	return visited;
}
void __graphKillAll(struct __graphNode *start, void (*killFunc)(void *),
                    void (*killEdge)(void *)) {
	strGraphNodeP allNodes = __graphNodeVisitAll(start);
	for (int i = 0; i != strGraphNodePSize(allNodes); i++) {
		__graphNodeKill(allNodes[i], killFunc, killEdge);
	}
	strGraphNodePDestroy(&allNodes);
}
struct __graphEdge *__graphNodeConnect(struct __graphNode *a,
                                       struct __graphNode *b, void *data,
                                       long itemSize) {
	//
	struct __graphEdge *newEdgeNode = NULL;
	if (data == NULL) {
		newEdgeNode = malloc(sizeof(struct __graphEdge));
	} else {
		newEdgeNode = malloc(sizeof(struct __graphEdge) + itemSize);
		memcpy(newEdgeNode + 1, data, itemSize);
	}

	newEdgeNode->from = a;
	newEdgeNode->to = b;
	newEdgeNode->valuePresent = data != NULL;

	rwWriteStart(a->lock);
	a->outgoing = strGraphEdgePSortedInsert(a->outgoing, newEdgeNode,
	                                        (geCmpType)ptrCompare);
	rwWriteEnd(a->lock);
	//
	rwWriteStart(b->lock);
	b->incoming = strGraphEdgePSortedInsert(b->incoming, newEdgeNode,
	                                        (geCmpType)ptrCompare);
	rwWriteEnd(b->lock);
	return newEdgeNode;
}
void __graphNodeReadLock(struct __graphNode *node) { rwReadStart(node->lock); }
void __graphNodeReadUnlock(struct __graphNode *node) { rwReadEnd(node->lock); }
void __graphNodeWriteLock(struct __graphNode *node) {
	rwWriteStart(node->lock);
}
void __graphNodeWriteUnlock(struct __graphNode *node) {
	rwWriteEnd(node->lock);
}
strGraphEdgeP __graphNodeOutgoing(const struct __graphNode *node) {
	return strGraphEdgePAppendData(NULL, (void *)node->outgoing,
	                               strGraphEdgePSize(node->outgoing));
}
strGraphNodeP __graphNodeOutgoingNodes(const struct __graphNode *node) {
	strGraphNodeP retVal =
	    strGraphNodePResize(NULL, strGraphEdgePSize(node->outgoing));
	for (long i = 0; i != strGraphEdgePSize(node->outgoing); i++)
		retVal[i] = node->outgoing[i]->to;

	return retVal;
}
strGraphNodeP __graphNodeIncomingNodes(const struct __graphNode *node) {
	strGraphNodeP retVal =
	    strGraphNodePResize(NULL, strGraphEdgePSize(node->incoming));
	for (long i = 0; i != strGraphEdgePSize(node->incoming); i++)
		retVal[i] = node->incoming[i]->from;

	return retVal;
	return retVal;
}
strGraphEdgeP __graphNodeIncoming(const struct __graphNode *node) {
	return strGraphEdgePAppendData(NULL, (void *)node->incoming,
	                               strGraphEdgePSize(node->incoming));
}
struct __graphNode *__graphEdgeOutgoing(const struct __graphEdge *edge) {
	return edge->to;
}
struct __graphNode *__graphEdgeIncoming(const struct __graphEdge *edge) {
	return edge->from;
}
void *__graphEdgeValuePtr(const struct __graphEdge *edge) {
	if (!edge->valuePresent)
		return NULL;
	return sizeof(struct __graphEdge) + (void *)edge;
}
void *__graphNodeValuePtr(const struct __graphNode *node) {
	return sizeof(struct __graphNode) + (void *)node;
}
int __graphIsConnectedTo(const struct __graphNode *from,
                         const struct __graphNode *to) {

	for (long i = 0; i != strGraphEdgePSize(from->outgoing); i++) {
		if (from->outgoing[i]->to == to)
			return 1;
	}
	return 0;
}
static char *ptr2Str(const void *a) { return base64Enc((void *)&a, sizeof(a)); }
graphNodeMapping createGraphMap(strGraphNodeP nodes, int preserveConnections) {
	__auto_type map = mapGraphNodeCreate();
	char *keys[strGraphNodePSize(nodes)];
	// Create keys and insert
	for (long i = 0; i != strGraphNodePSize(nodes); i++) {
		keys[i] = ptr2Str(nodes[i]);
		mapGraphNodeInsert(map, keys[i], graphNodeMappingCreate(nodes[i], 0));
	}

	//"Clone" the connections by mapping ptrs to nodes and connecting
	for (long i = 0; i != strGraphNodePSize(nodes); i++) {
		__auto_type current = *mapGraphNodeGet(map, keys[i]);

		__auto_type out = __graphNodeOutgoingNodes(nodes[i]);
		__auto_type in = __graphNodeIncomingNodes(nodes[i]);

		// Connect outgoing
		for (long i = 0; i != strGraphNodePSize(out); i++) {
			char *key = ptr2Str(out[i]);
			__auto_type find = *mapGraphNodeGet(map, key);

			// If not preserve connections,ignore multiple connections
			if (!preserveConnections)
				if (__graphIsConnectedTo(current, find))
					continue;

			if (find) {
				__graphNodeConnect(current, find, &out, sizeof(out[i]));
				free(key);
			}
		}
		// Connect incoming
		for (long i = 0; i != strGraphNodePSize(in); i++) {
			char *key = ptr2Str(in[i]);
			__auto_type find = *mapGraphNodeGet(map, key);

			// If not preserve connections,ignore multiple connections
			if (!preserveConnections)
				if (__graphIsConnectedTo(find, current))
					continue;

			if (find)
				__graphNodeConnect(find, current, &in[i], sizeof(in[i]));
			free(key);
		}
	}

	__auto_type retVal = *mapGraphNodeGet(map, keys[0]);
	for (long i = 0; i != strGraphNodePSize(nodes); i++)
		free(keys[i]);
	mapGraphNodeDestroy(map, NULL);

	return retVal;
}
//
// Does not preserve edges,see filterGraph
//
static void __filterTransparentKill(graphNodeMapping node) {
	__auto_type in = graphNodeMappingIncomingNodes(node);
	__auto_type out = graphNodeMappingOutgoingNodes(node);

	// Connect in to out(if not already connectected)
	for (long inI = 0; inI != strGraphNodeMappingPSize(in); inI++) {
		for (long outI = 0; outI != strGraphNodeMappingPSize(out); outI++) {
			// Check if not connected to
			if (__graphIsConnectedTo(in[inI], out[outI]))
				continue;

			graphNodeMappingConnect(in[inI], out[outI], NULL);
		}
	}

	graphNodeMappingKill(&node, NULL, NULL);
}
#define DEBUG_PRINT_ENABLE 1
graphNodeMapping
createFilteredGraph(struct __graphNode *start, strGraphNodeP nodes, void *data,
                    int (*pred)(void *data, struct __graphNode *)) {
	__auto_type clone = createGraphMap(nodes, 0);
	__auto_type cloneNodes = __graphNodeVisitAll(clone);

	graphNodeMapping retVal = NULL;
	for (long i = 0; i != strGraphNodeMappingPSize(cloneNodes); i++)
		if (!pred(data, *graphNodeMappingValuePtr(cloneNodes[i]))) {
			__auto_type sourceNode = *graphNodeMappingValuePtr(cloneNodes[i]);
			DEBUG_PRINT("Killing node %p\n", sourceNode);
			__filterTransparentKill(cloneNodes[i]); // Takes a mapped node!!!
		} else if (retVal == NULL)
			retVal = cloneNodes[i];
		else if (start == *graphNodeMappingValuePtr(cloneNodes[i]))
			retVal = cloneNodes[i];

	strGraphNodeMappingPDestroy(&cloneNodes);

	return retVal;
}
// https://efficientcodeblog.wordpress.com/2018/02/15/finding-all-paths-between-two-nodes-in-a-graph/
static void __graphAllPathsTo(strGraphEdgeP *currentPath, strGraphPath *paths,
                              const struct __graphNode *from,
                              const struct __graphNode *to) {
	// At destination so return after appending to paths
	if (from == to) {
	push:;
		__auto_type clone = strGraphEdgePAppendData(
		    NULL, (void *)*currentPath, strGraphEdgePSize(*currentPath));
		*paths = strGraphPathAppendItem(*paths, clone);
		return;
	}
	// Push if no outgoing and to is NULL
	if (0 == strGraphEdgePSize(from->outgoing) && to == NULL)
		goto push;

	// Ensure isn't visiting path that has already been visited
	for (long i = 0; i < strGraphEdgePSize(*currentPath); i++) {
		if (currentPath[0][i]->from == from)
			return;
	}

	for (long i = 0; i != strGraphEdgePSize(from->outgoing); i++) {
		// Push
		*currentPath = strGraphEdgePAppendItem(*currentPath, from->outgoing[i]);
		// dfs
		__graphAllPathsTo(currentPath, paths, from->outgoing[i]->to, to);
		// Pop
		*currentPath = strGraphEdgePPop(*currentPath, NULL);
	}
}
strGraphPath graphAllPathsTo(struct __graphNode *from, struct __graphNode *to) {
	strGraphPath paths = NULL;
	strGraphEdgeP currentPath = NULL;

	__graphAllPathsTo(&currentPath, &paths, from, to);

	strGraphEdgePDestroy(&currentPath);
	return paths;
}
void graphPrint(struct __graphNode *node,
                char *(*toStr)(struct __graphNode *)) {
	__auto_type allNodes = __graphNodeVisitAll(node);
	for (long i = 0; i != strGraphNodePSize(allNodes); i++) {
		__auto_type outNodes = __graphNodeOutgoingNodes(allNodes[i]);

		char *cur = toStr(allNodes[i]);
		printf("NODE:%s\n", cur);
		for (long i2 = 0; i2 != strGraphNodePSize(outNodes); i2++) {
			char *out = toStr(outNodes[i2]);
			printf("     %s\n", out);
			free(out);
		}
		// In
		__auto_type inNodes = __graphNodeOutgoingNodes(allNodes[i]);

		free(cur);
		strGraphNodePDestroy(&inNodes);
		strGraphNodePDestroy(&outNodes);
	}
	strGraphNodePDestroy(&allNodes);
}
