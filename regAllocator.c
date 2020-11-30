#include <IRLiveness.h>
#include <SSA.h>
#include <assert.h>
#include <graphColoring.h>
#include <subExprElim.h>
#define DEBUG_PRINT_ENABLE 1
#include <debugPrint.h>
typedef int (*gnCmpType)(const graphNodeIR *, const graphNodeIR *);
static int ptrPtrCmp(const void *a, const void *b) {
	if (*(void **)a > *(void **)b)
		return 1;
	else if (*(void **)a < *(void **)b)
		return -1;
	else
		return 0;
}
static void transparentKill(graphNodeIR node) {
	__auto_type incoming = graphNodeIRIncoming(node);
	__auto_type outgoing = graphNodeIROutgoing(node);
	for (long i1 = 0; i1 != strGraphEdgeIRPSize(incoming); i1++)
		for (long i2 = 0; i2 != strGraphEdgeIRPSize(outgoing); i2++)
			graphNodeIRConnect(graphEdgeIRIncoming(incoming[i1]),
			                   graphEdgeIROutgoing(outgoing[i2]), IR_CONN_FLOW);

	graphNodeIRKill(&node, IRNodeDestroy, NULL);
}
static int noIncomingPred(const void *data, const graphNodeIR *node) {
	strGraphEdgeIRP incoming __attribute__((cleanup(strGraphEdgePDestroy))) =
	    graphNodeIRIncoming(*node);
	return strGraphEdgeIRPSize(incoming) != 0;
};
static void replaceNodeWithExpr(graphNodeIR node, graphNodeIR valueSink) {
	__auto_type incoming = graphNodeIRIncoming(node);
	__auto_type outgoing = graphNodeIROutgoing(node);

	// Find input nodes(with no incoming)
	__auto_type sources = graphNodeIRAllNodes(valueSink);
	sources = strGraphNodeIRPRemoveIf(graphNodeIRAllNodes(valueSink), NULL,
	                                  noIncomingPred);

	// Connnect incoming to sources
	for (long i1 = 0; i1 != strGraphEdgeIRPSize(incoming); i1++)
		for (long i2 = 0; i2 != strGraphNodeIRPSize(sources); i2++)
			graphNodeIRConnect(graphEdgeIRIncoming(incoming[i1]), sources[i2],
			                   *graphEdgeIRValuePtr(incoming[i1]));

	// Connect sink to outgoing
	for (long i1 = 0; i1 != strGraphEdgeIRPSize(outgoing); i1++)
		graphNodeIRConnect(valueSink, graphEdgeIRIncoming(outgoing[i1]),
		                   *graphEdgeIRValuePtr(outgoing[i1]));

	strGraphEdgeIRPDestroy(&incoming), strGraphEdgeIRPDestroy(&outgoing);
}
static void removeChooseNode(graphNodeIR chooseNode) {
	// Kill node,remove outgoing nodes too if they variable they point to is
	// unused
	__auto_type outgoingNodes = graphNodeIROutgoingNodes(chooseNode);

	// Remove unused result vars
	for (long i2 = 0; i2 != strGraphNodeIRPSize(outgoingNodes); i2++) {
		__auto_type outgoingEdges = graphNodeIROutgoing(outgoingNodes[i2]);
		__auto_type filtered = IRGetConnsOfType(outgoingEdges, IR_CONN_FLOW);

		// If only flow out,then no need for variable
		int useless =
		    strGraphEdgeIRPSize(filtered) == strGraphEdgeIRPSize(outgoingEdges);

		strGraphEdgeIRPDestroy(&outgoingEdges);
		strGraphEdgeIRPDestroy(&filtered);

		// Remove var if useless
		if (useless)
			transparentKill(outgoingNodes[i2]);
	}

	strGraphNodeIRPDestroy(&outgoingNodes);
	transparentKill(chooseNode);
}
static void removeChooseNodes(strGraphNodeIRP nodes, graphNodeIR start) {
	for (long i = 0; i != strGraphNodeIRPSize(nodes); i++) {
		__auto_type val = graphNodeIRValuePtr(nodes[i]);
		// No value?
		if (!val)
			continue;

		// Ignore non-chooses
		if (val->type != IR_CHOOSE)
			continue;

		removeChooseNode(nodes[i]);
	}
}
LL_TYPE_DEF(strGraphNodeIRP, VarRefs);
LL_TYPE_FUNCS(strGraphNodeIRP, VarRefs);
struct aliasPair {
	graphNodeIR a, b;
};
static int aliasPairCmp(const struct aliasPair *a, const struct aliasPair *b) {
	struct IRNodeValue *Aa = (void *)graphNodeIRValuePtr(a->a);
	struct IRNodeValue *Ba = (void *)graphNodeIRValuePtr(b->a);
	struct IRNodeValue *Ab = (void *)graphNodeIRValuePtr(a->b);
	struct IRNodeValue *Bb = (void *)graphNodeIRValuePtr(b->b);
	int cmpA = IRVarCmp(&Aa->val.value.var, &Ba->val.value.var);
	if (cmpA != 0)
		return cmpA;

	int cmpB = IRVarCmp(&Ab->val.value.var, &Bb->val.value.var);
	if (cmpB != 0)
		return cmpB;

	return 0;
}
static int varRefsInsertCmp(const strGraphNodeIRP *a,
                            const strGraphNodeIRP *b) {
		struct IRNodeValue *valA = (void *)graphNodeIRValuePtr(a[0][0]), *valB = (void *)graphNodeIRValuePtr(b[0][0]);
	return IRVarCmp((const struct IRVar *)&valA->val.value.var,
	                (const struct IRVar *)&valB->val.value.var);
}
static int varRefsGetCmp(const void *a, const strGraphNodeP *b) {
		struct IRNodeValue *valB = (void*)graphNodeIRValuePtr(b[0][0]);
	struct IRNodeValue *valA = (void *)graphNodeIRValuePtr((graphNodeIR)a);
	return IRVarCmp((const struct IRVar *)&valA->val.value.var,
	                (const struct IRVar *)&valB->val.value.var);
}
STR_TYPE_DEF(struct aliasPair, AliasPair);
STR_TYPE_FUNCS(struct aliasPair, AliasPair);
STR_TYPE_DEF(strGraphNodeIRP, AliasBlob);
STR_TYPE_FUNCS(strGraphNodeIRP, AliasBlob);
static int AliasBlobCmp(const strGraphNodeIRP *A, const strGraphNodeIRP *B) {
	long aSize = strGraphNodeIRPSize(*A);
	long bSize = strGraphNodeIRPSize(*B);
	long max = (aSize > bSize) ? aSize : bSize;

	// Check for difference in shared portion
	for (long i = 0; i != max; i++) {
		if (ptrPtrCmp(&A[i], &B[i]))
			return ptrPtrCmp(&A[i], &B[i]);
	}

	// Now compare by sizes
	if (aSize == bSize)
		return 0;
	else if (aSize > bSize)
		return 1;
	else if (aSize < bSize)
		return -1;

	return 0;
}
void IRCoalesce(strGraphNodeIRP nodes, graphNodeIR start) {
	llVarRefs refs = NULL;
	strAliasPair aliases = NULL;

	for (long i = 0; i != strGraphNodeIRPSize(nodes); i++) {
		__auto_type val = graphNodeIRValuePtr(nodes[i]);
		// No value?
		if (!val)
			continue;

		// Ignore non-chooses
		if (val->type == IR_CHOOSE) {
			struct IRNodeChoose *choose = (void *)val;
			//
			// Remove choose nodes whoose incoming vars are all the same ,ALSO,be sure
			// to alias incoming var to choose with outgoing to choose
			//
			strGraphNodeIRP clone __attribute__((cleanup(strGraphNodeIRPDestroy))) =
			    strGraphNodeIRPUnique(strGraphNodeIRPClone(choose->canidates),
			                          (gnCmpType)ptrPtrCmp, NULL);
			if (strGraphNodeIRPSize(clone) == 1) {
				struct IRNodeValue *value = (void *)graphNodeIRValuePtr(clone[i]);
				assert(value->base.type == IR_VALUE);
				assert(value->val.type == IR_VAL_VAR_REF);

				// Check for signle outgoing vars to alias to
				strGraphEdgeIRP outgoing
				    __attribute__((cleanup(strGraphEdgeIRPDestroy))) =
				        graphNodeIROutgoing(clone[i]);
				strGraphEdgeIRP outgoingAssigns
				    __attribute__((cleanup(strGraphEdgeIRPDestroy))) =
				        IRGetConnsOfType(outgoing, IR_CONN_DEST);

				for (long i = 0; i != strGraphEdgeIRPSize(outgoingAssigns); i++) {
					__auto_type to = graphEdgeIROutgoing(outgoingAssigns[i]);
					struct IRNodeValue *val = (void *)graphNodeIRValuePtr(to);
					assert(val->base.type == IR_VALUE);
					// Only alias to vars
					if (val->val.type != IR_VAL_VAR_REF)
						continue;

					// Alias to previous new version of choose var
					struct aliasPair apForward = {clone[i], nodes[i]};
					if (NULL ==
					    strAliasPairSortedFind(aliases, apForward, aliasPairCmp)) {
						aliases =
						    strAliasPairSortedInsert(aliases, apForward, aliasPairCmp);
					}
				}
			}

		} else if (val->type == IR_VALUE) {
			// Is a var,check for direct assign
			if (((struct IRNodeValue *)val)->val.type == IR_VAL_VAR_REF) {
			findVarLoop:;
				__auto_type find = llVarRefsFind(refs, nodes[i], varRefsGetCmp);
				__auto_type find2 = llVarRefsFindRight(llVarRefsFirst(refs), nodes[3], varRefsGetCmp);
	
				if (!find) {
						DEBUG_PRINT("Adding var node %s\n", debugGetPtrNameConst(nodes[i]));
						__auto_type newRef =
								llVarRefsCreate(strGraphNodeIRPAppendItem(NULL, nodes[i]));
						llVarRefsInsert(refs, newRef, varRefsInsertCmp);
						refs = newRef;

					goto findVarLoop;
				}

				// Check if written into by another variable.
				__auto_type incoming = graphNodeIRIncoming(nodes[i]);
				__auto_type filtered = IRGetConnsOfType(incoming, IR_CONN_DEST);

				// aliasNode is NULL if an alias node isnt found
				graphNodeIR aliasNode = NULL;

				// If one filtered that is a value and a variable,set that as the alias
				// node
				if (strGraphEdgeIRPSize(filtered) == 1) {
					__auto_type inNode =
					    graphNodeIRValuePtr(graphEdgeIRIncoming(filtered[0]));
					if (inNode->type == IR_VALUE) {
						struct IRNodeValue *inValueNode = (void *)inNode;
						if (inValueNode->val.type == IR_VAL_VAR_REF) {
								aliasNode = graphEdgeIRIncoming(filtered[0]);
								DEBUG_PRINT("%s aliased to node %s\n",debugGetPtrNameConst(aliasNode), debugGetPtrNameConst(nodes[i]));
								
						}
					}
				}

				strGraphEdgeIRPDestroy(&incoming), strGraphEdgeIRPDestroy(&filtered);

				// If alais,add entry(if doesnt already exist)
				if (aliasNode) {
					struct aliasPair apForward = {aliasNode, nodes[i]};
					if (NULL ==
					    strAliasPairSortedFind(aliases, apForward, aliasPairCmp)) {
						aliases =
						    strAliasPairSortedInsert(aliases, apForward, aliasPairCmp);
					}
				}
			}
		}
	}

	//
	// Merge aliases that share same head/tail untill you cant do it anymore. This
	// allows for multiple vars to be merges For Example: a=b=c would be(before)
	// b<->c
	// a<->b
	// which would be turned into(after)
	// a<->b<->c

	// Turn pairs into blobs.
	strAliasBlob blobs = NULL;
	for (long i = 0; i != strAliasPairSize(aliases); i++) {
		strGraphNodeIRP blob = strGraphNodeIRPResize(NULL, 2);
		blob[0] = aliases[i].a;
		blob[1] = aliases[i].b;

		blobs = strAliasBlobSortedInsert(blobs, blob, AliasBlobCmp);
	}

	for (;;) {
	loop:;
		// First Check heads(i1) with tails(i2)
		for (long i1 = 0; i1 != strAliasBlobSize(blobs); i1++) {
			for (long i2 = 0; i2 != strAliasBlobSize(blobs); i2++) {
				__auto_type head = blobs[i1][0];
				// Last elem
				__auto_type tail = blobs[i2][strGraphNodeIRPSize(blobs[i2]) - 1];

				if (head == tail) {
						DEBUG_PRINT("Head:%s,Tail:%s\n", debugGetPtrNameConst(head),debugGetPtrNameConst(tail));
					// Merge tail node with head node
					blobs[i2] = strGraphNodeIRPConcat(blobs[i2], blobs[i1]);

					// Remove head node from blobs and restart loop
					memmove(&blobs[i1], &blobs[i1] + 1, strAliasBlobSize(blobs) - 1);

					//Pop to decrese size by 1
					blobs[i1]=strGraphNodeIRPPop(blobs[i1], NULL);
					goto loop;
				}
			}
		}
		// Didn't loop back
		break;
	}

	//
	// Replace vars with aliases
	//
	for (long i = 0; i != strAliasBlobSize(blobs); i++) {
			DEBUG_PRINT("Finding var %s\n", debugGetPtrNameConst(blobs[i][0]));
		__auto_type find = llVarRefsFind(refs, blobs[i][0], varRefsGetCmp);

		// Find first ref.
		strGraphNodeIRP refs2 = *llVarRefsValuePtr(find);
		__auto_type master = refs2[0];

		// Replace rest of blobs
		for (long i2 = 1; i2 != strGraphNodeIRPSize(blobs[i]); i2++) {
			strGraphNodeIRP refs = *llVarRefsValuePtr(find);
			for (long i3 = 1; i3 != strGraphNodeIRPSize(refs); i3++) {
					DEBUG_PRINT("Replacing %s with %s\n", debugGetPtrNameConst(refs[0]),debugGetPtrNameConst(master));
					// Replace with cloned value
					replaceNodeWithExpr(refs[0], cloneNode(master, IR_CLONE_NODE, NULL));
			}
		}
	}
}
static void IRRegisterAllocate(graphNodeIR start) {
	__auto_type allNodes = graphNodeIRAllNodes(start);
	removeChooseNodes(allNodes, start);
}
