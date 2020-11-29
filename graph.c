#include <base64.h>
#include <graph.h>
#include <hashTable.h>
#include <stdbool.h>
#include <stdlib.h>
#include <str.h>
#include <escaper.h>
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
		long itemSize;
		unsigned int valuePresent : 1;
};

struct __graphNode {
		strGraphEdgeP incoming;
		strGraphEdgeP outgoing;
		long itemSize;
		int version;
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
	retVal->itemSize=itemSize;
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

	__graphEdgeKillAllPred(in, out, data, pred, kill);
	__graphEdgeKillAllPred(out, in, data, pred, kill);
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
	for (int i = 0; i != 2; i++) {
		__auto_type connections = (i == 0) ? node->outgoing : node->incoming;
		connectionPtrs = strGraphEdgePSetUnion(connectionPtrs, connections,
		                                       (geCmpType)ptrCompare);
	}
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
	newEdgeNode->itemSize=itemSize;

	a->outgoing = strGraphEdgePSortedInsert(a->outgoing, newEdgeNode,
	                                        (geCmpType)ptrCompare);
	//
	b->incoming = strGraphEdgePSortedInsert(b->incoming, newEdgeNode,
	                                        (geCmpType)ptrCompare);
	return newEdgeNode;
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
graphNodeMapping __createGraphMap(const struct __graphNode *start,strGraphNodeP nodes, int preserveConnections) {
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

		__auto_type out = __graphNodeOutgoing(nodes[i]);
		__auto_type in = __graphNodeIncoming(nodes[i]);

		// Connect outgoing
		for (long i = 0; i != strGraphEdgePSize(out); i++) {
			char *key = ptr2Str(out[i]->to);
			__auto_type find = *mapGraphNodeGet(map, key);

			// If not preserve connections,ignore multiple connections
			if (!preserveConnections)
				if (__graphIsConnectedTo(current, find))
					continue;

			if (find) {
				__graphNodeConnect(current, find, &out[i], sizeof(out[i]));
				free(key);
			}
		}
	}

	char *key=ptr2Str(start);
	__auto_type retVal = *mapGraphNodeGet(map, key);
	free(key);
	
	for (long i = 0; i != strGraphNodePSize(nodes); i++)
		free(keys[i]);
	mapGraphNodeDestroy(map, NULL);

