#include <libdill.h>
#include <linkedList.h>
#include <readersWritersLock.h>
#include <skinny_mutex.h>
#include <stdbool.h>
#include <stdlib.h>
#include <str.h>
struct __graphNode;
STR_TYPE_DEF(struct __graphNode *, GraphNodeP);
STR_TYPE_FUNCS(struct __graphNode *, GraphNodeP);
struct __graphNode {
	strGraphNodeP outgoing;
	strGraphNodeP incoming;
	int version;
	struct rwLock *lock;
	unsigned int killable : 1;
};
static int ptrCompare(void *a, void *b) {
	return *(struct __graphNode **)a - *(struct __graphNode **)b;
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
STR_TYPE_DEF(int, Int);
STR_TYPE_FUNCS(int, Int);
enum dir { DIR_FORWARD, DIR_BACKWARD };
static coroutine void forwardVisit(struct __graphNode *node,
                                   void (*visit)(struct __graphNode *, void *),
                                   void *data) {
	visit(node, data);
}
static void __graphNodeVisitDirPred(struct __graphNode *node, void *data,
                                    int(pred)(struct __graphNode *, void *),
                                    void (*visit)(struct __graphNode *, void *),
                                    enum dir d) {
	strGraphNodeP toVisit __attribute__((cleanup(strGraphNodePDestroy)));
	toVisit = NULL;
	strGraphNodeP stack __attribute__((cleanup(strGraphNodePDestroy)));
	stack = NULL;
	strInt stackIndexes __attribute__((cleanup(strIntDestroy)));
	stackIndexes = NULL;
	//
	if (!pred(node, data)) {
		return;
	}
	// Start at node
	stack = strGraphNodePAppendItem(stack, node);
	stackIndexes = strIntAppendItem(stackIndexes, 0);
	// Add node to toVisit
	toVisit = strGraphNodePAppendItem(toVisit, node);
	//
	while (strGraphNodePSize(stack) != 0) {
		__auto_type topNode = stack[strGraphNodePSize(stack) - 1];
		__auto_type topIndex = stackIndexes[strIntSize(stackIndexes) - 1];
		__auto_type connections =
		    (d == DIR_FORWARD) ? topNode->outgoing : topNode->incoming;
		if (pred(connections[topIndex], data)) {
			__auto_type find = strGraphNodePSortedFind(toVisit, topNode, ptrCompare);
			if (find == NULL) {
				// Push to node-to-visit
				toVisit = strGraphNodePSortedInsert(toVisit, topNode, ptrCompare);
				// Push node and index
				stack = strGraphNodePAppendItem(stack, connections[topIndex]);
				stackIndexes = strIntAppendItem(stackIndexes, 0);
			}
		} else {
			__auto_type nextIndex = stackIndexes[strIntSize(stackIndexes) - 1]++;
			// Go to next node on top
			if (nextIndex >= strGraphNodePSize(connections)) {
				// Past all nodes,so pop
				stackIndexes = strIntResize(stackIndexes, strIntSize(stackIndexes) - 1);
				stack = strGraphNodePResize(stack, strGraphNodePSize(stack) - 1);
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
void __graphNodeDetach(struct __graphNode *in, struct __graphNode *out) {
	__auto_type inSet = strGraphNodePAppendItem(NULL, in);
	__auto_type outSet = strGraphNodePAppendItem(NULL, out);
	//
	rwWriteStart(in->lock);
	in->outgoing = strGraphNodePSetDifference(in->outgoing, outSet, ptrCompare);
	rwWriteEnd(in->lock);
	//
	rwWriteStart(out->lock);
	out->incoming = strGraphNodePSetDifference(out->incoming, inSet, ptrCompare);
	rwWriteEnd(out->lock);
	//
	strGraphNodePDestroy(&inSet);
	strGraphNodePDestroy(&outSet);
}
static coroutine void __graphNodeKillConnections(struct __graphNode *a,
                                                 struct __graphNode *b) {
	__graphNodeDetach(a, b);
	__graphNodeDetach(b, a);
}
void __graphNodeKill(struct __graphNode *node, void (*killItem)(void *item)) {
	__auto_type self = strGraphNodePAppendItem(NULL, node);
	rwWriteStart(node->lock);
	node->incoming = strGraphNodePSetDifference(node->incoming, self, ptrCompare);
	node->outgoing = strGraphNodePSetDifference(node->outgoing, self, ptrCompare);
	rwWriteEnd(node->lock);
	//
	__auto_type b = bundle();
	for (int i = 0; i != 2; i++) {
		__auto_type connections = (i == 0) ? node->incoming : node->outgoing;
		for (int i = 0; i != strGraphNodePSize(connections); i++) {
			bundle_go(b, __graphNodeKillConnections(node, connections[i]));
		}
	}
	hclose(b);
	//
	rwLockDestroy(node->lock);
	//
	strGraphNodePDestroy(&node->incoming);
	strGraphNodePDestroy(&node->outgoing);
	strGraphNodePDestroy(&self);
	if (killItem != NULL)
		killItem(node + sizeof(struct __graphNode));
	free(node);
}
static int __graph1NieghborPred(struct __graphNode *node, void *data) {
	struct __graphNode *parent = data;
	__auto_type find =
	    strGraphNodePSortedFind(parent->outgoing, node, ptrCompare);
	return (find == NULL) ? 0 : 1;
}
static int __graphAllPred(struct __graphNode *node, void *data) { return 1; }
static void __graphVisitAppend(struct __graphNode *node, void *data) {
	strGraphNodeP *allNodes = data;
	*allNodes = strGraphNodePSortedInsert(*allNodes, node, ptrCompare);
}
void __graphKillAll(struct __graphNode *start, void (*killFunc)(void *)) {
	strGraphNodeP allNodesForward __attribute__((cleanup(strGraphNodePDestroy)));
	allNodesForward = NULL;
	__graphNodeVisitForward(start, allNodesForward, __graphAllPred,
	                        __graphVisitAppend);
	for (int i = 0; i != strGraphNodePSize(allNodesForward); i++) {
		__graphNodeKill(allNodesForward[i], killFunc);
	}
	//
	strGraphNodeP allNodesBackward __attribute__((cleanup(strGraphNodePDestroy)));
	allNodesBackward = NULL;
	__graphNodeVisitBackward(start, allNodesBackward, __graphAllPred,
	                         __graphVisitAppend);
	for (int i = 0; i != strGraphNodePSize(allNodesBackward); i++) {
		__graphNodeKill(allNodesBackward[i], killFunc);
	}
}
void __graphNodeConnect(struct __graphNode *a, struct __graphNode *b) {
	rwWriteStart(a->lock);
	a->outgoing = strGraphNodePSortedInsert(a->outgoing, b, ptrCompare);
	rwWriteEnd(a->lock);
	//
	rwWriteStart(b->lock);
	b->incoming = strGraphNodePSortedInsert(b->incoming, a, ptrCompare);
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
const strGraphNodeP __graphNodeOutgoing(struct __graphNode *node) {
	return node->outgoing;
}
const strGraphNodeP __graphNodeIncoming(struct __graphNode *node) {
	return node->incoming;
}
