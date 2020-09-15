#include <graph.h>
#include <libdill.h>
#include <linkedList.h>
#include <readersWritersLock.h>
#include <stdbool.h>
#include <stdlib.h>
#include <str.h>
struct __graphNode;
struct __graphEdge {
	struct __graphNode *from;
	struct __graphNode *to;
	unsigned int valuePresent : 1;
};
struct __graphNode {
	struct __ll *outgoing;
	struct __ll *incoming;
	int version;
	struct rwLock *lock;
	unsigned int killable : 1;
};
static int ptrCompare(void *a, void *b) { return a - b; }
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
static coroutine void forwardVisit(struct __graphNode *node,
                                   void (*visit)(struct __graphNode *, void *),
                                   void *data) {
	visit(node, data);
}
static struct __graphEdge *__graphEdgeByDirection(struct __ll *edge,
                                                  enum dir d) {
	if (d == DIR_FORWARD)
		return __llValuePtr(edge);
	else
		return __llValuePtr(*(struct __ll **)__llValuePtr(edge));
}
STR_TYPE_DEF(struct __ll *, LLP);
STR_TYPE_FUNCS(struct __ll *, LLP);
static void __graphNodeVisitDirPred(struct __graphNode *node, void *data,
                                    int(pred)(struct __graphNode *, void *),
                                    void (*visit)(struct __graphNode *, void *),
                                    enum dir d) {
	strGraphNodeP toVisit __attribute__((cleanup(strGraphNodePDestroy)));
	toVisit = NULL;
	strGraphNodeP stack __attribute__((cleanup(strGraphNodePDestroy)));
	stack = NULL;
	strLLP stackIndexes __attribute__((cleanup(strLLPDestroy)));
	stackIndexes = NULL;
	//
	if (!pred(node, data)) {
		return;
	}
	//
	__auto_type connections =
	    (d == DIR_FORWARD) ? node->outgoing : node->incoming;
	if (connections == NULL)
		return;
	// Start at node
	stack = strGraphNodePAppendItem(stack, node);
	stackIndexes = strLLPAppendItem(stackIndexes, __llGetFirst(connections));
	// Add node to toVisit
	toVisit = strGraphNodePAppendItem(toVisit, node);
	//
	while (strGraphNodePSize(stack) != 0) {
		__auto_type topNode = stack[strGraphNodePSize(stack) - 1];
		__auto_type topIndex = stackIndexes[strLLPSize(stackIndexes) - 1];
		__auto_type edge = __graphEdgeByDirection(topIndex, d);
		__auto_type connection = (d == DIR_FORWARD) ? edge->to : edge->from;
		if (pred(connection, data)) {
			__auto_type find = strGraphNodePSortedFind(toVisit, topNode, ptrCompare);
			if (find == NULL) {
				// Push to node-to-visit
				toVisit = strGraphNodePSortedInsert(toVisit, topNode, ptrCompare);
				// Push node and index
				stack = strGraphNodePAppendItem(stack, connection);
				__auto_type connection2 =
				    (d == DIR_FORWARD) ? connection->outgoing : connection->incoming;
				stackIndexes = strLLPAppendItem(
				    stackIndexes, __llFindLeft(connection2, NULL, __llFirstPred));
			}
		} else {
			// Go to next node on top
			__auto_type nextIndex = __llNext(topIndex);
			if (nextIndex == NULL) {
				// Past all nodes,so pop
				stackIndexes = strLLPResize(stackIndexes, strLLPSize(stackIndexes) - 1);
				stack = strGraphNodePResize(stack, strGraphNodePSize(stack) - 1);
			} else {
				// Put next item on top of stack nodes
				stackIndexes[strLLPSize(stackIndexes) - 1] = nextIndex;
			}
		}
	}
	if (visit != NULL) {
		int b = bundle();
		for (int i = 0; i != strGraphNodePSize(toVisit); i++) {
			bundle_go(b, forwardVisit(toVisit[i], visit, data));
		}
		hclose(b);
	}
}
void __graphNodeVisitForward(struct __graphNode *node, void *data,
                             int(pred)(struct __graphNode *, void *),
                             void (*visit)(struct __graphNode *, void *)) {
	__graphNodeVisitDirPred(node, data, pred, visit, DIR_FORWARD);
}
void __graphNodeVisitBackward(struct __graphNode *node, void *data,
                              int(pred)(struct __graphNode *, void *),
                              void (*visit)(struct __graphNode *, void *)) {
	__graphNodeVisitDirPred(node, data, pred, visit, DIR_BACKWARD);
}
static struct __ll *__graphEdgeKillAllPred(struct __ll *ll,
                                           struct __graphNode *from,
                                           struct __graphNode *to, void *data,
                                           int (*pred)(void *, void *),
                                           void (*kill)(void *), enum dir d) {
	if (ll == NULL)
		return NULL;
	ll = __llGetFirst(ll);
	for (__auto_type node = ll; node != NULL;) {
		__auto_type next = __llNext(node);
		struct __graphEdge *edge = __graphEdgeByDirection(node, d);
		__auto_type value = sizeof(struct __graphEdge) + (void *)edge;
		//
		if (to != NULL)
			if (edge->to != to)
				goto endLoop;
		if (from != NULL)
			if (edge->from != from)
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
				if (edge->valuePresent)
					kill(value);
			}
			//
			ll = __llRemoveNode(node);
			__llDestroy(node, kill);
		}
	endLoop:
		node = next;
	}
	return ll;
}
void __graphEdgeKill(struct __graphNode *in, struct __graphNode *out,
                     void *data, int (*pred)(void *, void *),
                     void (*kill)(void *)) {
	// out's incoming's elements point to __graphEdge(which are destroyed when
	// in->outgoing is destroyed below)
	rwWriteStart(out->lock);
	out->incoming = __graphEdgeKillAllPred(out->incoming, in, out, data, pred,
	                                       NULL, DIR_BACKWARD);
	rwWriteEnd(out->lock);
	//
	rwWriteStart(in->lock);
	in->outgoing = __graphEdgeKillAllPred(in->outgoing, in, out, data, pred, kill,
	                                      DIR_FORWARD);
	rwWriteEnd(in->lock);
	//
}
static int alwaysTrue(void *a, void *b) { return 1; }
static void __graphNodeDetach(struct __graphNode *in, struct __graphNode *out,
                              void (*killEdge)(void *)) {
	__graphEdgeKill(in, out, NULL, alwaysTrue, killEdge);
}

