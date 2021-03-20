#include "IRFilter.h"
#include "IRLiveness.h"
#include <assert.h>
#include "cleanup.h"
#include "graphColoring.h"
#define DEBUG_PRINT_ENABLE 1
#include "debugPrint.h"
#include "regAllocator.h"
#include <math.h>
static __thread const void *__varFilterData = NULL;
static __thread int (*__varFiltPred)(const struct parserVar *, const void *);
static char *ptr2Str(const void *a) {
		long len=snprintf(NULL, 0, "%p", a);
		char buffer[len+1];
		sprintf(buffer, "%p", a);
		return strcpy(calloc(len+1,1),buffer);
}
static int IRVarCmp2(const struct IRVar **a, const struct IRVar **b) {
	return IRVarCmp(*a, *b);
}
static char *var2Str(graphNodeIR var) {
	if (debugGetPtrNameConst(var))
		return debugGetPtrName(var);

	__auto_type value = (struct IRNodeValue *)graphNodeIRValuePtr(var);
	if (value->val.value.var.var->name) {
		char buffer[1024];
		sprintf(buffer, "%s-%li", value->val.value.var.var->name, value->val.value.var.SSANum);
		char *retVal = calloc(strlen(buffer) + 1,1);
		strcpy(retVal, buffer);
		return retVal;
	} else {
		char buffer[1024];
		sprintf(buffer, "%p-%li", value->val.value.var.var, value->val.value.var.SSANum);
		char *retVal = calloc(strlen(buffer) + 1,1);
		strcpy(retVal, buffer);
		return retVal;
	}
	return NULL;
}
static char *strClone(const char *text) {
		char *retVal = calloc(strlen(text) + 1,1);
	strcpy(retVal, text);

	return retVal;
}
static void debugShowGraphIR(graphNodeIR enter) {
#if DEBUG_PRINT_ENABLE
		const char *name = tmpnam(NULL);
	__auto_type map = graphNodeCreateMapping(enter, 1);
	IRGraphMap2GraphViz(map, "viz", name, NULL, NULL, NULL, NULL);
	char buffer[1024];
	sprintf(buffer, "sleep 0.1 &&dot -Tsvg %s > /tmp/dot.svg && firefox /tmp/dot.svg & ", name);

	system(buffer);
#endif
}
PTR_MAP_FUNCS(struct __graphNode *, struct regSlice, regSlice);
static char *interfereNode2Label(const struct __graphNode *node, mapGraphVizAttr *attrs, const void *data) {
	ptrMapregSlice map = (void *)data;

	__auto_type var = &graphNodeIRLiveValuePtr((graphNodeIRLive)node)->ref;
	__auto_type dummy = IRCreateVarRef(var->var);
	((struct IRNodeValue *)graphNodeIRValuePtr(dummy))->val.value.var.SSANum = var->SSANum;
	char *name = var2Str(dummy);
	graphNodeIRKill(&dummy, (void(*)(void*))IRNodeDestroy, NULL);

	if (name)
		name = name;
	else if (var->var->name)
		name = strClone(var->var->name);
	else
		name = ptr2Str(var);

	if (map)
		if (ptrMapregSliceGet(map, (struct __graphNode *)node)) {
			const char *format = "%s(%s)";
			long len = snprintf(NULL, 0, format, name, ptrMapregSliceGet(map, (struct __graphNode *)node)->reg->name);
			char buffer[len + 1];
			sprintf(buffer, format, name, ptrMapregSliceGet(map, (struct __graphNode *)node)->reg->name);

			return strClone(buffer);
		}
	return name;
}
static void debugPrintInterferenceGraph(graphNodeIRLive graph, ptrMapregSlice map) {
	char *fn = tmpnam(NULL);
	FILE *file = fopen(fn, "w");
	graph2GraphVizUndir(file, graph, "interference", interfereNode2Label, map);
	fclose(file);

	char buffer[512];
	sprintf(buffer,
	        "sleep 0.1 && dot -Tsvg %s>/tmp/interfere.svg  && firefox "
	        "/tmp/interfere.svg &",
	        fn);
	system(buffer);
}
static const char *IR_ATTR_NODE_SPILL_LOADS_COMPUTED = "COMPUTED_SPILLS_AND_LOADS";
static const char *IR_ATTR_NODE_VARS_SPILLED_AT = "SPILLED_VAR_AT";
static const char *IR_ATTR_NODE_LOAD_VAR_AT = "LOADED_VAR_AT";
STR_TYPE_DEF(struct IRVar *, IRVar);
STR_TYPE_FUNCS(struct IRVar *, IRVar);
typedef int (*gnCmpType)(const graphNodeIR *, const graphNodeIR *);
static int ptrPtrCmp(const void *a, const void *b) {
		if(*(void**)a>*(void**)b)
				return 1;
		else if(*(void**)a<*(void**)b)
				return -1;
		return 0;
}
static void transparentKill(graphNodeIR node, int preserveEdgeValue) {
	__auto_type incoming = graphNodeIRIncoming(node);
	__auto_type outgoing = graphNodeIROutgoing(node);
	for (long i1 = 0; i1 != strGraphEdgeIRPSize(incoming); i1++)
		for (long i2 = 0; i2 != strGraphEdgeIRPSize(outgoing); i2++)
			graphNodeIRConnect(graphEdgeIRIncoming(incoming[i1]), graphEdgeIROutgoing(outgoing[i2]),
			                   (preserveEdgeValue) ? *graphEdgeIRValuePtr(incoming[i1]) : IR_CONN_FLOW);

	graphNodeIRKill(&node, (void(*)(void*))IRNodeDestroy, NULL);
}
static int noIncomingPred(const void *data, const graphNodeIR *node) {
	strGraphEdgeIRP incoming = graphNodeIRIncoming(*node);
	return strGraphEdgeIRPSize(incoming) != 0;
};
static void replaceNodeWithExpr(graphNodeIR node, graphNodeIR valueSink) {
	__auto_type incoming = graphNodeIRIncoming(node);
	__auto_type outgoing = graphNodeIROutgoing(node);

	// Find input nodes(with no incoming)
	__auto_type sources = graphNodeIRAllNodes(valueSink);
	sources = strGraphNodeIRPRemoveIf(graphNodeIRAllNodes(valueSink), NULL, noIncomingPred);

	// Connnect incoming to sources
	for (long i1 = 0; i1 != strGraphEdgeIRPSize(incoming); i1++)
		for (long i2 = 0; i2 != strGraphNodeIRPSize(sources); i2++)
			graphNodeIRConnect(graphEdgeIRIncoming(incoming[i1]), sources[i2], *graphEdgeIRValuePtr(incoming[i1]));

	// Connect sink to outgoing
	for (long i1 = 0; i1 != strGraphEdgeIRPSize(outgoing); i1++)
		graphNodeIRConnect(valueSink, graphEdgeIROutgoing(outgoing[i1]), *graphEdgeIRValuePtr(outgoing[i1]));

	graphNodeIRKill(&node, (void(*)(void*))IRNodeDestroy, NULL);
}
static int gnIRVarCmp(const graphNodeIR *a, const graphNodeIR *b) {
	__auto_type A = (struct IRNodeValue *)graphNodeIRValuePtr(*a);
	__auto_type B = (struct IRNodeValue *)graphNodeIRValuePtr(*b);
	assert(A->base.type == IR_VALUE);
	assert(B->base.type == IR_VALUE);

	return IRVarCmp(&A->val.value.var, &B->val.value.var);
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
		int useless = strGraphEdgeIRPSize(filtered) == strGraphEdgeIRPSize(outgoingEdges);

		// Remove var if useless
		if (useless)
			transparentKill(outgoingNodes[i2], 1);
	}

	transparentKill(chooseNode, 1);
}
STR_TYPE_DEF(strGraphNodeIRP, VarRefs);
STR_TYPE_FUNCS(strGraphNodeIRP, VarRefs);
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
static int varRefsGetCmp(const strGraphNodeIRP *a, const strGraphNodeP *b) {
	long aSize = strGraphNodeIRPSize(*a);
	long bSize = strGraphNodeIRPSize(*b);
	long minSize = (aSize < bSize) ? aSize : bSize;

	for (long i = 0; i != minSize; i++) {
		int cmp = ptrPtrCmp(a + i, b + i);
		if (cmp != 0)
			return cmp;
	}

	if (aSize > bSize)
		return 1;
	if (aSize < bSize)
		return -1;
	else
		return 0;
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
static int getVarRefIndex(strVarRefs refs, graphNodeIR node) {
	for (long i = 0; i != strVarRefsSize(refs); i++) {
		if (strGraphNodeIRPSortedFind(refs[i], node, (gnCmpType)ptrPtrCmp))
			return i;
	}

	printf("Dear programmer,variable node %s not found in refs\n", debugGetPtrNameConst(node));
	assert(0);
	return -1;
}
static struct IRVar *getVar(graphNodeIR node) {
	struct IRNode *irNode = (void *)graphNodeIRValuePtr(node);
	if (irNode->type == IR_SPILL_LOAD) {
		struct IRNodeSpill *spill = (void *)irNode;
		if (spill->item.type != IR_VAL_VAR_REF)
			return NULL;

		return &spill->item.value.var;
	} else if (irNode->type == IR_VALUE) {
		struct IRNodeValue *value = (void *)irNode;
		if (value->val.type == IR_VAL_VAR_REF) {
			return &value->val.value.var;
		} else if (value->val.type == IR_VAL_REG) {
			// Check for vairiable attribute if in register
			__auto_type find = llIRAttrFind(irNode->attrs, IR_ATTR_VARIABLE, IRAttrGetPred);
			if (!find)
				return NULL;

			return &((struct IRAttrVariable *)llIRAttrValuePtr(find))->var;
		} else
			return NULL;
	} else
		return NULL;
}
static int chooseCmp(const graphNodeIR *a,const graphNodeIR *b) {
		struct  IRNodeChoose *A=(void*)graphNodeIRValuePtr(*a);
		struct  IRNodeChoose *B=(void*)graphNodeIRValuePtr(*b);
		long aSize=strGraphNodeIRPSize(A->canidates);
		long bSize=strGraphNodeIRPSize(A->canidates);
		long min=aSize>bSize?bSize:aSize;
		for(long v=0;v!=min;v++) {
				int cmp=IRVarCmp(getVar(A->canidates[v]),getVar(B->canidates[v]));
				if(cmp!=0)
						return cmp;
		}
		if(aSize==bSize)
				return 0;
		return aSize>bSize?1:-1;
}
MAP_TYPE_DEF(long,Count);
MAP_TYPE_FUNCS(long,Count);
static int IsInteger(struct object *obj) {
	// Check if ptr
	if (obj->type == TYPE_PTR || obj->type == TYPE_ARRAY)
		return 1;

	// Check if integer type
	struct object *valids[] = {
	    &typeI8i, &typeI16i, &typeI32i, &typeI64i, &typeU8i, &typeU16i, &typeU32i, &typeU64i, &typeBool,
	};
	for (long i = 0; i != sizeof(valids) / sizeof(*valids); i++)
		if (valids[i] == obj)
			return 1;

	return 0;
}
static int isFloating(struct object *obj) {
	return obj == &typeF64;
}
static double interfereMetric(double cost, graphNodeIRLive node) {
	//
	// graphNodeIRLive is undirected node!!!
	//
	strGraphEdgeIRP connections CLEANUP(strGraphEdgeIRPDestroy) = graphNodeIRLiveOutgoing(node);
	double retVal = cost / (strGraphEdgeIRLivePSize(connections));

	return retVal;
}
struct conflictPair {
	graphNodeIRLive a;
	graphNodeIRLive b;
	double aWeight;
	double bWeight;
};
static int conflictPairWeightCmp(const void *a, const void *b) {
	const struct conflictPair *A = a, *B = b;

	double minWeightA = (A->aWeight < A->bWeight) ? A->aWeight : A->bWeight;
	double minWeightB = (B->aWeight < B->bWeight) ? B->aWeight : B->bWeight;

	if (minWeightA > minWeightB)
		return 1;
	else if (minWeightA < minWeightB)
		return -1;
	return 0;
}
static int conflictPairCmp(const struct conflictPair *a, const struct conflictPair *b) {
	int cmp = ptrPtrCmp(&a->a, &b->a);
	if (cmp != 0)
		return cmp;

	return ptrPtrCmp(&a->b, &b->b);
}
STR_TYPE_DEF(struct conflictPair, ConflictPair);
STR_TYPE_FUNCS(struct conflictPair, ConflictPair);
static int filterIntVars(graphNodeIR node, const void *data) {
	struct IRNodeValue *value = (void *)graphNodeIRValuePtr(node);
	if (value->base.type != IR_VALUE)
		return 0;
	if (value->val.type != IR_VAL_VAR_REF)
		return 0;
	if (!IsInteger(IRValueGetType(&value->val)))
		return 0;
	if (value->val.value.var.var->isRefedByPtr)
		return 0;
	if (value->val.value.var.var->isNoreg)
		return 0;
	if (value->val.value.var.var->isGlobal)
			return 0;
	if (__varFiltPred)
		if (!__varFiltPred(value->val.value.var.var, __varFilterData))
			return 0;
	return 1;
}
static int filterFloatVars(graphNodeIR node, const void *data) {
	struct IRNodeValue *value = (void *)graphNodeIRValuePtr(node);
	if (value->base.type != IR_VALUE)
		return 0;
	if (value->val.type != IR_VAL_VAR_REF)
		return 0;
	if (!isFloating(IRValueGetType(&value->val)))
		return 0;
	if (value->val.value.var.var->isRefedByPtr)
		return 0;
	if (__varFiltPred)
		if (!__varFiltPred(value->val.value.var.var, __varFilterData))
			return 0;
	return 1;
}
static int isVarNode(const struct IRNode *irNode) {
	if (irNode->type == IR_VALUE) {
		struct IRNodeValue *val = (void *)irNode;
		if (val->val.type == IR_VAL_VAR_REF)
			return 1;
	}

	return 0;
}
STR_TYPE_DEF(int, Int);
STR_TYPE_FUNCS(int, Int);
static int intCmp(const int *a, const int *b) {
	return *a - *b;
}
struct metricPair {
	graphNodeIRLive node;
	double metricValue;
};
struct interferencePair {
	struct IRVar *var;
	strIRVar inteferesWith;
};
static int isVar(graphNodeIR node) {
	if (graphNodeIRValuePtr(node)->type == IR_VALUE) {
		// Is a variable
		struct IRNodeValue *valOut = (void *)graphNodeIRValuePtr(node);
		if (valOut->val.type == IR_VAL_VAR_REF) {
			return 1;
		}
	}
	return 0;
}
static int graphPathCmp(const strGraphEdgeIRP *a, const strGraphEdgeIRP *b) {
	long aSize = strGraphEdgeIRPSize(*a);
	long bSize = strGraphEdgeIRPSize(*b);
	long min = (aSize < bSize) ? aSize : bSize;

	for (long i = 0; i != min; i++) {
		__auto_type aNode = graphEdgeIROutgoing(a[0][i]);
		__auto_type bNode = graphEdgeIROutgoing(b[0][i]);
		int cmp = ptrPtrCmp(&aNode, &bNode);
		if (cmp != 0)
			return cmp;
	}

	if (aSize > bSize)
		return 1;
	else if (aSize < bSize)
		return -1;
	else
		return 0;
}

struct varsAndReplaced {
	strIRVar vars;
	strGraphNodeIRP *replaced;
};
static int untillStartOfExpr(const struct __graphNode *node, const struct __graphEdge *edge, const void *data) {
	return IRIsExprEdge(*graphEdgeIRValuePtr((graphEdgeIR)edge));
};

struct varToLiveNode {
	struct IRVar var;
	graphNodeIRLive live;
};
static int varToLiveNodeCompare(const struct varToLiveNode *a, const struct varToLiveNode *b) {
	return IRVarCmp(&a->var, &b->var);
}
STR_TYPE_DEF(struct varToLiveNode, VarToLiveNode);
STR_TYPE_FUNCS(struct varToLiveNode, VarToLiveNode);
static void removeDeadExpresions(graphNodeIR startAt, strIRVar liveVars) {
	strGraphNodeIRP allNodes CLEANUP(strGraphNodeIRPDestroy) = graphNodeIRAllNodes(startAt);
	strGraphNodeIRP toRemove CLEANUP(strGraphNodeIRPDestroy) = strGraphNodeIRPReserve(NULL, strGraphNodeIRPSize(allNodes));

loop:
	allNodes = strGraphNodeIRPSetDifference(allNodes, toRemove, (gnCmpType)ptrPtrCmp);

	for (long i = 0; i != strGraphNodeIRPSize(allNodes); i++) {
		// Dont revisit
		toRemove = strGraphNodeIRPSortedInsert(toRemove, allNodes[i], (gnCmpType)ptrPtrCmp);

		//
		// Find end of expression that is an assigned node
		//

		// Check for assign
		strGraphEdgeIRP incoming = graphNodeIRIncoming(allNodes[i]);
		strGraphEdgeIRP incomingAssigns = IRGetConnsOfType(incoming, IR_CONN_DEST);
		if (strGraphEdgeIRPSize(incomingAssigns)) {
			// If destination is a variable that isn't alive,destroy it
			if (isVar(allNodes[i])) {
				// Ensure allNodes[i ] is end of expression
				strGraphEdgeIRP outgoing = graphNodeIROutgoing(allNodes[i]);
				for (long i = 0; i != strGraphEdgeIRPSize(outgoing); i++)
					if (IRIsExprEdge(*graphEdgeIRValuePtr(outgoing[i])))
						goto __continue;

				// Ensure points to non-alive variable
				struct IRNodeValue *value = (void *)graphNodeIRValuePtr(allNodes[i]);
				if (NULL == strIRVarSortedFind(liveVars, &value->val.value.var, IRVarCmp2)) {
					__auto_type in = graphEdgeIRIncoming(incoming[0]);

					// Wasn't found in live vairables so delete assign
					transparentKill(allNodes[i], 1);

					// Check if dead expression now that we removed the dead assign
					if (IRIsDeadExpression(in)) {
						// Remove dead expression
						strGraphNodeIRP removed CLEANUP(strGraphNodeIRPDestroy);
						IRRemoveDeadExpression(in, &removed);
						toRemove = strGraphNodeIRPSetUnion(toRemove, removed, (gnCmpType)ptrPtrCmp);

						// Add allNodes[i] to removed(transparently killed above)
						toRemove = strGraphNodeIRPSortedInsert(toRemove, allNodes[i], (gnCmpType)ptrPtrCmp);

						// Restart search
						goto loop;
					}
				}
			}
		}
	__continue:;
	}
}
static int nodeEqual(const graphNodeIR *a, const graphNodeIR *b) {
	return *a == *b;
}
//
// This a function for turning colors into registers
//

typedef int (*regCmpType)(const struct reg **, const struct reg **);
typedef struct regSlice (*color2RegPredicate)(strRegSlice adjacent, strRegP avail, graphNodeIRLive live, int color, const void *data, long colorCount,
                                              const int *colors);
static struct regSlice color2Reg(strRegSlice adjacent, strRegP avail, graphNodeIRLive live, int color, const void *data, long colorCount) {
	__auto_type avail2 = strRegPClone(avail);
	strRegP adjRegs = NULL;
	for (long i = 0; i != strRegSliceSize(adjacent); i++) {
		adjRegs = strRegPSortedInsert(adjRegs, adjacent[i].reg, (regCmpType)ptrPtrCmp);
	}
	avail2 = strRegPSetDifference(avail2, adjRegs, (regCmpType)ptrPtrCmp);

	if (strRegPSize(avail2) == 0) {
		avail2 = strRegPClone(avail);
	}

	struct regSlice slice;
	slice.reg = avail2[color % strRegPSize(avail2)];
	slice.offset = 0;
	slice.widthInBits = slice.reg->size * 8;;
	slice.type = graphNodeIRLiveValuePtr(live)->ref.var->type;

	return slice;
}
STR_TYPE_DEF(struct metricPair, MetricPair);
STR_TYPE_FUNCS(struct metricPair, MetricPair);
static struct conflictPair *__conflictPairFindAffects(graphNodeIRLive node, const struct conflictPair *start, const struct conflictPair *end) {
	for (; start != end; start++) {
		if (start->a == node)
			return (void *)start;
		else if (start->b == node)
			return (void *)start;
	}

	return NULL;
}
strConflictPair conflictPairFindAffects(graphNodeIRLive node, strConflictPair pairs) {
	strConflictPair retVal = NULL;
	__auto_type start = pairs;
	__auto_type end = pairs + strConflictPairSize(pairs);
	for (; start != end;) {
		__auto_type find = __conflictPairFindAffects(node, start, end);
		// Quit if no find
		if (!find)
			break;

		// Insert
		retVal = strConflictPairSortedInsert(retVal, *find, conflictPairCmp);

		// Continue from after find
		start = find + 1;
	}
	return retVal;
}
static int conflictPairContains(graphNodeIRLive node, const struct conflictPair *pair) {
	return pair->a == node || pair->b == node;
}
static void *IR_ATTR_TEMP_VARIABLE = "TMP_VAR";
static void replaceVarsWithRegisters(ptrMapregSlice map, strGraphNodeIRLiveP allLiveNodes,strGraphNodeIRP *allNodes) {
	//
	// Create an associative array to turn variables into live nodes
	//
		strVarToLiveNode varToLive CLEANUP(strVarToLiveNodeDestroy) = NULL;
	for (long i = 0; i != strGraphNodeIRLivePSize(allLiveNodes); i++) {
		struct varToLiveNode pair;
		pair.live = allLiveNodes[i];
		pair.var = graphNodeIRLiveValuePtr(allLiveNodes[i])->ref;
		varToLive = strVarToLiveNodeSortedInsert(varToLive, pair, varToLiveNodeCompare);
	}

	strGraphNodeIRP removed CLEANUP(strGraphNodeIRPDestroy)=NULL;
	for (long i = 0; i != strGraphNodeIRPSize(allNodes[0]); i++) {
		// Look for vairable
		if (isVar(allNodes[0][i])) {
			struct IRNodeValue *value = (void *)graphNodeIRValuePtr(allNodes[0][i]);

			// Ensure variable is in the current liveness graph
			struct IRVar var = value->val.value.var;
			struct varToLiveNode dummy;
			dummy.live = NULL;
			dummy.var = value->val.value.var;
			__auto_type find = strVarToLiveNodeSortedFind(varToLive, dummy, varToLiveNodeCompare);

			if (find) {
				// Get register slice from liveness node to register slice map
				__auto_type find2 = ptrMapregSliceGet(map, find->live);
				if (!find2)
					continue;

				__auto_type slice = *find2;

				// Replace
				__auto_type regRef = IRCreateRegRef(&slice);
				replaceNodeWithExpr(allNodes[0][i], regRef);
				removed=strGraphNodeIRPSortedInsert(removed, allNodes[0][i], (gnCmpType)ptrPtrCmp);
				
				// Add attrbute to regRef that marks as originating from varaible
				struct IRAttrVariable attr;
				attr.base.name = IR_ATTR_VARIABLE;
				attr.base.destroy = NULL;
				attr.var = var;
				var.var->refCount++;
				__auto_type attrLL = __llCreate(&attr, sizeof(attr));
				// Insert
				__auto_type attrPtr = &graphNodeIRValuePtr(regRef)->attrs;
				llIRAttrInsert(*attrPtr, attrLL, IRAttrInsertPred);
				*attrPtr = attrLL;
			}
		}
	}
	*allNodes=strGraphNodeIRPSetDifference(*allNodes, removed, (gnCmpType)ptrPtrCmp);
}
void IRAttrVariableRemoveAllNodes(graphNodeIR node) {
	strGraphNodeIRP allNodes CLEANUP(strGraphNodeIRPDestroy) = graphNodeIRAllNodes(node);
	for (long i = 0; i != strGraphNodeIRPSize(allNodes); i++) {
		__auto_type attrsPtr = &graphNodeIRValuePtr(allNodes[i])->attrs;
		__auto_type find = llIRAttrFind(*attrsPtr, IR_ATTR_VARIABLE, IRAttrGetPred);
		if (find) {
			*attrsPtr = llIRAttrRemove(find);
			llIRAttrDestroy(&find, NULL);
		}
	}
}
static int isAssignedLiveVarOrLoad(graphNodeIR start, const void *live) {
	// Check if load of live variable
	if (graphNodeIRValuePtr(start)->type == IR_SPILL_LOAD) {
		struct IRNodeSpill *spill = (void *)graphNodeIRValuePtr(start);
		strVar liveVars = (void *)live;
		return NULL != strVarSortedFind(liveVars, spill->item.value.var, IRVarCmp);
	}

	// Is attributed to a live varible
	__auto_type find = llIRAttrFind(graphNodeIRValuePtr(start)->attrs, IR_ATTR_VARIABLE, IRAttrGetPred);
	if (find) {
		struct IRAttrVariable *var = (void *)llIRAttrValuePtr(find);

		struct IRNodeValue *val = (void *)graphNodeIRValuePtr(start);
		strVar liveVars = (void *)live;
		return NULL != strVarSortedFind(liveVars, var->var, IRVarCmp);
	}

	return 0;
}
static int isAssignRegMappedNode(const void *data, const graphNodeMapping *mapping) {
	strGraphEdgeIRP incoming CLEANUP(strGraphEdgeIRPDestroy) = graphNodeIRIncoming(*graphNodeMappingValuePtr((graphNodeMapping)*mapping));
	strGraphEdgeIRP assigns CLEANUP(strGraphEdgeIRPDestroy) = IRGetConnsOfType(incoming, IR_CONN_DEST);
	return strGraphEdgeIRPSize(assigns) != 0;
}
struct varInReg {
	struct IRVar var;
	struct regSlice slice;
};
static int varInRegCmp(const struct varInReg *a, const struct varInReg *b) {
	return IRVarCmp(&a->var, &b->var);
}
STR_TYPE_DEF(struct varInReg, VarInReg);
STR_TYPE_FUNCS(struct varInReg, VarInReg);
static void strGraphPathsDestroy2(strGraphPath *paths) {
	for (long i = 0; i != strGraphPathSize(*paths); i++)
		strGraphEdgePDestroy(&paths[0][i]);
	strGraphPathDestroy(paths);
}
static int isExprEdge(const void *data, const graphEdgeIR *edge) {
	return IRIsExprEdge(*graphEdgeIRValuePtr(*edge));
}
typedef int (*geCmpType)(const graphEdgeIR *, const graphEdgeIR *);
struct rematerialization {
	graphNodeIR clone;
	strRegSlice registers;
};
static void addToNodes(struct __graphNode *node, void *data) {
	strGraphNodeIRP *Data = (void *)data;
	*Data = strGraphNodeIRPSortedInsert(*Data, node, (gnCmpType)ptrPtrCmp);
}
static void ptrMapGraphNodeDestroy2(ptrMapGraphNode *node) {
 	ptrMapGraphNodeDestroy(*node, NULL);
}
static void mapRegSliceDestroy2(ptrMapregSlice *toDestroy) {
	ptrMapregSliceDestroy(*toDestroy, NULL);
}
static void strGraphNodeIRLivePDestroy2(strGraphNodeIRLiveP *str) {
		for(long g=0;g!=strGraphNodeIRLivePSize(*str);g++) {
				graphNodeIRLiveKill(&str[0][g], NULL, NULL);
		}
		strGraphNodeIRLivePDestroy(str);
}
static void llVertexColorsDestroy2(llVertexColor *colors) {
		llVertexColorDestroy( colors,NULL);
}
typedef int (*removeIfPred)(const void *, const struct conflictPair *);
void IRRegisterAllocate(graphNodeIR start, double (*nodeWeight)(struct IRVar *,void *data),void *nodeWeightData, int (*varFiltPred)(const struct parserVar *, const void *),
                        const void *varFiltData) {
	__varFilterData = varFiltData;
	__varFiltPred = varFiltPred;
	// SSA
	strGraphNodeIRP allNodes2 CLEANUP(strGraphNodeIRPDestroy) = graphNodeIRAllNodes(start);
	
	strGraphNodeIRLiveP intInterfere CLEANUP(strGraphNodeIRLivePDestroy2) = IRInterferenceGraphFilter(start, NULL, filterIntVars);

	__auto_type floatInterfere = NULL;
	// IRInterferenceGraphFilter(start, filterFloatVars, NULL);

	strIRVar liveVars CLEANUP(strIRVarDestroy) = NULL;
	strGraphNodeIRLiveP interferes  = strGraphNodeIRLivePClone(intInterfere);
	interferes = strGraphNodeIRLivePConcat(interferes, strGraphNodeIRLivePClone(floatInterfere));
	 {
		__auto_type interfere = interferes;

		ptrMapregSlice regsByLivenessNode = ptrMapregSliceCreate(); // TODO rename
		// Choose registers

		strGraphNodeIRLiveP allColorNodes CLEANUP(strGraphNodeIRPDestroy) = strGraphNodeIRLivePClone(interfere);
		for (long i = 0; i != strGraphNodeIRLivePSize(allColorNodes); i++) {
			// Get adjacent items
			__auto_type outgoing = graphNodeIRLiveOutgoingNodes(allColorNodes[i]);
			strRegSlice adj = NULL;
			for (long i2 = 0; i2 != strGraphNodeIRLivePSize(outgoing); i2++) {
				if (outgoing[i2] == allColorNodes[i])
					continue;
				// search for adjacent
				__auto_type find = ptrMapregSliceGet(regsByLivenessNode, outgoing[i2]);

				// Insert if exists
				if (find)
					adj = strRegSliceAppendItem(adj, *find);
			}

			// Assign register to

			// Make dummy value to find type of variable
			struct IRValue value;
			value.type = IR_VAL_VAR_REF;
			value.value.var = graphNodeIRLiveValuePtr(allColorNodes[i])->ref;
			__auto_type type = IRValueGetType(&value);

			__auto_type regsForType = regGetForType(type);
			//
			// If type is F64,ignore ST(0) and ST(1),they are used for arithmetic
			//
			regsForType = strRegPRemoveItem(regsForType, &regX86ST0, (regCmpType)ptrPtrCmp);
			regsForType = strRegPRemoveItem(regsForType, &regX86ST1, (regCmpType)ptrPtrCmp);

			struct regSlice slice ;
			if(!value.value.var.var->inReg) {
					slice=color2Reg(adj, regsForType, allColorNodes[i], rand(), NULL, strGraphNodeIRLivePSize(allColorNodes));
			} else {
					slice.reg = value.value.var.var->inReg;
					slice.offset = 0;
					slice.widthInBits = slice.reg->size * 8;
					slice.type=value.value.var.var->type;
			}

			// Insert find
			if(!ptrMapregSliceGet(regsByLivenessNode, allColorNodes[i]))
					ptrMapregSliceAdd(regsByLivenessNode, allColorNodes[i], slice);
		}

		
		// Find noreg nodes and remove them from conflicts and mark them as spill.
		for(long n=0;n!=strGraphNodeIRLivePSize(allColorNodes);n++) {
				__auto_type node=allColorNodes[n];
				if(graphNodeIRLiveValuePtr(node)->ref.var->isNoreg) {
						ptrMapregSliceRemove(regsByLivenessNode, node);
				}
		}
		
		//Assert no adjacent conflicts 
		for(long n=0;n!=strGraphNodeIRPSize(allColorNodes);n++) {
				strGraphNodeIRLiveP adj CLEANUP(strGraphNodeIRLivePDestroy)=graphNodeIROutgoingNodes(allColorNodes[n]);
				for(long a=0;a!=strGraphNodeIRPSize(adj);a++) {
				check:;
						__auto_type curReg=ptrMapregSliceGet(regsByLivenessNode ,  allColorNodes[n]);
						__auto_type aReg=ptrMapregSliceGet(regsByLivenessNode ,  adj[a]);				
						
						if(!curReg)
								continue;
						if(!aReg)
								continue;
						if(regConflict(curReg->reg, aReg->reg)) {
								strGraphEdgeIRLiveP currDegO CLEANUP(strGraphEdgeIRLivePDestroy)=graphNodeIRLiveOutgoing(allColorNodes[n]);
								strGraphEdgeIRLiveP aRegO CLEANUP(strGraphEdgeIRLivePDestroy)=graphNodeIRLiveOutgoing(adj[a]);
								if(strGraphEdgeIRLivePSize(currDegO)>strGraphEdgeIRLivePSize(aRegO)) {
										ptrMapregSliceRemove(regsByLivenessNode, allColorNodes[n]);
										assert(!!ptrMapregSliceGet(regsByLivenessNode, allColorNodes[a]));
										printf("Spilling :%p,%p\n",allColorNodes[n],graphNodeIRLiveValuePtr(allColorNodes[n])->ref.var);
								}else {
										ptrMapregSliceRemove(regsByLivenessNode, adj[a]);
										assert(!ptrMapregSliceGet(regsByLivenessNode, adj[a]));
										printf("Spilling :%p,%p\n",adj[a],graphNodeIRLiveValuePtr(adj[a])->ref.var);
								}
								goto check;
						}
						assert(!regConflict(curReg->reg, aReg->reg));
				}
		}
		// Replace with registers
		replaceVarsWithRegisters(regsByLivenessNode, allColorNodes,&allNodes2);
		
	}

	//	removeDeadExpresions(start, liveVars);
	// debugShowGraphIR(start);
}
