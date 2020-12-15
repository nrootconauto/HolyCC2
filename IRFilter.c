#include <IR.h>
#include <cleanup.h>
static int isFlowNode(enum IRConnType type) {
	switch (type) {
	case IR_CONN_FLOW:
	case IR_CONN_COND_FALSE:
	case IR_CONN_COND_TRUE:
		return 1;
	default:
		return 0;
	}
}
static int isFlowBlockOrStartOrEndPred(const struct __graphNode *node,
                                       const void *start) {
	// Check if multiple incoming flows
	strGraphEdgeIRP incoming CLEANUP(strGraphEdgeIRPDestroy) =
	    graphNodeIRIncoming((graphNodeIR)node);
	int flowCount = 0;
	for (long i = 0; i != strGraphEdgeIRPSize(incoming); i++)
		if (isFlowNode(*graphEdgeIRValuePtr(incoming[i])))
			flowCount++;

	if (flowCount > 1)
		return 1;

	// Check if multiple flow out
	strGraphEdgeIRP outgoing CLEANUP(strGraphEdgeIRPDestroy) =
	    graphNodeIROutgoing((graphNodeIR)node);
	flowCount = 0;
	for (long i = 0; i != strGraphEdgeIRPSize(outgoing); i++)
		if (isFlowNode(*graphEdgeIRValuePtr(outgoing[i])))
			flowCount++;
	if (flowCount > 1)
		return 1;

	// Check if start node(data is start)
	if (start == node)
		return 1;

	// Check if end(no outgoing)
	if (strGraphEdgeIRPSize(outgoing) == 0)
		return 1;

	return 0;
}
static void strGraphPathDestroy2(strGraphPath *path) {
	for (long i = 0; i != strGraphPathSize(*path); i++)
		strGraphEdgeIRPDestroy(&path[0][i]);

	strGraphPathDestroy(path);
}
static int ptrPtrCmp(const void *a, const void *b) {
	if (*(void **)a > *(void **)b)
		return 1;
	else if (*(void **)a < *(void **)b)
		return -1;
	else
		return 0;
}
struct assocPairIR2Mapping {
	graphNodeIR ir;
	graphNodeMapping mapping;
};
STR_TYPE_DEF(struct assocPairIR2Mapping, Assoc);
STR_TYPE_FUNCS(struct assocPairIR2Mapping, Assoc);
static int assocCmp(const struct assocPairIR2Mapping *a,
                    const struct assocPairIR2Mapping *b) {
	return ptrPtrCmp(&a->ir, &b->ir);
}
static int assocContainsMapping(const void *node,
                                const struct assocPairIR2Mapping *pair) {
	return pair->mapping == node;
}
typedef int (*gnCmpType)(const graphNodeIR *, const graphNodeIR *);

static void __IRFilter(graphNodeMapping connectTo, graphNodeIR startAt,
                       int (*pred)(graphNodeIR, const void *), const void *data,
                       strGraphNodeIRP *visited, strAssoc *ir2Mapping) {

	strGraphPath paths =
	    graphAllPathsToPredicate(startAt, startAt, isFlowBlockOrStartOrEndPred);
	strAssoc exit2Mapping CLEANUP(strAssocDestroy) = NULL;

	int foundNewPredItem = 0, foundPredItem = 0;
	for (long i2 = 0; i2 < strGraphPathSize(paths); i2++) {
		int pathIncludedFlow = 0;

		__auto_type connectTo2 = connectTo;
		if (strGraphEdgeIRPSize(paths[i2]) == 0)
			continue;

		for (long i3 = 0; i3 != strGraphEdgeIRPSize(paths[i2]); i3++) {
			if (IR_CONN_FLOW == *graphEdgeIRValuePtr(paths[i2][i3]))
				pathIncludedFlow = 1;

			__auto_type node = graphEdgeIROutgoing(paths[i2][i3]);

			// Check if predicate likes the value
			if (pred(node, data)) {
				foundPredItem = 1;

				// Check if mapping already exists
				struct assocPairIR2Mapping pair = {node, NULL};
				__auto_type find = strAssocSortedFind(*ir2Mapping, pair, assocCmp);
				if (NULL == find) {
					foundNewPredItem = 1;

					__auto_type mapping = graphNodeMappingCreate(node, 0);
					graphNodeMappingConnect(connectTo2, mapping, NULL);
					connectTo2 = mapping;

					// Add a maping
					struct assocPairIR2Mapping pair2 = {node, mapping};
					*ir2Mapping = strAssocSortedInsert(*ir2Mapping, pair2, assocCmp);
				} else {
					// Connect and break,no need to visit already visited node
					if (!graphNodeMappingConnectedTo(connectTo2, find->mapping))
						graphNodeMappingConnect(connectTo2, find->mapping, NULL);

					connectTo2 = find->mapping;
				}
			}
		}
		// Add exit for later use
		__auto_type last = paths[i2][strGraphEdgeIRPSize(paths[i2]) - 1];
		__auto_type lastNode = graphEdgeIROutgoing(last);

		struct assocPairIR2Mapping pair3 = {lastNode, connectTo2};
		exit2Mapping = strAssocSortedInsert(exit2Mapping, pair3, assocCmp);

		strGraphEdgeIRPDestroy(&paths[i2]);
	}

	//
	// Now that we have the exits,lets check if we added some mappings to the
	// exits
	// We only want to go through the parts of the path that contain an item that
	// fuffils the preficate. If no such item exists,we check ahead
	//
	if (foundNewPredItem || foundPredItem) {
		// Filter out paths that didint have an item(that fuffiled the predicate)
		// out
		exit2Mapping =
		    strAssocRemoveIf(exit2Mapping, connectTo, assocContainsMapping);
	}

	//
	// If found pred item,but none of them are new,we visited here before so quit
	//
	if (foundNewPredItem == 0 && foundPredItem)
		return;

	for (long i = 0; i != strAssocSize(exit2Mapping); i++) {
		__IRFilter(exit2Mapping[i].mapping, exit2Mapping[i].ir, pred, data, visited,
		           ir2Mapping);
	}

	strGraphPathDestroy(&paths);
}
graphNodeIR IRFilter(graphNodeIR start, int (*pred)(graphNodeIR, const void *),
                     const void *data) {
	strGraphNodeIRP visited CLEANUP(strGraphNodeIRPDestroy) = NULL;
	strAssoc assoc CLEANUP(strAssocDestroy) = NULL;
	__auto_type retVal = graphNodeMappingCreate(start, 0);
	__IRFilter(retVal, start, pred, data, &visited, &assoc);

	return retVal;
}