static coroutine void __graphNodeKillConnections(struct __graphNode *a,
                                                 struct __graphNode *b,
                                                 void (*killEdge)(void *)) {
	__graphNodeDetach(a, b, killEdge);
	__graphNodeDetach(b, a, killEdge);
}
static int llpredAlwaysTrue(void *a, void *b) { return 0; }
void __graphNodeKill(struct __graphNode *node, void (*killNode)(void *item),
                     void (*killEdge)(void *item)) {
	strGraphEdgeP connectionPtrs = NULL;
	//
	rwWriteStart(node->lock);
	for (int i = 0; i != 2; i++) {
		__auto_type connections = (i == 0) ? node->incoming : node->outgoing;
		for (__auto_type node = __llGetFirst(connections); node != NULL;) {
			__auto_type toKill = __llFindRight(node, NULL, llpredAlwaysTrue);
			strGraphEdgePAppendItem(
			    connectionPtrs,
			    __graphEdgeByDirection(__llValuePtr(toKill),
			                           (i == 0) ? DIR_FORWARD : DIR_BACKWARD));
			node = __llNext(toKill);
			if (toKill == NULL)
				break;
		}
	}
	node->incoming = NULL;
	node->outgoing = NULL;
	rwWriteEnd(node->lock);
	//
	__auto_type b = bundle();
	for (int i = 0; i != strGraphEdgePSize(connectionPtrs); i++) {
		__auto_type connection1 = connectionPtrs[i];
		__auto_type notNode =
		    (connection1->from == node)
		        ? connection1->to
		        : connection1
		              ->from; // If is a self reference,will defualt to self,so
		                      // connections to self will also be removed
		for (__auto_type connection2 = __llGetFirst(notNode->outgoing);
		     connection2 != NULL; connection2 = __llNext(connection2)) {
			bundle_go(b, __graphNodeKillConnections(node, __llValuePtr(connection2),
			                                        killEdge));
			bundle_go(b, __graphNodeKillConnections(__llValuePtr(connection2), node,
			                                        killEdge));
		}
	}
	hclose(b);
	//
	rwLockDestroy(node->lock);
	//
	strGraphEdgePDestroy(&connectionPtrs);
	if (killNode != NULL)
		killNode(node + sizeof(struct __graphNode));
	free(node);
}
static int __graphAllPred(struct __graphNode *node, void *data) { return 1; }
static void __graphVisitAppend(struct __graphNode *node, void *data) {
	strGraphNodeP *allNodes = data;
	*allNodes = strGraphNodePSortedInsert(*allNodes, node, ptrCompare);
}
void __graphKillAll(struct __graphNode *start, void (*killFunc)(void *),
                    void (*killEdge)(void *)) {
	strGraphNodeP allNodesForward __attribute__((cleanup(strGraphNodePDestroy)));
	allNodesForward = NULL;
	__graphNodeVisitForward(start, allNodesForward, __graphAllPred,
	                        __graphVisitAppend);
	for (int i = 0; i != strGraphNodePSize(allNodesForward); i++) {
		__graphNodeKill(allNodesForward[i], killFunc, killEdge);
	}
	//
	strGraphNodeP allNodesBackward __attribute__((cleanup(strGraphNodePDestroy)));
	allNodesBackward = NULL;
	__graphNodeVisitBackward(start, allNodesBackward, __graphAllPred,
	                         __graphVisitAppend);
	for (int i = 0; i != strGraphNodePSize(allNodesBackward); i++) {
		__graphNodeKill(allNodesBackward[i], killFunc, killEdge);
	}
}
void __graphNodeConnect(struct __graphNode *a, struct __graphNode *b,
                        void *data, long itemSize) {
	struct __graphEdge newEdge;
	newEdge.from = a;
	newEdge.to = b;
	newEdge.valuePresent = data != NULL;
	//
	struct __ll *newEdgeLL = NULL;
	struct __ll *newEdgeLL2 = NULL;
	if (data == NULL) {
		newEdgeLL = __llCreate(&newEdge, sizeof(struct __graphEdge));
	} else {
		char buffer[itemSize + sizeof(struct __graphEdge)];
		memcpy(buffer, &newEdge, sizeof(struct __graphEdge));
		memcpy(buffer + sizeof(struct __graphEdge), data, itemSize);
		newEdgeLL = __llCreate(buffer, itemSize + sizeof(struct __graphEdge));
	}
	newEdgeLL2 = __llCreate(&newEdgeLL, sizeof(struct __ll *));
	rwWriteStart(a->lock);
	a->outgoing = __llInsert(a->outgoing, newEdgeLL, ptrCompare);
	rwWriteEnd(a->lock);
	//
	rwWriteStart(b->lock);
	b->incoming = __llInsert(b->incoming, newEdgeLL2, ptrCompare);
	rwWriteEnd(b->lock);
}
void __graphNodeReadLock(struct __graphNode *node) { rwReadStart(node->lock); }
void __graphNodeReadUnlock(struct __graphNode *node) { rwReadEnd(node->lock); }
void __graphNodeWriteLock(struct __graphNode *node) {
	rwWriteStart(node->lock);
}
void __graphNodeWriteUnlock(struct __graphNode *node) {
	rwWriteEnd(node->lock);
}
strGraphEdgeP __graphNodeOutgoing(struct __graphNode *node) {
	strGraphEdgeP retVal = NULL;
	for (__auto_type node2 = __llGetFirst(node->outgoing); node2 != NULL;
	     node2 = __llNext(node2)) {
		retVal = strGraphEdgePAppendItem(retVal, __llValuePtr(node2));
	}
	return retVal;
}
strGraphEdgeP __graphNodeIncoming(struct __graphNode *node) {
	strGraphEdgeP retVal = NULL;
	for (__auto_type node2 = __llGetFirst(node->incoming); node2 != NULL;
	     node2 = __llNext(node2)) {
		retVal = strGraphEdgePAppendItem(
		    retVal, __llValuePtr(*(struct __ll **)__llValuePtr(node2)));
	}
	return retVal;
}
struct __graphNode *__graphEdgeOutgoing(struct __graphEdge *edge) {
	return edge->to;
}
struct __graphNode *__graphEdgeIncoming(struct __graphEdge *edge) {
	return edge->from;
}
void *__graphEdgeValuePtr(struct __graphEdge *edge) {
	if (!edge->valuePresent)
		return NULL;
	return sizeof(struct __graphEdge) + (void *)edge;
}