	return retVal;
}
graphNodeMapping createGraphMap(strGraphNodeP nodes, int preserveConnections) {
		return __createGraphMap(nodes[0], nodes, preserveConnections);
};
graphNodeMapping graphNodeCreateMapping(const struct __graphNode *node ,int preserveConnections) {
		__auto_type allNodes=__graphNodeVisitAll(node);
		return __createGraphMap(node, allNodes, preserveConnections);
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
void graphPrint(struct __graphNode *node, char *(*toStr)(struct __graphNode *),
                char *(*toStrEdge)(struct __graphEdge *)) {
	__auto_type allNodes = __graphNodeVisitAll(node);
	for (long i = 0; i != strGraphNodePSize(allNodes); i++) {
		__auto_type outNodes = __graphNodeOutgoing(allNodes[i]);

		char *cur = toStr(allNodes[i]);
		printf("%s:\n", cur);
		for (long i2 = 0; i2 != strGraphEdgePSize(outNodes); i2++) {
			char *out = toStr(outNodes[i2]->to);
			char *edgeStr = toStrEdge(outNodes[i2]);
			printf("    %s(%s)\n", out,edgeStr);
			free(out);
		}
		// In
		__auto_type inNodes = __graphNodeOutgoingNodes(allNodes[i]);

		free(cur);
		strGraphNodePDestroy(&inNodes);
		strGraphEdgePDestroy(&outNodes);
	}
	strGraphNodePDestroy(&allNodes);
}
void graphReplaceWithNode(strGraphNodeP toReplace,
                          struct __graphNode *replaceWith,
                          int (*edgeCmp)(const struct __graphEdge *,
                                         const struct __graphEdge *),
                          void (*killNodeData)(void *), long edgeSize) {
	strGraphEdgeP allIncoming = NULL;
	strGraphEdgeP allOutgoing = NULL;

	strGraphNodeP visitedIncoming = NULL;
	strGraphNodeP visitedOutgoing = NULL;

	// Incoming connnections not connection to items in toReplace
	for (long i = 0; i != strGraphNodePSize(toReplace); i++) {
		for (long i2 = 0; i2 != strGraphEdgePSize(toReplace[i]->incoming); i2++) {
			__auto_type from = toReplace[i]->incoming[i2]->from;

			// Ignore incoming connections to internal items of blob(toReplace)
			if (NULL !=
			    strGraphNodePSortedFind(toReplace, from, (gnCmpType)ptrCompare))
				continue;

			// Edge for repeat edges ,if repeat edgew dont connect
			int existingEdge = 1;
			if (edgeCmp)
				existingEdge = NULL != strGraphEdgePSortedFind(
				                           allIncoming, toReplace[i]->incoming[i2],
				                           (geCmpType)edgeCmp);
			else
				// Check if exisiting connection
				existingEdge = NULL != strGraphNodePSortedFind(visitedIncoming, from,
				                                               (gnCmpType)ptrCompare);

			if (existingEdge)
				continue;

			allIncoming = strGraphEdgePSortedInsert(
			    allIncoming, toReplace[i]->incoming[i2], (geCmpType)ptrCompare);
			visitedIncoming = strGraphNodePSortedInsert(visitedIncoming, from,
			                                            (gnCmpType)ptrCompare);
		}
	}

	// Outgoing connnections not connection to items in toReplace
	for (long i = 0; i != strGraphNodePSize(toReplace); i++) {
		for (long i2 = 0; i2 != strGraphEdgePSize(toReplace[i]->outgoing); i2++) {
			__auto_type to = toReplace[i]->outgoing[i2]->to;

			// Ignore incoming connections to internal items of blob(toReplace)
			if (NULL != strGraphNodePSortedFind(toReplace, to, (gnCmpType)ptrCompare))
				continue;

			// Edge for repeat edges ,if repeat edgew dont connect
			int existingEdge = 1;
			if (edgeCmp)
				existingEdge = NULL != strGraphEdgePSortedFind(
				                           allOutgoing, toReplace[i]->outgoing[i2],
				                           (geCmpType)edgeCmp);
			else
				// Check if exisiting connection
				existingEdge = NULL != strGraphNodePSortedFind(visitedOutgoing, to,
				                                               (gnCmpType)ptrCompare);

			if (existingEdge)
				continue;

			allOutgoing = strGraphEdgePSortedInsert(
			    allOutgoing, toReplace[i]->outgoing[i2], (geCmpType)ptrCompare);
			visitedOutgoing =
			    strGraphNodePSortedInsert(visitedOutgoing, to, (gnCmpType)ptrCompare);
		}
	}

	// Connect incoming to replaceWith
	for (long i = 0; i != strGraphEdgePSize(allIncoming); i++) {
		if (allIncoming[i]->valuePresent) {
			__graphNodeConnect(allIncoming[i]->from, replaceWith,
			                   __graphEdgeValuePtr(allIncoming[i]), edgeSize);
		} else {
			__graphNodeConnect(allIncoming[i]->from, replaceWith, NULL, 0);
		}
	}

	// Connect outgoing to replaceWith
	for (long i = 0; i != strGraphEdgePSize(allOutgoing); i++) {
		if (allOutgoing[i]->valuePresent) {
			__graphNodeConnect(replaceWith, allOutgoing[i]->to,
			                   __graphEdgeValuePtr(allOutgoing[i]), edgeSize);
		} else {
			__graphNodeConnect(replaceWith, allOutgoing[i]->to, NULL, 0);
		}
	}

	strGraphNodePDestroy(&visitedIncoming);
	strGraphNodePDestroy(&visitedOutgoing);
	strGraphEdgePDestroy(&allIncoming);
	strGraphEdgePDestroy(&allOutgoing);

	// Kill all nodes to replace
	for (long i = 0; i != strGraphNodePSize(toReplace); i++)
		__graphNodeKill(toReplace[i], killNodeData, NULL);
}
STR_TYPE_DEF(char, Char);
STR_TYPE_FUNCS(char, Char);
static strChar ptr2GraphVizName(const void *a) {
	__auto_type name = ptr2Str(a);
	const char *format = "%s";
	long len = snprintf(NULL, 0, format, name);
	char buffer[len + 1];
	sprintf(buffer, format, name);

	free(name);

	return strCharAppendData(NULL, buffer, strlen(buffer) + 1);
}
static char *strClone(const char *text) {
	char *retVal = malloc(strlen(text) + 1);
	strcpy(retVal, text);

	return retVal;
}
struct graphVizEdge {
	//
	// These are const char *s,they point to keys from map
	//
	const char *inNodeName;
	const char *outNodeName;
	mapGraphVizAttr attrs;
};

LL_TYPE_DEF(struct graphVizEdge, GVEdge);
LL_TYPE_FUNCS(struct graphVizEdge, GVEdge);
static int llGVEdgeCmp(const struct graphVizEdge *ed,
                       const struct graphVizEdge *node) {
	__auto_type newV = ed;
	__auto_type nodeV = node;

	int res = ptrCompare(&newV->inNodeName, &nodeV->inNodeName);
	if (res != 0)
		return res;

	res = ptrCompare(&newV->outNodeName, &nodeV->outNodeName);
	if (res != 0)
		return res;

	return 0;
}
static char *stringify(const char *text) {
		char *escaped=escapeString((char*)text);
		long size=snprintf(NULL, 0, "\"%s\"", escaped);
		char buffer[size+1];
		sprintf(buffer, "\"%s\"", escaped);

		free(escaped);

		return strClone(buffer);
}
static void writeOutGVAttrs(FILE *dumpTo, mapGraphVizAttr attrs) {
	// write out attributes(if exist)
	long count;
	mapGraphVizAttrKeys(attrs, NULL, &count);

	if (count) {
		fprintf(dumpTo, "[");

		// Get attr names
		const char *attrNames[count];
		mapGraphVizAttrKeys(attrs, attrNames, NULL);

		for (long i2 = 0; i2 != count; i2++) {
				// Print terminating ";" for previous item (if has a first item)
			if (i2 != 0)
				fprintf(dumpTo, ";");

			__auto_type attrValue=stringify(*mapGraphVizAttrGet(attrs, attrNames[i2]));
			fprintf(dumpTo, "%s=%s", attrNames[i2],
			        attrValue);
			free(attrValue);
		}

		fprintf(dumpTo, "]");
	}
}
struct GraphVizNode {
	strChar name;
	mapGraphVizAttr attrs;
};
MAP_TYPE_DEF(struct GraphVizNode, GVNode);
MAP_TYPE_FUNCS(struct GraphVizNode, GVNode);
struct GVDepthPair {
		graphNodeMapping node;
		int depth;
};
MAP_TYPE_DEF(struct GVDepthPair,DepthPair);
MAP_TYPE_FUNCS(struct GVDepthPair,DepthPair);
static int GVDepthPairSort(const void *a,const void *b) {
		const struct GVDepthPair *A=a,*B=b;
		return A->depth-B->depth;
}
static strGraphNodeP graphVizRankSort(const struct __graphNode *enter,strGraphNodeP allNodes) {
		strGraphNodeP visited=NULL;
		mapDepthPair depthByNode=mapDepthPairCreate();

		strGraphNodeP current=strGraphNodePAppendItem(NULL, (struct __graphNode*)enter);
		for(int depth=0;;depth++) {
				strGraphNodeP next=NULL;
				
				//Find unvisited outgoing nodes of currents
				for(long i=0;i!=strGraphNodePSize(current);i++) {
						//Register node at depth
						__auto_type key=ptr2Str(current[i]);
						struct GVDepthPair pair;
						pair.depth=depth;
						pair.node=current[i];
						mapDepthPairInsert(depthByNode, key, pair);
						free(key);
						
						//Add current to visited
						visited=strGraphNodePSortedInsert(visited, current[i], (gnCmpType)ptrCompare);
						
						//Filter out visited nodes
						__auto_type outs=__graphNodeOutgoingNodes(current[i]);
						outs=strGraphNodePSetDifference(outs, visited, (gnCmpType)ptrCompare);

						//Add outs to next;
						next=strGraphNodePSetUnion(next, outs, (gnCmpType)ptrCompare);
				}

				strGraphNodePDestroy(&current);
				current=next;

				if(strGraphNodePSize(current)==0)
						break;
		}
		
		strGraphNodePDestroy(&current);

		//All unreachable nodes should be 0
		__auto_type allNodes2=strGraphNodePClone(allNodes);
		allNodes2=strGraphNodePSetDifference(allNodes2, visited, (gnCmpType)ptrCompare);
		for(long i=0;i!=strGraphNodePSize(allNodes2);i++) {
				struct GVDepthPair pair;
						pair.depth=0;
						pair.node=allNodes2[i];
						__auto_type key=ptr2Str(allNodes2[i]);
						mapDepthPairInsert(depthByNode, key, pair);
						free(key);
		}
		
		long kCount;
		mapDepthPairKeys(depthByNode, NULL, &kCount);
		const char *keys[kCount];
		mapDepthPairKeys(depthByNode, keys, NULL);

		//Fill up buffer with pairs,then we will sor them
		struct GVDepthPair buffer[kCount];
		for(long i=0;i!=kCount;i++)
				buffer[i]=*mapDepthPairGet(depthByNode, keys[i]);

		//Sort by depth
		qsort(buffer, kCount, sizeof(*buffer), GVDepthPairSort);

		mapDepthPairDestroy(depthByNode, NULL);

		

		//Copy to retVal
		strGraphNodeP retVal=strGraphNodePResize(NULL, kCount);
		for(long i=0;i!=kCount;i++)
				retVal[i]=buffer[i].node;

		return retVal;
}
void graph2GraphViz(FILE *dumpTo, graphNodeMapping graph, const char *title,
                    char *(*nodeToLabel)(const struct __graphNode *node,
                                         mapGraphVizAttr *attrs,
                                         const void *data),
                    char *(*edgeToLabel)(const struct __graphEdge *node,
                                         mapGraphVizAttr *attrs,
                                         const void *data),
                    const void *nodeData, const void *edgeData) {
	__auto_type allNodes = __graphNodeVisitAll(graph);
	__auto_type allNodes2=graphVizRankSort(graph,allNodes);
	strGraphNodePDestroy(&allNodes);
	allNodes=allNodes2;
	//
	// Make nodes
	//
	mapGVNode gvNodesMap = mapGVNodeCreate();
	for (long i = 0; i != strGraphNodePSize(allNodes); i++) {
		// Create node
		struct GraphVizNode node;
		node.name = ptr2GraphVizName(allNodes[i]);
		node.attrs = mapGraphVizAttrCreate();

		// Compute attributes and name
		char *name = NULL;
		if (nodeToLabel)
			name = nodeToLabel(allNodes[i], &node.attrs, nodeData);

		// Assign label if not provided
		if (!mapGraphVizAttrGet(node.attrs, "label")) {
				if(name)
						mapGraphVizAttrInsert(node.attrs, "label", strClone(name));
				else 
						mapGraphVizAttrInsert(node.attrs, "label", strClone(node.name));
		}

		// Insert
		char* key=ptr2Str(allNodes[i]);
		mapGVNodeInsert(gvNodesMap, key, node);
		free(key);
		free(name);
	}
	//
	// Edges
	//

	llGVEdge gvEdges = NULL;

	strGraphEdgeMappingP visitedEdges = NULL;
	for (long i = 0; i != strGraphNodePSize(allNodes); i++) {
		strGraphEdgeMappingP edges = graphNodeMappingOutgoing(allNodes[i]);

		// Dont re-visit visited eges
		edges =
		    strGraphEdgePSetDifference(edges, visitedEdges, (geCmpType)ptrCompare);
		visitedEdges=strGraphEdgePSetUnion(visitedEdges, edges, (geCmpType)ptrCompare);

		for (long i2 = 0; i2 != strGraphEdgeMappingPSize(edges); i2++) {
			// find graphviz versions of nodes coming in
			char *key = ptr2GraphVizName(edges[i2]->from);
			__auto_type inPtr = mapGVNodeGet(gvNodesMap, key);
			strCharDestroy(&key);
			key = ptr2GraphVizName(edges[i2]->to);
			__auto_type outPtr = mapGVNodeGet(gvNodesMap, key);
			strCharDestroy(&key);

			// Create edge
			struct graphVizEdge edge;
			edge.attrs = mapGraphVizAttrCreate();
			edge.inNodeName = mapGVNodeValueKey(inPtr);
			edge.outNodeName = mapGVNodeValueKey(outPtr);

			// Assign attribute and name from predicate
			char *name = NULL;
			if (edgeToLabel) {
				name = edgeToLabel(edges[i2], &edge.attrs, edgeData);
			}

			// Assign name to label if not already defined
			if (name)
				mapGraphVizAttrInsert(edge.attrs, "label", strClone(name));

			__auto_type newNode = llGVEdgeCreate(edge);
			llGVEdgeInsert(gvEdges, newNode, llGVEdgeCmp);
			gvEdges = newNode;

			free(name);
		}
	}

	//
	// Dump to file
	//

	fprintf(dumpTo, "digraph %s {\n", title);

	// Nodes in sorted order
	{
		// Get keys
		long kCount;
		mapGVNodeKeys(gvNodesMap, NULL, &kCount);
		const char *keys[kCount];
		mapGVNodeKeys(gvNodesMap, keys, NULL);

		// Write out nodes IN ORDER OF SORT
		for (long i = 0; i != strGraphNodePSize(allNodes); i++) {
				__auto_type key=ptr2Str(allNodes[i]);
				
				__auto_type find = *mapGVNodeGet(gvNodesMap, key);
				free(key);
				fprintf(dumpTo, "    \"%s\"", find.name);

			writeOutGVAttrs(dumpTo, find.attrs);

			fprintf(dumpTo, ";\n");
		}
	}

	// Write out edges
	for (__auto_type edge = llGVEdgeFirst(gvEdges); edge != NULL;
	     edge = llGVEdgeNext(edge)) {
		__auto_type edgeValue = llGVEdgeValuePtr(edge);
		// Write out nodes
		fprintf(dumpTo, "    \"%s\"->\"\%s\"", edgeValue->inNodeName,
		        edgeValue->outNodeName);

		writeOutGVAttrs(dumpTo, edgeValue->attrs);

		fprintf(dumpTo, ";\n");
	}

	fprintf(dumpTo, "}");
}
long graphNodeValueSize(const struct __graphNode *node) {
		return node->itemSize;
}
long graphEdgeValueSize(const struct __graphEdge *edge) {
		return edge->itemSize;
} 
