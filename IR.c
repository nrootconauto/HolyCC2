#include <IR.h>
#include <assert.h>
#include <base64.h>
#include <cleanup.h>
#include <exprParser.h>
#include <stdarg.h>
#include <stdint.h>
#include <IRTypeInference.h>
void *IR_ATTR_VARIABLE = "IS_VARIABLE";
typedef int (*gnIRCmpType)(const graphNodeIR *, const graphNodeIR *);
typedef int (*geIRCmpType)(const graphEdgeIR *, const graphEdgeIR *);
typedef int (*geMapCmpType)(const graphEdgeMapping *, const graphEdgeMapping *);
typedef int (*gnMapCmpType)(const graphNodeMapping *, const graphNodeMapping *);
#define ALLOCATE(x)                                                                                                                                                \
	({                                                                                                                                                               \
		typeof(&x) ptr = malloc(sizeof(x));                                                                                                                            \
		memcpy(ptr, &x, sizeof(x));                                                                                                                                    \
		ptr;                                                                                                                                                           \
	})
#define GRAPHN_ALLOCATE(x) ({ __graphNodeCreate(&x, sizeof(x), 0); })
static __thread ptrMapIRVarRefs IRVars;
static int ptrCmp(const void *a, const void *b) {
	if (a > b)
		return 1;
	else if (a < b)
		return -1;
	else
		return 0;
}
int IRAttrInsertPred(const struct IRAttr *a, const struct IRAttr *b) {
	const struct IRAttr *A = a, *B = b;
	return ptrCmp(A->name, B->name);
}
int IRAttrGetPred(const void *key, const struct IRAttr *b) {
	const struct IRAttr *B = b;
	return ptrCmp(key, B->name);
}
STR_TYPE_DEF(char, Char);
STR_TYPE_FUNCS(char, Char);
static strChar ptr2Str(const void *a) {
	__auto_type res = base64Enc((const char *)&a, sizeof(a));
	__auto_type retVal = strCharAppendData(NULL, res, strlen(res) + 1);

	return retVal;
}
static char *strClone(const char *text) {
	char *retVal = malloc(strlen(text) + 1);
	strcpy(retVal, text);

	return retVal;
}
graphNodeIR IRCreateSpillLoad(struct IRVar *var) {
	struct IRNodeSpill spill;
	spill.base.attrs = NULL;
	spill.base.type = IR_SPILL_LOAD;
	spill.item.type = IR_VAL_VAR_REF;
	spill.item.value.var = *var;

	return GRAPHN_ALLOCATE(spill);
}
graphNodeIR IRCreateRegRef(const struct regSlice *slice) {
	struct IRNodeValue val;
	val.base.attrs = NULL;
	val.base.type = IR_VALUE;
	val.val.type = IR_VAL_REG;
	val.val.value.reg = *slice;

	return GRAPHN_ALLOCATE(val);
}
graphNodeIR IRCreateFuncCall(graphNodeIR func, ...) {
	struct IRNodeFuncCall call;
	call.base.attrs = NULL;
	call.base.type = IR_FUNC_CALL;
	__auto_type retVal = GRAPHN_ALLOCATE(call);

	va_list args;
	va_start(args, func);
	for (long a = 0;; a++) {
		__auto_type arg = va_arg(args, graphNodeIR);
		if (!arg)
			break;

		assert(a <= 128);
		struct IRNodeFuncCall *call = (void *)graphNodeIRValuePtr(retVal);
		graphNodeIRConnect(arg, retVal, IR_CONN_FUNC_ARG_1 + a);
	}
	va_end(args);

	return retVal;
}
graphNodeIR IRCreateIntLit(int64_t lit) {
	struct IRNodeValue val;
	val.base.attrs = NULL;
	val.base.type = IR_VALUE;
	val.val.type = IR_VAL_INT_LIT;
	val.val.value.intLit.base = 10;
	val.val.value.intLit.type = INT_SLONG;
	val.val.value.intLit.value.sLong = lit;

	return GRAPHN_ALLOCATE(val);
}

graphNodeIR IRCreateStrLit(const char *text) {
	struct IRNodeValue val;
	val.base.attrs = NULL;
	val.base.type = IR_VALUE;
	val.val.type = IR_VAL_STR_LIT;
	val.val.value.strLit = __vecAppendItem(NULL, text,strlen(text)+1);

	return GRAPHN_ALLOCATE(val);
}
graphNodeIR IRCreateUnop(graphNodeIR a, enum IRNodeType type) {
	struct IRNodeBinop unop;
	unop.base.type = type;
	unop.base.attrs = NULL;

	__auto_type retVal = GRAPHN_ALLOCATE(unop);
	graphNodeIRConnect(a, retVal, IR_CONN_SOURCE_A);

	return retVal;
}
graphNodeIR IRCreateBinop(graphNodeIR a, graphNodeIR b, enum IRNodeType type) {
	struct IRNodeBinop binop2;
	binop2.base.type = type;
	binop2.base.attrs = NULL;

	__auto_type retVal = GRAPHN_ALLOCATE(binop2);
	graphNodeIRConnect(a, retVal, IR_CONN_SOURCE_A);
	graphNodeIRConnect(b, retVal, IR_CONN_SOURCE_B);

	return retVal;
}
graphNodeIR IRCreateLabel() {
	struct IRNodeLabel lab;
	lab.base.attrs = NULL;
	lab.base.type = IR_LABEL;

	return GRAPHN_ALLOCATE(lab);
}
graphNodeIR IRCreateStmtStart() {
	struct IRNodeStatementStart start;
	start.base.attrs = NULL;
	start.base.type = IR_STATEMENT_START;
	start.end = NULL;

	return GRAPHN_ALLOCATE(start);
}
graphNodeIR IRCreateStmtEnd(graphNodeIR start) {
	struct IRNodeStatementStart end;
	end.base.attrs = NULL;
	end.base.type = IR_STATEMENT_END;

	__auto_type retVal = GRAPHN_ALLOCATE(end);
	((struct IRNodeStatementStart *)graphNodeIRValuePtr(start))->end = retVal;
	return retVal;
}
struct parserVar *IRCreateVirtVar(struct object *type) {
	struct parserVar var;
	var.name = NULL;
	var.refs = NULL;
	var.type = type;
	var.isGlobal = 0;
	var.isTmp=1;
	__auto_type alloced = ALLOCATE(var);

	return alloced;
}
graphNodeIR IRCreateTypecast(graphNodeIR in, struct object *inType, struct object *outType) {
	struct IRNodeTypeCast cast;
	assert(inType);
	assert(outType);
	cast.base.attrs = NULL;
	cast.base.type = IR_TYPECAST;
	cast.in = inType;
	cast.out = outType;

	__auto_type retVal = GRAPHN_ALLOCATE(cast);
	graphNodeIRConnect(in, retVal, IR_CONN_SOURCE_A);

	return retVal;
}
static int isNotOfSSANum(const long *ssa, const graphNodeIR *node) {
	struct IRNodeValue *val = (void *)graphNodeIRValuePtr((graphNodeIR)*node);
	return val->val.value.var.SSANum != *ssa;
}
strGraphNodeIRP IRVarRefs(struct parserVar *var, long *SSANum) {
	__auto_type find = ptrMapIRVarRefsGet(IRVars, var);
	if (!find)
		return NULL;
	strGraphNodeIRP clone = strGraphNodeIRPClone(find->refs);
	if (!SSANum)
		return clone;
	return strGraphNodeIRPRemoveIf(clone, SSANum, (int (*)(const void *, const graphNodeIR *))isNotOfSSANum);
}
graphNodeIR IRCreateVarRef(struct parserVar *var) {
loop:;
	__auto_type find = ptrMapIRVarRefsGet(IRVars, var);
	if (!find) {
		struct IRVar ref;
		ref.SSANum = 0;
		ref.var = var;
		struct IRVarRefs refs;
		refs.refs = NULL;
		ptrMapIRVarRefsAdd(IRVars, var, refs);
		goto loop;
	}
	assert(find);

	struct IRNodeValue val;
	val.base.attrs = NULL;
	val.base.type = IR_VALUE;
	val.val.type = IR_VAL_VAR_REF;

	val.val.value.var.addressedByPtr = 0;
	val.val.value.var.SSANum = 0;
	val.val.value.var.var = var;

	__auto_type alloced = GRAPHN_ALLOCATE(val);
	find->refs = strGraphNodeIRPSortedInsert(find->refs, alloced, (gnIRCmpType)ptrCmp);
	return alloced;
}
graphNodeIR IRCreateValueFromLabel(graphNodeIR lab) {
	struct IRNodeValue val;
	val.base.attrs = NULL;
	val.base.type = IR_VALUE;
	val.val.type = __IR_VAL_LABEL;
	val.val.value.memLabel = lab;

	return GRAPHN_ALLOCATE(lab);
}
static int ptrPtrCmp(const void *a, const void *b) {
	if (*(void **)a > *(void **)b)
		return 1;
	else if (*(void **)a < *(void **)b)
		return -1;
	else
		return 0;
}
strGraphNodeP IRStatementNodes(const graphNodeIR stmtStart, const graphNodeIR stmtEnd) {
	//
	// Visit all nodes from start->end node of statement
	//
	strGraphEdgeIRP heads = graphNodeIROutgoing(stmtStart);
	strGraphNodeIRP allNodes = strGraphNodeIRPAppendItem(NULL, stmtStart);
	while (strGraphEdgeIRPSize(heads)) {
		strGraphNodeIRP unvisitedHeads = NULL;
		// Add unvisted to visited,
		for (size_t i = 0; i != strGraphEdgeIRPSize(heads); i++) {
			if (!IRIsExprEdge(*graphEdgeIRValuePtr(heads[i])))
				continue;

			__auto_type head = graphEdgeIROutgoing(heads[i]);
			if (NULL == strGraphNodeIRPSortedFind(allNodes, head, (gnIRCmpType)ptrPtrCmp)) {
				allNodes = strGraphNodeIRPSortedInsert(allNodes, head, (gnIRCmpType)ptrPtrCmp);
				unvisitedHeads = strGraphNodeIRPSortedInsert(unvisitedHeads, head, (gnIRCmpType)ptrPtrCmp);
			}
		}

		heads = NULL;
		// Add outgoing heads to heads
		for (size_t i = 0; i != strGraphNodeIRPSize(unvisitedHeads); i++) {
			__auto_type newHeads = graphNodeIROutgoing(unvisitedHeads[i]);

			for (size_t i = 0; i != strGraphEdgeIRPSize(newHeads); i++) {
				// Dont add end node,we want to stop at end node
				if (graphNodeIRValuePtr(graphEdgeIROutgoing(newHeads[i]))->type == IR_STATEMENT_END)
					continue;

				if (!IRIsExprEdge(*graphEdgeIRValuePtr(newHeads[i])))
					continue;

				// Dont re-insert same head
				if (NULL == strGraphEdgeIRPSortedFind(heads, newHeads[i], (geIRCmpType)ptrPtrCmp))
					heads = strGraphEdgeIRPSortedInsert(heads, newHeads[i], (geIRCmpType)ptrPtrCmp);
			}
		}
	}

	return allNodes;
}
void initIR() {
		IRVars = ptrMapIRVarRefsCreate();
}
graphNodeIR createFuncStart(const struct parserFunction *func) {
	struct IRNodeFuncStart start;
	start.base.attrs = NULL;
	start.base.type = IR_FUNC_START;
	start.end = NULL;

	return GRAPHN_ALLOCATE(start);
}
graphNodeIR createFuncEnd(graphNodeIR start) {
	struct IRNodeFuncEnd end;
	end.base.attrs = NULL;
	end.base.type = IR_FUNC_END;

	((struct IRNodeFuncStart *)graphNodeIRValuePtr(start))->end = start;
	return GRAPHN_ALLOCATE(end);
}
strGraphEdgeIRP IRGetConnsOfType(strGraphEdgeIRP conns, enum IRConnType type) {
	strGraphEdgeIRP retVal = NULL;
	for (long i = 0; i != strGraphEdgeIRPSize(conns); i++) {
		if (*graphEdgeIRValuePtr(conns[i]) == type)
			retVal = strGraphEdgeIRPAppendItem(retVal, conns[i]);
	}

	return retVal;
}
void IRMoveAttrsTo(graphNodeIR from, graphNodeIR to) {
	__auto_type fromNode = graphNodeIRValuePtr(from);
	__auto_type toNode = graphNodeIRValuePtr(to);
	toNode->attrs = fromNode->attrs;
	fromNode->attrs = NULL;
}
struct object *IRValueGetType(struct IRValue *node) {
	switch (node->type) {
	case IR_VAL_VAR_REF: {
		return node->value.var.var->type;
	}
	case IR_VAL_FLT_LIT:
		return &typeF64;
	case IR_VAL_STR_LIT:
		return objectPtrCreate(&typeU8i);
	case IR_VAL_INT_LIT:
		return &typeI64i;
	case __IR_VAL_MEM_FRAME:
		return node->value.__frame.type;
	case __IR_VAL_MEM_GLOBAL:
		return node->value.__global.symbol->type;
	case __IR_VAL_LABEL:
	case IR_VAL_REG:
	case IR_VAL_FUNC:
		//?
		return NULL;
	}
}
void IRInsertBefore(graphNodeIR insertBefore, graphNodeIR entry, graphNodeIR exit, enum IRConnType connType) {
	__auto_type incoming = graphNodeIRIncoming(insertBefore);

	for (long i = 0; i != strGraphEdgeIRPSize(incoming); i++) {
		// Connect incoming to entry
		graphNodeIRConnect(graphEdgeIRIncoming(incoming[i]), entry, *graphEdgeIRValuePtr(incoming[i]));

		// Disconnect for insertBefore
		graphEdgeIRKill(graphEdgeIRIncoming(incoming[i]), insertBefore, NULL, NULL, NULL);
	}
	// Connect exit to insertBefore
	graphNodeIRConnect(exit, insertBefore, connType);
}
void IRInsertAfter(graphNodeIR insertAfter, graphNodeIR entry, graphNodeIR exit, enum IRConnType connType) {
	__auto_type outgoing = graphNodeIROutgoing(insertAfter);

	for (long i = 0; i != strGraphEdgeIRPSize(outgoing); i++) {
		// Connect outgoing to entry
		graphNodeIRConnect(exit, graphEdgeIROutgoing(outgoing[i]), *graphEdgeIRValuePtr(outgoing[i]));

		// Disconnect for insertBefore
		graphEdgeIRKill(graphEdgeIROutgoing(outgoing[i]), insertAfter, NULL, NULL, NULL);
	}

	graphNodeIRConnect(insertAfter, entry, connType);
}
graphNodeIR IRCreateAssign(graphNodeIR in, graphNodeIR dst) {
	__auto_type baseType = objectBaseType(IRNodeType(dst));
	if (baseType->type == TYPE_CLASS || baseType->type == TYPE_UNION) {
		graphNodeIRConnect(IRCreateAddrOf(in), dst, IR_CONN_ASSIGN_FROM_PTR);
		return dst;
	}
	graphNodeIRConnect(in, dst, IR_CONN_DEST);
	return dst;
}
graphNodeIR IRCreateCondJmp(graphNodeIR cond, graphNodeIR t, graphNodeIR f) {
	struct IRNodeCondJump cJmp;
	cJmp.base.attrs = NULL;
	cJmp.base.type = IR_COND_JUMP;
	cJmp.cond = -1; // TODO

	graphNodeIR retVal = GRAPHN_ALLOCATE(cJmp);
	graphNodeIRConnect(cond, retVal, IR_CONN_SOURCE_A);
	graphNodeIRConnect(retVal, t, IR_CONN_COND_TRUE);
	graphNodeIRConnect(retVal, f, IR_CONN_COND_FALSE);

	return retVal;
}
int IRIsExprEdge(enum IRConnType type) {
	switch (type) {
	case IR_CONN_SOURCE_A:
	case IR_CONN_SOURCE_B:
	case IR_CONN_FUNC:
	case IR_CONN_SIMD_ARG:
	case IR_CONN_DEST:
	case IR_CONN_ASSIGN_FROM_PTR:
	case IR_CONN_FUNC_ARG_1 ... IR_CONN_FUNC_ARG_128:
	case IR_CONN_ARRAY_DIM_1 ... IR_CONN_ARRAY_DIM_16:
		return 1;
	default:
		return 0;
	}
}
static int exprEdgePred(const struct __graphNode *node, const struct __graphEdge *edge, const void *data) {
	__auto_type type = *graphEdgeIRValuePtr((void *)edge);
	return IRIsExprEdge(type);
}
static void addNode2List(struct __graphNode *node, void *data) {
	strGraphNodeIRP *nodes = data;
	*nodes = strGraphNodeIRPSortedInsert(*nodes, node, (gnIRCmpType)ptrPtrCmp);
}
static const char *IRAttrStmtStart = "STMT_START";
struct IRAttrStmtStart {
	struct IRAttr base;
	graphNodeIR node;
};
static int untilAssign(const struct __graphNode *node, const struct __graphEdge *edge, const void *data) {
	//
	// Edge may be a "virtual"(mapped edge from replace that has no value)
	// fail is edge value isnt present
	//
	__auto_type edgeValue = *graphEdgeIRValuePtr((void *)edge);
	if (!edgeValue)
		return 0;

	if (!IRIsExprEdge(edgeValue))
		return 0;

	strGraphEdgeIRP incoming = graphNodeIRIncoming(((graphNodeIR)node));

	if (strGraphEdgeIRPSize(incoming) == 1) {
		__auto_type type = graphEdgeIRValuePtr(incoming[0]);
		if (*type == IR_CONN_DEST)
			return 0;
	}

	return 1;
}
strGraphNodeIRP IRStmtNodes(graphNodeIR end) {
	strGraphNodeIRP starts = strGraphNodeIRPAppendItem(NULL, end);
	__auto_type start = IRStmtStart(end);
	graphNodeIRVisitBackward(end, &starts, exprEdgePred, addNode2List);
	if (!strGraphNodeIRPSortedFind(starts, start, (gnIRCmpType)ptrPtrCmp))
		starts = strGraphNodeIRPSortedInsert(starts, start, (gnIRCmpType)ptrPtrCmp);
	return starts;
}
static void transparentKill(graphNodeIR node) {
	__auto_type incoming = graphNodeIRIncoming(node);
	__auto_type outgoing = graphNodeIROutgoing(node);
	for (long i1 = 0; i1 != strGraphEdgeIRPSize(incoming); i1++)
		for (long i2 = 0; i2 != strGraphEdgeIRPSize(outgoing); i2++)
			graphNodeIRConnect(graphEdgeIRIncoming(incoming[i1]), graphEdgeIROutgoing(outgoing[i2]), IR_CONN_FLOW);

	graphNodeIRKill(&node, NULL, NULL);
}
void IRRemoveNeedlessLabels(graphNodeIR start) {
	strGraphNodeIRP all CLEANUP(strGraphNodeIRPDestroy) = graphNodeIRAllNodes(start);
	for (long i = 0; i != strGraphNodeIRPSize(all); i++) {
		if (graphNodeIRValuePtr(all[i])->type != IR_LABEL)
			continue;
		if (all[i] == start)
			continue;
		strGraphNodeIRP in CLEANUP(strGraphNodeIRPDestroy) = graphNodeIRIncomingNodes(all[i]);
		strGraphNodeIRP out CLEANUP(strGraphNodeIRPDestroy) = graphNodeIRIncomingNodes(all[i]);
		if (strGraphNodeIRPSize(in) == 1 && strGraphNodeIRPSize(out) == 1) {
		destroy:
			transparentKill(all[i]);
			continue;
		}

		if (strGraphNodeIRPSize(in) == 1) {
			if (graphNodeIRValuePtr(in[0])->type == IR_STATEMENT_START)
				goto destroy;
		}
	}
}
int IRIsDeadExpression(graphNodeIR end) {
	strGraphNodeIRP starts = strGraphNodeIRPAppendItem(NULL, end);
	graphNodeIRVisitBackward(end, &starts, untilAssign, addNode2List);

	int isDead = 1;
	// Check for function call with starts
	for (long i = 0; i != strGraphNodeIRPSize(starts); i++) {
		if (graphNodeIRValuePtr(starts[i])->type == IR_FUNC_CALL) {
			isDead = 0;
			break;
		}
	}

	return isDead;
}
void IRRemoveDeadExpression(graphNodeIR end, strGraphNodeP *removed) {
	__auto_type nodes = IRStmtNodes(end);
	strGraphNodeIRP starts = strGraphNodeIRPAppendItem(NULL, end);
	graphNodeIRVisitBackward(end, &starts, untilAssign, addNode2List);

	for (long i = 0; i != strGraphNodeIRPSize(starts); i++) {
		transparentKill(starts[i]);
	}

	if (removed)
		*removed = starts;
}
graphNodeIR IRStmtStart(graphNodeIR node) {
	strGraphNodeIRP starts = strGraphNodeIRPAppendItem(NULL, node);
	graphNodeIRVisitBackward(node, &starts, exprEdgePred, addNode2List);

	strGraphNodeIRP tops = NULL;
	// Find a top-level node that has node connections that constirute a statement
	for (long i = 0; i != strGraphNodeIRPSize(starts); i++) {
		strGraphNodeIRP starts2 = NULL;
		graphNodeIRVisitBackward(starts[i], &starts2, exprEdgePred, addNode2List);

		if (starts2 == NULL)
			tops = strGraphNodeIRPAppendItem(tops, starts[i]);
	}

	long len = strGraphNodeIRPSize(tops);
	if (len == 0)
		return NULL;

	// If epxression starts at one (expression node),use that
	if (len == 1)
		return tops[0];

	// Check if all statements have a common start
	__auto_type first = tops[0];
	strGraphNodeIRP firstIncoming = graphNodeIRIncomingNodes(first);
	for (long i = 1; i != len; i++) {
		__auto_type iIncoming = graphNodeIRIncomingNodes(first);

		// Ensure there is no difference between the incoming nodes
		__auto_type diff = strGraphNodeIRPSetDifference(strGraphNodeIRPClone(firstIncoming), iIncoming, (gnIRCmpType)ptrPtrCmp);
		if (0 != strGraphNodeIRPSize(diff)) {
			fprintf(stderr, "Dear programmer,all nodes of expression should share a "
			                "common start.\n");
			assert(0);
		}
	}

	// If one common node,return that
	if (1 == strGraphNodeIRPSize(firstIncoming))
		return firstIncoming[0];

	// Otherwise route all incoming "traffic" through a label
	__auto_type label = IRCreateLabel();
	for (long i = 0; i != len; i++) {
		__auto_type incoming = graphNodeIRIncoming(tops[i]);
		// Connect incoming to node
		for (long i2 = 0; i2 != strGraphEdgeIRPSize(incoming); i2++)
			graphNodeIRConnect(label, tops[i], *graphEdgeIRValuePtr(incoming[i2]));

		// Kill old connections
		strGraphNodeIRP disconnected CLEANUP(strGraphNodeIRPDestroy)=NULL;
		for (long i2 = 0; i2 != strGraphEdgeIRPSize(incoming); i2++) {
				__auto_type node=graphEdgeIRIncoming(incoming[i2]);
				if(strGraphNodeIRPSortedFind(disconnected, node, (gnIRCmpType)ptrPtrCmp))
						continue;
				
				graphEdgeIRKill(node, tops[i], NULL, NULL, NULL);
				disconnected=strGraphNodeIRPSortedInsert(disconnected,  node, (gnIRCmpType)ptrPtrCmp);
		}

		graphNodeIRConnect(label, tops[i], IR_CONN_FLOW);
	}

	return label;
}
int IRVarCmpIgnoreVersion(const struct IRVar *a, const struct IRVar *b) {
	return ptrCmp(a->var, b->var);
}
int IRVarCmp(const struct IRVar *a, const struct IRVar *b) {
	if (a->SSANum != b->SSANum) {
		if (a->SSANum > b->SSANum)
			return 1;
		else if (a->SSANum < b->SSANum)
			return -1;
	}

	return ptrCmp(a->var, b->var);
}
static strChar lexerInt2Str(struct lexerInt *i) {
	strChar retVal = strCharAppendItem(NULL, '\0');
	int64_t Signed;
	uint64_t Unsigned;
	const char digits[] = "0123456789";
	switch (i->type) {
	case INT_SLONG:
		Signed = i->value.sLong;
		goto dumpS;
	case INT_ULONG:;
		Unsigned = i->value.uLong;
		goto dumpU;
	}
dumpS:;
	int originalSigned = Signed;
	if (Signed < 0)
		Signed = -Signed;

	do {
		retVal = strCharAppendItem(retVal, '\0');
		memmove(retVal + 1, retVal, strlen(retVal));
		retVal[0] = digits[Signed % 10];
		Signed /= 10;
	} while (Signed != 0);

	if (originalSigned < 0) {
		retVal = strCharAppendItem(retVal, '\0');
		memmove(retVal + 1, retVal, strlen(retVal));
		retVal[0] = '-';
	}

	return retVal;
dumpU:
	do {
		retVal = strCharAppendItem(retVal, '\0');
		memmove(retVal + 1, retVal, strlen(retVal));
		retVal[0] = digits[Signed % 10];
		Unsigned /= 10;
	} while (Unsigned != 0);

	return retVal;
}
char *graphEdgeIR2Str(struct __graphEdge *edge) {
	switch (*graphEdgeIRValuePtr(edge)) {
	case IR_CONN_ARRAY_DIM_1...IR_CONN_ARRAY_DIM_16:
		return strClone("IR_CONN_ARRAY_DIM");
	case IR_CONN_NEVER_FLOW:
		return strClone("IR_CONN_NEVER_FLOW");
	case IR_CONN_CASE:
		return strClone("IR_CONN_CASE");
	case IR_CONN_DFT:
		return strClone("IR_CONN_DFT");
	case IR_CONN_ASSIGN_FROM_PTR:
		return strClone("IR_CONN_ASSIGN_FROM_PTR");
	case IR_CONN_FLOW:
		return strClone("IR_CONN_FLOW");
	case IR_CONN_SOURCE_A:
		return strClone("IR_CONN_SOURCE_A");
	case IR_CONN_SOURCE_B:
		return strClone("IR_CONN_SOURCE_B");
	case IR_CONN_DEST:
		return strClone("IR_CONN_DEST");
	case IR_CONN_COND:
		return strClone("IR_CONN_COND");
	case IR_CONN_COND_FALSE:
		return strClone("IR_CONN_COND_FALSE");
	case IR_CONN_COND_TRUE:
		return strClone("IR_CONN_COND_TRUE");
	case IR_CONN_FUNC:
		return strClone("IR_CONN_FUNC");
	case IR_CONN_SIMD_ARG:
		return strClone("IR_CONN_SIMD_ARG");
	case IR_CONN_FUNC_ARG_1 ... IR_CONN_FUNC_ARG_128: {
		int argi = *graphEdgeIRValuePtr(edge) - IR_CONN_FUNC_ARG_1 + 1;
		const char *fmt = "IR_CONN_FUNC_ARG(&i)";
		long c = snprintf(NULL, 0, fmt, argi);
		char buffer[c + 1];
		sprintf(buffer, fmt, argi);
		return strClone(buffer);
	}
	}

	return NULL;
}
graphNodeIR IRCreateReturn(graphNodeIR exp, graphNodeIR func) {
	struct IRNodeFuncReturn ret;
	ret.base.attrs = NULL;
	ret.base.type = IR_FUNC_RETURN;
	ret.exp = exp;

	__auto_type retVal = GRAPHN_ALLOCATE(ret);
	if (exp)
		graphNodeIRConnect(exp, retVal, IR_CONN_SOURCE_A);

	return retVal;
}
strChar cloneFromStr(const char *text) {
	return strCharAppendData(NULL, text, strlen(text + 1));
}
static strChar opToText(enum IRNodeType type) {
	switch (type) {
	case IR_LOR:
		return strClone("||");
	case IR_LSHIFT:
		return strClone("<<");
	case IR_RSHIFT:
		return strClone(">>");
	case IR_LT:
		return strClone("<");
	case IR_LXOR:
		return strClone("^^");
	case IR_MOD:
		return strClone("%");
	case IR_MULT:
		return strClone("*");
	case IR_NE:
		return strClone("!=");
	case IR_NEG:
		return strClone("-");
	case IR_POS:
		return strClone("+");
	case IR_POW:
		return strClone("`");
	case IR_ADD:
		return strClone("+");
	case IR_ADDR_OF:
		return strClone("ADDR-OF");
	case IR_BAND:
		return strClone("&");
	case IR_BNOT:
		return strClone("~");
	case IR_BOR:
		return strClone("|");
	case IR_BXOR:
		return strClone("^");
	case IR_COND_JUMP:
		return strClone("IF");
	case IR_DEC:
		return strClone("--");
	case IR_DERREF:
		return strClone("DEREF");
	case IR_DIV:
		return strClone("/");
	case IR_EQ:
		return strClone("==");
	case IR_FUNC_CALL:
		return strClone("FUNC-CALL");
	case IR_FUNC_RETURN:
		return strClone("RETURN");
	case IR_GE:
		return strClone(">=");
	case IR_GT:
		return strClone(">");
	case IR_INC:
		return strClone("++");
	case IR_LABEL:
		return strClone("LABEL");
	case IR_LAND:
		return strClone("&&");
	case IR_LE:
		return strClone("<=");
	case IR_LNOT:
		return strClone("!");
	case IR_SUB:
		return strClone("-");
	default:
		return NULL;
	}
}

#define COLOR_RED "red"
#define COLOR_ORANGE "orange"
#define COLOR_YELLOW "yellow"
#define COLOR_BLUE "blue"
#define COLOR_PURPLE "purple"
#define COLOR_PINK "pink"
static const char *rainbowColors[] = {
    COLOR_RED, COLOR_ORANGE, COLOR_YELLOW, COLOR_BLUE, COLOR_PURPLE, COLOR_PINK,
};
#define FROM_FORMAT(fmt, ...)                                                                                                                                      \
	({                                                                                                                                                               \
		long len = snprintf(NULL, 0, fmt, __VA_ARGS__);                                                                                                                \
		char __buffer[len + 1];                                                                                                                                          \
		sprintf(__buffer, fmt, __VA_ARGS__);                                                                                                                             \
		strClone(__buffer);                                                                                                                                              \
	})
MAP_TYPE_DEF(int, LabelNum);
MAP_TYPE_FUNCS(int, LabelNum);

static char *IRValue2GraphVizLabel(struct IRValue *val) {
	// Choose a label based on type
	switch (val->type) {
	case IR_VAL_FUNC: {
		const char *name = val->value.func->name;

		const char *format = "VAL FUNC:%s";
		return FROM_FORMAT(format, name);
	}
	case IR_VAL_INT_LIT: {
		__auto_type intStr = lexerInt2Str(&val->value.intLit);

		__auto_type labelText = FROM_FORMAT("VAL INT:%s", intStr);

		return labelText;
	}
	case IR_VAL_REG:
		if (val->value.reg.reg->name)
			return FROM_FORMAT("VAL REG:%s", val->value.reg.reg->name);
		break;
	case IR_VAL_STR_LIT: {
		const char *format = "VAL STR:\"%s\"";
		long len=__vecSize(val->value.strLit);
		char nulTerm[len+1];
		memset(nulTerm, 0, len+1);
		strncpy(nulTerm,(char*)val->value.strLit, len);
		return FROM_FORMAT(format, nulTerm);
	}
	case IR_VAL_VAR_REF: {
		strChar tmp = NULL;
		if (val->value.var.var->name)
			tmp = strClone(val->value.var.var->name);
		else {
			tmp = FROM_FORMAT("%p", val->value.var.var);
		}

		const char *format = "VAL VAR :%s-%li";
		__auto_type labelText = FROM_FORMAT(format, tmp, val->value.var.SSANum);

		return labelText;
	}
	case __IR_VAL_LABEL: {
		return strClone("LABEL"); // TODO
	}
	case __IR_VAL_MEM_FRAME: {
		return FROM_FORMAT("FRAME OFFSET:%li", val->value.__frame.offset);
	}
	case __IR_VAL_MEM_GLOBAL: {
		strChar tmp;
		if (val->value.__global.symbol->name)
			tmp = strClone(val->value.__global.symbol->name);
		else
			tmp = FROM_FORMAT("%p", val->value.__global.symbol);

		__auto_type labelText = FROM_FORMAT("SYM %s", tmp);
		return labelText;
	}
	}
	return NULL;
}

static void makeGVTerminalNode(mapGraphVizAttr *attrs) {
	mapGraphVizAttrInsert(*attrs, "shape", strClone("ellipse"));
	mapGraphVizAttrInsert(*attrs, "fillcolor", strClone(COLOR_BLUE));
}
static void makeGVProcessNode(mapGraphVizAttr *attrs) {
	mapGraphVizAttrInsert(*attrs, "shape", strClone("rectangle"));
	mapGraphVizAttrInsert(*attrs, "fillcolor", strClone(COLOR_RED));
}
static void makeGVDecisionNode(mapGraphVizAttr *attrs) {
	mapGraphVizAttrInsert(*attrs, "shape", strClone("diamond"));
	mapGraphVizAttrInsert(*attrs, "fillcolor", strClone(COLOR_YELLOW));
}
struct graphVizDataNode {
	char *(*nodeOverride)(graphNodeIR node, mapGraphVizAttr *attrs, const void *data);
	void *data;
	mapLabelNum *labelNums;
};
graphNodeIR IRCreateArrayDecl(struct parserVar *assignInto,struct object *type,strGraphNodeIRP dims) {
		struct IRNodeArrayDecl arr;
		arr.base.attrs=NULL;
		arr.base.type=IR_ARRAY_DECL;
		arr.itemType=type;
		__auto_type retVal=GRAPHN_ALLOCATE(arr);
		assert(strGraphNodeIRPSize(dims)<=16);
		for(long d=strGraphNodeIRPSize(dims)-1;d>=0;d--) {
				graphNodeIR from=NULL;
				//Create a variable to hold array dim if not an integer
				struct IRNodeValue *val=(void*)graphNodeIRValuePtr(dims[d]);
				if(val->base.type==IR_VALUE) {
						if(val->val.type==IR_VAL_INT_LIT) {
								from=dims[d];
								goto connect;
						}
				}
				__auto_type dimVar=IRCreateVirtVar(IRNodeType(dims[d]));
				__auto_type dimVarRef=IRCreateVarRef(dimVar);
				graphNodeIRConnect(dims[d], dimVarRef, IR_CONN_DEST);
				from=dimVarRef;
		connect:
				graphNodeIRConnect(from, retVal, IR_CONN_ARRAY_DIM_1+d);

				__auto_type arrClone=(struct objectArray*)type;
				assert(arrClone->base.type==TYPE_ARRAY);
				arrClone=(void*)objectArrayCreate(arrClone->type, NULL,from);
				type=(struct object*)arrClone;
		}
		assignInto->type=type;
		__auto_type assignNode=IRCreateVarRef(assignInto);
		
		graphNodeIRConnect(retVal, assignNode, IR_CONN_DEST);
		return assignNode;
}
static char *IRCreateGraphVizNode(const struct __graphNode *node, mapGraphVizAttr *attrs, const void *data) {
	const struct graphVizDataNode *data2 = data;

	// Try override first
	if (data2->nodeOverride) {
		long oldCount;
		mapGraphVizAttrKeys(*attrs, NULL, &oldCount);

		char *overrideName = data2->nodeOverride(*graphNodeMappingValuePtr((struct __graphNode *)node), attrs, data2->data);
		if (overrideName) {
		changed:
			return strClone(overrideName);
		}

		// attribute size change
		long newCount;
		mapGraphVizAttrKeys(*attrs, NULL, &newCount);
		if (newCount != oldCount)
			goto changed;
	}

	struct IRNode *value = graphNodeIRValuePtr(*graphNodeMappingValuePtr((struct __graphNode *)node));
	switch (value->type) {
	case IR_SOURCE_MAPPING: {
			struct IRNodeSourceMapping *mapping=(void*)value;
			FILE *f=fopen(mapping->fn, "r");
			char buffer [mapping->len+1];
			memset(buffer, 0, mapping->len+1);
			fseek(f,mapping->start,SEEK_SET);
			fread(buffer, mapping->len, 1, f);
			__auto_type retVal=FROM_FORMAT("MAPPING:%s", buffer);
			fclose(f);
			return retVal ;
	}
	case IR_ARRAY_DECL: {
			return strClone("ARRAY");
	}
	case IR_SPILL_LOAD: {
		struct IRNodeLoad *load = (void *)value;
		char *val = IRValue2GraphVizLabel(&load->item);
		const char *format = "SPILL: %s";
		long len = snprintf(NULL, 0, format, val);
		char buffer[len + 1];
		sprintf(buffer, format, val);

		return strClone(buffer);
	}
	case IR_CHOOSE: {
		makeGVDecisionNode(attrs);
		strChar message CLEANUP(strCharDestroy) = NULL;
		message = strCharAppendData(message, "CHOOSE", strlen("CHOOSE"));
		struct IRNodeChoose *choose = (void *)graphNodeIRValuePtr(*graphNodeMappingValuePtr((graphNodeMapping)node));
		for (long c = 0; c != strGraphNodeIRPSize(choose->canidates); c++) {
			char *msg = IRValue2GraphVizLabel(&((struct IRNodeValue *)graphNodeIRValuePtr(choose->canidates[c]))->val);
			message = strCharAppendItem(message, ',');
			message = strCharAppendData(message, msg, strlen(msg));
			free(msg);
		}
		message = strCharAppendItem(message, '\0');
		return strClone(message);
	}
	case IR_COND_JUMP:
		makeGVDecisionNode(attrs);
		return strClone("IF");
	case IR_FUNC_RETURN:
		makeGVTerminalNode(attrs);
		return strClone("RETURN");
	case IR_FUNC_START:
		return strClone("FUNC-START");
	case IR_FUNC_END:
		return strClone("FUNC-END");
	case IR_LABEL_LOCAL:
	case IR_LABEL: {
		__auto_type ptrStr = ptr2Str(node);
		__auto_type labelNum = *mapLabelNumGet(*data2->labelNums, ptrStr);

		__auto_type str = FROM_FORMAT("LABEL: #%i", labelNum);
		__auto_type str2 = strClone(str);

		mapGraphVizAttrInsert(*attrs, "color", strClone(COLOR_ORANGE));
		mapGraphVizAttrInsert(*attrs, "shape", strClone("rectangle"));

		return str2;
	}
	case IR_JUMP_TAB: {
		makeGVDecisionNode(attrs);
		return strClone("JUMP-TABLE");
	}
	case IR_STATEMENT_END:
		makeGVTerminalNode(attrs);
		return strClone("STMT-END");
	case IR_VALUE: {
		strChar attrs CLEANUP(strCharDestroy) = NULL;
		__auto_type var = llIRAttrFind(value->attrs, IR_ATTR_VARIABLE, IRAttrGetPred);
		if (var) {
			struct IRAttrVariable *varAttr = (void *)llIRAttrValuePtr(var);
			struct IRValue tmp;
			tmp.type = IR_VAL_VAR_REF;
			tmp.value.var = varAttr->var;
			char *varStr = IRValue2GraphVizLabel(&tmp);
			char *nugget = FROM_FORMAT("[%s]", varStr);
			attrs = strCharAppendData(attrs, nugget, strlen(nugget));
			free(varStr);
		}
		attrs = strCharAppendItem(attrs, '\0');
		char *tmp = IRValue2GraphVizLabel(&((struct IRNodeValue *)value)->val);
		char *retVal = FROM_FORMAT("%s%s", tmp, attrs);
		free(tmp);
		return retVal;
	}
	case IR_TYPECAST: {
		struct IRNodeTypeCast *cast = (void *)value;
		char *typeNameIn = object2Str(cast->in);
		char *typeNameOut = object2Str(cast->out);

		__auto_type retVal = FROM_FORMAT("TYPECAST(%s->%s)", typeNameIn, typeNameOut);

		makeGVProcessNode(attrs);
		return retVal;
	}
	case IR_STATEMENT_START:
		makeGVTerminalNode(attrs);
		return strClone("STMT-START");
	case IR_SUB_SWITCH_START_LABEL:
		makeGVTerminalNode(attrs);
		return strClone("SUB-SWITCH START");
	case IR_MEMBERS_ADDR_OF: {
		const char *header = "ADDR-OF ";
		strChar message CLEANUP(strCharDestroy) = strCharAppendData(NULL, header, strlen(header));
		struct IRNodeMembersAddrOf *mems = (void *)value;
		strGraphEdgeIRP in CLEANUP(strGraphEdgeIRPDestroy) = graphNodeIRIncoming((graphNodeIR)node);
		__auto_type currentType = IRNodeType(graphEdgeIRIncoming(in[0]));
		for (long m = 0; m != strObjectMemberSize(mems->members); m++) {
			if (currentType->type == TYPE_PTR) {
				message = strCharAppendData(message, "->", 2);
			} else {
				message = strCharAppendItem(message, '.');
			}
			message = strCharAppendData(message, mems->members[m].name, strlen(mems->members[m].name));
		}

		return strClone(message);
	}
	case IR_MEMBERS: {
		goto fail;
		strChar message CLEANUP(strCharDestroy) = NULL;
		struct IRNodeMembers *mems = (void *)value;
		strGraphEdgeIRP in CLEANUP(strGraphEdgeIRPDestroy) = graphNodeIRIncoming((graphNodeIR)node);
		__auto_type currentType = IRNodeType(graphEdgeIRIncoming(in[0]));
		for (long m = 0; m != strObjectMemberSize(mems->members); m++) {
			if (currentType->type == TYPE_PTR) {
				message = strCharAppendData(message, "->", 2);
			} else {
				message = strCharAppendItem(message, '.');
			}
			message = strCharAppendData(message, mems->members[m].name, strlen(mems->members[m].name));
		}

		return strClone(message);
	}
	case IR_INC:
	case IR_DEC:
	case IR_ADD:
	case IR_SUB:
	case IR_POS:
	case IR_NEG:
	case IR_MULT:
	case IR_MOD:
	case IR_DIV:
	case IR_POW:
	case IR_LAND:
	case IR_LXOR:
	case IR_LOR:
	case IR_LNOT:
	case IR_BNOT:
	case IR_BAND:
	case IR_BXOR:
	case IR_BOR:
	case IR_LSHIFT:
	case IR_RSHIFT:
	case IR_GT:
	case IR_LT:
	case IR_GE:
	case IR_LE:
	case IR_EQ:
	case IR_NE:
	case IR_FUNC_CALL:
	case IR_ADDR_OF:
	case IR_DERREF:;
		// default:;
		// Check for operator type
		__auto_type retVal = opToText(value->type);
		if (retVal) {
			makeGVProcessNode(attrs);
			return retVal;
		}
		goto fail;
	case IR_FUNC_ARG: {
		struct IRNodeFuncArg *arg = (void *)value;
		const char *fmt = "FUNC-ARG:%li";
		long len = snprintf(NULL, 0, fmt, arg->argIndex);
		char buffer[len + 1];
		sprintf(buffer, fmt, arg->argIndex);
		return strClone(buffer);
	}
	case IR_X86_INST: {
		struct IRNodeX86Inst *inst = (void *)value;
		const char *fmt = "INST:%s";
		long len = snprintf(NULL, 0, fmt, inst->name);
		char buffer[len + 1];
		sprintf(buffer, fmt, inst->name);
		return strClone(buffer);
	}
	default:
	fail:
		// Unkown type
		return strClone("\?\?\?!");
	}
}
#define COLOR_GREY "grey"
struct graphVizDataEdge {
	char *(*edgeOverride)(graphEdgeIR node, mapGraphVizAttr *attrs, const void *data);
	void *data;
	mapLabelNum *labelNums;
};
static char *IRCreateGraphVizEdge(const struct __graphEdge *__edge, mapGraphVizAttr *attrs, const void *data) {
	graphEdgeIR edge = *graphEdgeMappingValuePtr((struct __graphEdge *)__edge);
	if (!edge)
		return NULL;

	// Frist check ovveride
	const struct graphVizDataEdge *data2 = data;
	if (data2->edgeOverride) {
		// old count
		long oldCount;
		mapGraphVizAttrKeys(*attrs, NULL, &oldCount);

		char *name = data2->edgeOverride(edge, attrs, data);
		// Check if name provided
		if (name) {
		changed:
			return strClone(name);
		}

		// Check if new attrs added
		long newCount;
		mapGraphVizAttrKeys(*attrs, NULL, &newCount);
		if (oldCount != newCount)
			goto changed;
	}

	__auto_type edgeVal = graphEdgeIRValuePtr(edge);
	if (edgeVal == NULL) {
		mapGraphVizAttrInsert(*attrs, "color", strClone(COLOR_GREY));
		return NULL;
	}

	switch (*edgeVal) {
	case IR_CONN_ARRAY_DIM_1...IR_CONN_ARRAY_DIM_16:
			mapGraphVizAttrInsert(*attrs, "color", strClone("dim"));
			return NULL;
	case IR_CONN_ASSIGN_FROM_PTR:
		mapGraphVizAttrInsert(*attrs, "color", strClone("orange"));
		return strClone("Assign from Ptr");
	case IR_CONN_NEVER_FLOW:
		mapGraphVizAttrInsert(*attrs, "style", strClone("dashed"));
		return NULL;
	case IR_CONN_COND:
	case IR_CONN_FLOW:
		mapGraphVizAttrInsert(*attrs, "color", strClone("black"));
		return NULL;
	case IR_CONN_DEST:
		mapGraphVizAttrInsert(*attrs, "color", strClone(COLOR_PINK));
		return NULL;
	case IR_CONN_COND_TRUE:
		mapGraphVizAttrInsert(*attrs, "color", strClone(COLOR_RED));
		return NULL;
	case IR_CONN_COND_FALSE:
		mapGraphVizAttrInsert(*attrs, "color", strClone(COLOR_BLUE));
		return NULL;
	case IR_CONN_FUNC:
		mapGraphVizAttrInsert(*attrs, "color", strClone(rainbowColors[0]));
		return NULL;
	case IR_CONN_FUNC_ARG_1 ... IR_CONN_FUNC_ARG_128: {
		struct IRNodeFuncCall *funcNode = (void *)graphNodeIRValuePtr(graphEdgeIROutgoing((struct __graphEdge *)edge));
		// Look at func node and find index of edge
		if (funcNode) {
			if (funcNode->base.type == IR_FUNC_CALL) {
				__auto_type incomingNode = graphEdgeIRIncoming((struct __graphEdge *)edge);

				// Look for index of edge
				int i = *graphEdgeIRValuePtr(edge) - IR_CONN_FUNC_ARG_1;

				// Color based on rainbow
				__auto_type color = rainbowColors[i % (sizeof(rainbowColors) / sizeof(*rainbowColors))];
				mapGraphVizAttrInsert(*attrs, "color", strClone(color));

				// Label name is index
				__auto_type str = FROM_FORMAT("Arg %i", i);
				__auto_type retVal = strClone(str);

				return retVal;
			}
		}

		// Isnt connected to a  func-call node
		mapGraphVizAttrInsert(*attrs, "color", strClone(COLOR_GREY));
		return NULL;
	}
	case IR_CONN_SIMD_ARG:
		// TODO
		return NULL;
	case IR_CONN_SOURCE_A: {
		// Check if result is binop(if has both IR_CONN_SOURCE_A and
		// IR_CONN_SOURCE_B).
		__auto_type outNode = graphEdgeIROutgoing((struct __graphEdge *)edge);
		__auto_type incoming = graphNodeIRIncoming(outNode);
		__auto_type filtered = IRGetConnsOfType(incoming, IR_CONN_SOURCE_B);
		int isBinop = strGraphEdgeIRPSize(filtered) != 0;

		// If binop,color red and label "A"
		if (isBinop) {
			mapGraphVizAttrInsert(*attrs, "color", strClone(COLOR_RED));
			return strClone("A");
		}

		return NULL;
	}
	case IR_CONN_SOURCE_B:
		mapGraphVizAttrInsert(*attrs, "color", strClone(COLOR_BLUE));
		return strClone("B");
	case IR_CONN_CASE:
	case IR_CONN_DFT:
		// TODO
		return NULL;
	}

	return strClone("\?\?\?!");
}

void IRGraphMap2GraphViz(graphNodeMapping graph, const char *title, const char *fn,
                         char *(*nodeLabelOverride)(graphNodeIR node, mapGraphVizAttr *attrs, const void *data),
                         char *(*edgeLabelOverride)(graphEdgeIR node, mapGraphVizAttr *attrs, const void *data), const void *dataNodes, const void *dataEdge) {
	__auto_type allNodes = graphNodeMappingAllNodes(graph);

	//
	// Number labels
	//
	mapLabelNum labelNums = mapLabelNumCreate();

	long labelCount = 0;
	for (long i = 0; i != strGraphNodeIRPSize(allNodes); i++) {
		if (!*graphNodeMappingValuePtr(allNodes[i]))
			continue;
		struct IRNode *nodeVal = graphNodeIRValuePtr(*graphNodeMappingValuePtr(allNodes[i]));
		if (nodeVal->type == IR_LABEL) {
			// Regsiter a unique label number
			char *key = ptr2Str(allNodes[i]);
			mapLabelNumInsert(labelNums, key, labelCount + 1);
			// inc label count
			labelCount++;
		}
	}

	// Create edge data tuple
	struct graphVizDataEdge edgeData;
	edgeData.data = (void *)dataEdge;
	edgeData.edgeOverride = edgeLabelOverride;
	edgeData.labelNums = &labelNums;

	// Create node data tuple
	struct graphVizDataNode nodeData;
	nodeData.data = (void *)dataEdge;
	nodeData.nodeOverride = nodeLabelOverride;
	nodeData.labelNums = &labelNums;

	FILE *dumpTo = fopen(fn, "w");
	graph2GraphViz(dumpTo, graph, title, IRCreateGraphVizNode, IRCreateGraphVizEdge, &nodeData, &edgeData);
	fclose(dumpTo);
}
static graphNodeIR __cloneNode(ptrMapGraphNode mappings, graphNodeIR node, enum IRCloneMode mode, const void *data);
static void __cloneNodeCopyConnections(ptrMapGraphNode mappings, graphNodeIR from, graphNodeIR connectTo, enum IRCloneMode mode, const void *data) {
	int ignoreAssigns = 0;
	switch (mode) {
	case IR_CLONE_UP_TO:
	case IR_CLONE_EXPR: {
		ignoreAssigns = 0;
		goto cloneExpressions;
	}
	case IR_CLONE_EXPR_UNTIL_ASSIGN: {
		ignoreAssigns = 1;
		goto cloneExpressions;
	}
	case IR_CLONE_NODE:
		return;
	}
cloneExpressions:;
	__auto_type incoming = graphNodeIRIncoming(from);
	for (long i = 0; i != strGraphEdgeIRPSize(incoming); i++) {
		__auto_type type = *graphEdgeIRValuePtr(incoming[i]);
		if (IRIsExprEdge(type)) {
			// If ignore assigns,ignore IR_CONN_DEST
			if (ignoreAssigns && type == IR_CONN_DEST)
				continue;

			// Clone
			__auto_type newNode = __cloneNode(mappings, graphEdgeIRIncoming(incoming[i]), mode, data);

			graphNodeIRConnect(newNode, connectTo, type);
		}
	}

	return;
}
static graphNodeIR __cloneNode(ptrMapGraphNode mappings, graphNodeIR node, enum IRCloneMode mode, const void *data) {
	__auto_type find = ptrMapGraphNodeGet(mappings, node);
	if (find)
		return *find;

	__auto_type ir = graphNodeIRValuePtr(node);
	switch (ir->type) {
	case IR_ARRAY_DECL: {
			struct IRNodeArrayDecl *arr=(void*)ir;
			return GRAPHN_ALLOCATE(*arr);
	}
	case IR_FUNC_CALL: {
		// Copy argument vector
		__auto_type reference = (struct IRNodeFuncCall *)graphNodeIRValuePtr(node);
		struct IRNodeFuncCall clone;
		clone.base.attrs = NULL;
		clone.base.type = IR_FUNC_CALL;

		__auto_type newNode = GRAPHN_ALLOCATE(clone);

		// Quit if we are where we want to bne
		if (mode == IR_CLONE_UP_TO) {
			// Check if we are to stop at node
			if (NULL != strGraphNodeIRPSortedFind((strGraphNodeIRP)data, node, (gnIRCmpType)ptrPtrCmp))
				return newNode;
		}

		if (mode == IR_CLONE_UP_TO) {
			if (NULL == strGraphNodeIRPSortedFind((strGraphNodeIRP)data, node, (gnIRCmpType)ptrPtrCmp)) {
				strGraphEdgeIRP incoming CLEANUP(strGraphEdgeIRPDestroy) = graphNodeIRIncoming(node);
				for (long i = 0; i != strGraphEdgeIRPSize(incoming); i++) {
					__auto_type newNode2 = __cloneNode(mappings, graphEdgeIRIncoming(incoming[i]), mode, data);
					graphNodeIRConnect(newNode2, newNode, *graphEdgeIRValuePtr(incoming[i]));
				}
			}
		} else if (mode == IR_CLONE_EXPR) {
			__auto_type incoming = graphNodeIRIncoming(node);
			// Connect all incoming that arg args,those will be done later(IN ORDER)
			for (long i = 0; i != strGraphEdgeIRPSize(incoming); i++) {
				// if not func arg
				if (*graphEdgeIRValuePtr(incoming[i]) < IR_CONN_FUNC_ARG_1)
					continue;
				if (*graphEdgeIRValuePtr(incoming[i]) > IR_CONN_FUNC_ARG_128)
					continue;

				__auto_type newNode2 = __cloneNode(mappings, graphEdgeIRIncoming(incoming[i]), mode, data);
				graphNodeIRConnect(newNode2, newNode, *graphEdgeIRValuePtr(incoming[i]));
			}

			// Now do function arguments
			__auto_type newNodeCall = (struct IRNodeFuncCall *)graphNodeIRValuePtr(newNode);
			for (long i = 0; i != strGraphEdgeIRPSize(incoming); i++) {
				__auto_type newNode2 = __cloneNode(mappings, graphEdgeIRIncoming(incoming[i]), mode, data);
				graphNodeIRConnect(newNode2, newNode, *graphEdgeIRValuePtr(incoming[i]));
			}
		}

		// Copy connections
		__cloneNodeCopyConnections(mappings, node, newNode, mode, data);

		// Register node
		ptrMapGraphNodeAdd(mappings, node, newNode);

		return newNode;
	}
	case IR_CHOOSE: {
		__auto_type nodeValue = (struct IRNodeChoose *)graphNodeIRValuePtr(node);

		struct IRNodeChoose choose;
		choose.base.attrs = NULL;
		choose.base.type = IR_CHOOSE;
		choose.canidates = strGraphNodeIRPClone(nodeValue->canidates);
		__auto_type retVal = GRAPHN_ALLOCATE(choose);

		// Quit if we are at where we want to be
		if (IR_CLONE_UP_TO) {
			// Check if we are to stop at node
			if (NULL != strGraphNodeIRPSortedFind((strGraphNodeIRP)data, node, (gnIRCmpType)ptrPtrCmp))
				return retVal;
		}

		// Copy connections

		__cloneNodeCopyConnections(mappings, node, retVal, mode, data);

		// Register node
		ptrMapGraphNodeAdd(mappings, node, retVal);

		return retVal;
	}
	case IR_ADD:
	case IR_VALUE:
	case IR_ADDR_OF:
	case IR_BAND:
	case IR_BNOT:
	case IR_BOR:
	case IR_BXOR:
	case IR_DEC:
	case IR_DERREF:
	case IR_DIV:
	case IR_EQ:
	case IR_GE:
	case IR_GT:
	case IR_INC:
	case IR_LAND:
	case IR_COND_JUMP:
	case IR_FUNC_END:
	case IR_FUNC_START:
	case IR_FUNC_RETURN:
	case IR_LABEL:
	case IR_LE:
	case IR_LNOT:
	case IR_LOR:
	case IR_LSHIFT:
	case IR_LT:
	case IR_LXOR:
	case IR_MOD:
	case IR_NE:
	case IR_MULT:
	case IR_NEG:
	case IR_POS:
	case IR_POW:
	case IR_RSHIFT:
	case IR_STATEMENT_END:
	case IR_SPILL_LOAD:
	case IR_STATEMENT_START:
	case IR_SUB:
	case IR_SUB_SWITCH_START_LABEL:
	case IR_TYPECAST: {
		// Clone value(ignore atttibute)
		__auto_type retVal = __graphNodeCreate(graphNodeIRValuePtr(node), graphNodeValueSize(node), 0);
		graphNodeIRValuePtr(retVal)->attrs = NULL;

		// Quit if we are where we want to be
		if (IR_CLONE_UP_TO) {
			// Check if we are to stop at node
			if (NULL != strGraphNodeIRPSortedFind((strGraphNodeIRP)data, node, (gnIRCmpType)ptrPtrCmp))
				return retVal;
		}

		// Copy connections
		__cloneNodeCopyConnections(mappings, node, retVal, mode, data);

		// Register node
		ptrMapGraphNodeAdd(mappings, node, retVal);

		return retVal;
	}
	case IR_SIMD: {
		assert(0); // TODO
	}
	case IR_JUMP_TAB: {
		__auto_type table = (struct IRNodeJumpTable *)graphNodeIRValuePtr(node);
		struct IRNodeJumpTable newTable;
		newTable.base.attrs = NULL;
		newTable.base.type = IR_JUMP_TAB;
		newTable.labels = strIRTableRangeClone(table->labels);
		newTable.startIndex = table->startIndex;

		__auto_type retVal = GRAPHN_ALLOCATE(newTable);

		// Quit if we are where we want to be
		if (mode == IR_CLONE_UP_TO) {
			// Check if we are to stop at node
			if (NULL != strGraphNodeIRPSortedFind((strGraphNodeIRP)data, node, (gnIRCmpType)ptrPtrCmp))
				return retVal;
		}

		if (mode != IR_CLONE_NODE) {
			// Copy connections
			__cloneNodeCopyConnections(mappings, node, retVal, mode, data);
		}
		// Register node
		ptrMapGraphNodeAdd(mappings, node, retVal);

		return retVal;
	}
	case IR_LABEL_LOCAL: {
		struct IRNodeLabelLocal *loc = (void *)graphNodeIRValuePtr(node);
		__auto_type retVal = GRAPHN_ALLOCATE(*loc);
		// Register node
		ptrMapGraphNodeAdd(mappings, node, retVal);
		return retVal;
	}
	case IR_FUNC_ARG: {
		struct IRNodeFuncArg *arg = (void *)graphNodeIRValuePtr(node);
		__auto_type retVal = GRAPHN_ALLOCATE(*arg);
		// Quit if we are where we want to be
		if (mode == IR_CLONE_UP_TO) {
			// Check if we are to stop at node
			if (NULL != strGraphNodeIRPSortedFind((strGraphNodeIRP)data, node, (gnIRCmpType)ptrPtrCmp))
				return retVal;
		}

		if (mode != IR_CLONE_NODE) {
			// Copy connections
			__cloneNodeCopyConnections(mappings, node, retVal, mode, data);
		}
	}
	}
}
graphNodeIR IRCloneNode(graphNodeIR node, enum IRCloneMode mode, ptrMapGraphNode *mappings) {
	__auto_type mappings2 = ptrMapGraphNodeCreate();
	__auto_type retVal = __cloneNode(mappings2, node, mode, NULL);

	if (mappings) {
		*mappings = mappings2;
	} else {
		ptrMapGraphNodeDestroy(mappings2, NULL);
	}

	return retVal;
}
graphNodeIR IRCloneUpTo(graphNodeIR node, strGraphNodeIRP to, ptrMapGraphNode *mappings) {
	__auto_type mappings2 = ptrMapGraphNodeCreate();
	__auto_type retVal = __cloneNode(mappings2, node, IR_CLONE_UP_TO, NULL);

	if (mappings) {
		*mappings = mappings2;
	} else {
		ptrMapGraphNodeDestroy(mappings2, NULL);
	}

	return retVal;
}
graphNodeIR IRCreateSourceMapping(const char *fileName,long start,long len) {
		struct IRNodeSourceMapping mapping;
		mapping.fn=fileName;
		mapping.start=start;
		mapping.len=len;
		mapping.base.type=IR_SOURCE_MAPPING;
		mapping.base.attrs=NULL;
		return GRAPHN_ALLOCATE(mapping);
}
graphNodeIR IREndOfExpr(graphNodeIR node) {
	if (graphNodeIRValuePtr(node)->type == IR_LABEL) {
		strGraphEdgeIRP out CLEANUP(strGraphEdgeIRPDestroy) = graphNodeIROutgoing(node);
		int flowOut = 0;
		for (long e = 0; e != strGraphEdgeIRPSize(out); e++)
			if (*graphEdgeIRValuePtr(out[e]) == IR_CONN_FLOW)
				flowOut++;
		// Labels can be used to start statements that have 2 or more operands
		if (flowOut < 2)
			return node;
		// Label may contain no-flow edge
		if (flowOut != strGraphEdgeIRPSize(out))
			return node;
		node = graphEdgeIROutgoing(out[0]);
	}
	for (;;) {
	loop:;
		strGraphEdgeIRP outgoing;
		outgoing = graphNodeIROutgoing(node);
		for (long i = 0; i != strGraphEdgeIRPSize(outgoing); i++) {
			if (IRIsExprEdge(*graphEdgeIRValuePtr(outgoing[i]))) {
				__auto_type node2 = graphEdgeIROutgoing(outgoing[i]);
				// Ensure end is not a conditional jump or jump table
				switch (graphNodeIRValuePtr(node2)->type) {
				case IR_COND_JUMP:
				case IR_JUMP_TAB:
				case IR_FUNC_RETURN:
					goto end;
				default:;
				}
				node = node2;
				goto loop;
			}
		}
	end:
		// No expression edges so is end of expression
		return node;
	}
}
int IRIsOperator(graphNodeIR node) {
	switch (graphNodeIRValuePtr(node)->type) {
	case IR_ADD:
	case IR_ADDR_OF:
	case IR_BAND:
	case IR_BNOT:
	case IR_BOR:
	case IR_BXOR:
	case IR_DEC:
	case IR_DERREF:
	case IR_DIV:
	case IR_EQ:
	case IR_FUNC_CALL:
	case IR_GE:
	case IR_GT:
	case IR_INC:
	case IR_LAND:
	case IR_LE:
	case IR_LNOT:
	case IR_LOR:
	case IR_LSHIFT:
	case IR_LT:
	case IR_LXOR:
	case IR_MOD:
	case IR_MULT:
	case IR_NE:
	case IR_NEG:
	case IR_POS:
	case IR_POW:
	case IR_RSHIFT:
	case IR_SIMD:
	case IR_SUB:
	case IR_TYPECAST:
		return 1;
	default:
		return 0;
	}
}
graphNodeIR IRCreateFuncArg(struct object *type, long funcIndex) {
	struct IRNodeFuncArg arg;
	arg.argIndex = funcIndex;
	arg.base.attrs = NULL;
	arg.base.type = IR_FUNC_ARG;
	arg.type = type;

	return GRAPHN_ALLOCATE(arg);
}
graphNodeIR IRObjectArrayScale(struct objectArray *arr) {
		strGraphNodeIRP scales CLEANUP(strGraphNodeIRPDestroy)=NULL;
		for(arr=(struct objectArray*)arr->type;arr->base.type==TYPE_ARRAY;arr=(void*)arr->type) {
				assert(arr->dimIR);
				//Should have been assigned into a variable or be a constant
				scales=strGraphNodeIRPAppendItem(scales, IRCloneNode(arr->dimIR, IR_CLONE_NODE, NULL));
		}
		scales=strGraphNodeIRPAppendItem(scales, IRCreateIntLit(objectSize((struct object*)arr,NULL)));
		if(strGraphNodeIRPSize(scales)==1)
				return scales[0];
		graphNodeIR scale =IRCreateBinop(scales[0], scales[1], IR_MULT);
		for(long s=2;s<strGraphNodeIRPSize(scales);s++)
				scale=IRCreateBinop(scale, scales[s], IR_MULT);
		return scale;
}
graphNodeIR IRCreateAddrOf(graphNodeIR input) {
	if (graphNodeIRValuePtr(input)->type == IR_MEMBERS) {
		struct IRNodeMembers *mems = (void *)graphNodeIRValuePtr(input);
		__auto_type members = strObjectMemberClone(mems->members);
		struct IRNodeMembersAddrOf addrOf;
		addrOf.base.attrs = NULL;
		addrOf.base.type = IR_MEMBERS_ADDR_OF;
		addrOf.members = members;
		__auto_type retVal = GRAPHN_ALLOCATE(addrOf);
		strGraphNodeIRP toReplace CLEANUP(strGraphNodeIRPDestroy) = strGraphNodeIRPAppendItem(NULL, input);
		graphIRReplaceNodes(toReplace, retVal, NULL, (void (*)(void *))IRNodeDestroy);
		return retVal;
	} else if (graphNodeIRValuePtr(input)->type == IR_DERREF) {
		strGraphEdgeIRP in CLEANUP(strGraphEdgeIRPDestroy) = graphNodeIRIncoming(input);
		strGraphEdgeIRP inSource CLEANUP(strGraphEdgeIRPDestroy) = IRGetConnsOfType(inSource, IR_CONN_SOURCE_A);
		__auto_type inNode = graphEdgeIRIncoming(inSource[0]);
		graphNodeIRKill(&input, (void (*)(void *))IRNodeDestroy, NULL);
		return inNode;
	}
	struct IRNode node;
	node.attrs = NULL;
	node.type = IR_ADDR_OF;
	__auto_type retVal = GRAPHN_ALLOCATE(node);
	graphNodeIRConnect(input, retVal, IR_CONN_SOURCE_A);
	return retVal;
}
graphNodeIR IRCreateDerref(graphNodeIR input) {
	if (graphNodeIRValuePtr(input)->type == IR_MEMBERS_ADDR_OF) {
		struct IRNodeMembersAddrOf *addrOf = (void *)graphNodeIRValuePtr(input);
		__auto_type clone = strObjectMemberClone(addrOf->members);
		struct IRNodeMembers members;
		members.base.attrs = NULL;
		members.base.type = IR_MEMBERS;
		members.members = clone;
		strGraphNodeIRP toReplace CLEANUP(strGraphNodeIRPDestroy) = strGraphNodeIRPAppendItem(NULL, input);
		__auto_type retVal = GRAPHN_ALLOCATE(members);
		graphIRReplaceNodes(toReplace, retVal, NULL, (void (*)(void *))IRNodeDestroy);
		return retVal;
	} else if (graphNodeIRValuePtr(input)->type == IR_ADDR_OF) {
		strGraphEdgeIRP incoming CLEANUP(strGraphEdgeIRPDestroy) = graphNodeIRIncoming(input);
		strGraphEdgeIRP inSource CLEANUP(strGraphEdgeIRPDestroy) = IRGetConnsOfType(incoming, IR_CONN_SOURCE_A);
		__auto_type sourceNode = graphEdgeIRIncoming(inSource[0]);
		graphNodeIRKill(&input, (void (*)(void *))IRNodeDestroy, NULL);
		return sourceNode;
	}
	struct IRNode node;
	node.attrs = NULL;
	node.type = IR_DERREF;
	__auto_type retVal = GRAPHN_ALLOCATE(node);
	graphNodeIRConnect(input, retVal, IR_CONN_SOURCE_A);
	return retVal;
}
graphNodeIR IRCreateMemberAccess(graphNodeIR input, const char *name) {
	__auto_type type = IRNodeType(input);
	__auto_type originalType = type;
	if (type->type == TYPE_PTR) {
		struct objectPtr *ptr = (void *)type;
		type = ptr->type;
	}
	struct objectMember *member = NULL;
	if (type->type == TYPE_CLASS) {
		struct objectClass *cls = (void *)type;
		for (long i = 0; i != strObjectMemberSize(cls->members); i++)
			if (0 == strcmp(cls->members[i].name, name))
				member = &cls->members[i];

	} else if (type->type == TYPE_UNION) {
		struct objectUnion *un = (void *)type;
		for (long i = 0; i != strObjectMemberSize(un->members); i++)
			if (0 == strcmp(un->members[i].name, name))
				member = &un->members[i];
	}

	if (graphNodeIRValuePtr(input)->type == IR_MEMBERS) {
		struct IRNodeMembers *members = (void *)graphNodeIRValuePtr(input);
		members->members = strObjectMemberAppendItem(members->members, *member);
		return input;
	}

	assert(member);

	struct IRNodeMembers memberNode;
	memberNode.base.attrs = NULL;
	memberNode.base.type = IR_MEMBERS;
	memberNode.members = strObjectMemberAppendItem(NULL, *member);
	__auto_type memNode = GRAPHN_ALLOCATE(memberNode);
	graphNodeIRConnect(input, memNode, IR_CONN_SOURCE_A);
	return memNode;
}
#include <IRFilter.h>
static int isNotExprEdge(const void *data, const graphEdgeIR *edge) {
	return !IRIsExprEdge(*graphEdgeIRValuePtr(*edge));
}
PTR_MAP_FUNCS(graphNodeIR, graphNodeIR, AffectedNodes);
static void __IRInsertNodesBetweenExprs(graphNodeIR expr, ptrMapAffectedNodes affected, int (*pred)(graphNodeIR, const void *), const void *predData) {
	if (ptrMapAffectedNodesGet(affected, expr))
		return;

	if (pred)
		if (!pred(expr, predData))
			return;

	strGraphEdgeIRP in CLEANUP(strGraphEdgeIRPDestroy) = graphNodeIRIncoming(expr);
	in = strGraphEdgeIRPRemoveIf(in, NULL, isNotExprEdge);
	for (long i = 0; i != strGraphEdgeIRPSize(in); i++) {
		__auto_type node = graphEdgeIRIncoming(in[i]);
		// Recursivly do the same for incoming expression
		__IRInsertNodesBetweenExprs(node, affected, pred, predData);
		// Ignore existing assigns(unless assigning into an array or ptr)
		if (*graphEdgeIRValuePtr(in[i]) == IR_CONN_DEST) {
			__auto_type type = graphNodeIRValuePtr(expr)->type;
			if (type == IR_DERREF)
				;
			else
				continue;
		}

		struct IRNodeValue *nodeValue = (void *)graphNodeIRValuePtr(node);
		if (nodeValue->base.type == IR_VALUE)
			continue;

		// Not a value so insert a variable after the operation(the varaible will be assigned into)
		__auto_type tmp = IRCreateVirtVar(IRNodeType(node));
		__auto_type tmpRef = IRCreateVarRef(tmp);
		IRInsertAfter(node, tmpRef, tmpRef, IR_CONN_DEST);
	}
	ptrMapAffectedNodesAdd(affected, expr, NULL);
}
static int IsEndOfExprNode(graphNodeIR node, const void *data) {
	strGraphEdgeIRP in CLEANUP(strGraphEdgeIRPDestroy) = graphNodeIRIncoming(node);
	strGraphEdgeIRP out CLEANUP(strGraphEdgeIRPDestroy) = graphNodeIROutgoing(node);
	in = strGraphEdgeIRPRemoveIf(in, NULL, isNotExprEdge);
	out = strGraphEdgeIRPRemoveIf(out, NULL, isNotExprEdge);
	if (strGraphEdgeIRPSize(in) || strGraphEdgeIRPSize(out)) {
		if (IREndOfExpr(node) == node)
			return 1;
	}

	return 0;
}
graphNodeIR IRCreateArrayAccess(graphNodeIR arr,graphNodeIR index) {
		struct object *type=IRNodeType(arr);
		if(type->type==TYPE_ARRAY) {
				struct objectArray *arrayType=(void*)type;
				__auto_type ptr=IRCreateBinop(arr,  IRCreateBinop(index , IRObjectArrayScale(arrayType), IR_MULT) , IR_ADD);
				return IRCreateDerref(ptr);
		}
		__auto_type ptr=IRCreateBinop(arr,  IRCreateBinop(index , IRCreateIntLit(objectSize(type, NULL)), IR_MULT) , IR_ADD);
		return IRCreateDerref(ptr);
}
void IRInsertNodesBetweenExprs(graphNodeIR expr, int (*pred)(graphNodeIR, const void *), const void *predData) {
	__auto_type filtered = IRFilter(expr, IsEndOfExprNode, NULL);
	//IRPrintMappedGraph(filtered);
	strGraphNodeMappingP all CLEANUP(strGraphNodeMappingPDestroy) = graphNodeMappingAllNodes(filtered);
	__auto_type affected = ptrMapAffectedNodesCreate();
	for (long i = 0; i != strGraphNodeMappingPSize(all); i++) {
		// Assign types to  nodes of expression
		IRNodeType(*graphNodeMappingValuePtr(all[i]));
		__IRInsertNodesBetweenExprs(*graphNodeMappingValuePtr(all[i]), affected, pred, predData);
	}
	ptrMapAffectedNodesDestroy(affected, NULL);
	graphNodeMappingKillGraph(&filtered, NULL, NULL);
}
void IRPrintMappedGraph(graphNodeMapping map) {
	const char *name = tmpnam(NULL);
	IRGraphMap2GraphViz(map, "viz", name, NULL, NULL, NULL, NULL);
	char buffer[1024];
	sprintf(buffer, "sleep 0.1 &&dot -Tsvg %s > /tmp/dot.svg && firefox /tmp/dot.svg &", name);

	system(buffer);
}
graphNodeIR IRCreatePtrRef(graphNodeIR ptr) {
	struct IRNodePtrRef derref;
	derref.base.attrs = NULL;
	derref.base.type = IR_DERREF;
	__auto_type node = GRAPHN_ALLOCATE(derref);
	graphNodeIRConnect(ptr, node, IR_CONN_SOURCE_A);

	return node;
}
STR_TYPE_DEF(struct IRVar, IRVar);
STR_TYPE_FUNCS(struct IRVar, IRVar);
void IRMarkPtrVars(graphNodeIR start) {
	strGraphNodeIRP allNodes CLEANUP(strGraphNodeIRPDestroy) = graphNodeIRAllNodes(start);
	for (long i = 0; strGraphNodeIRPSize(allNodes); i++) {
		__auto_type type = graphNodeIRValuePtr(allNodes[i])->type;
		if (type == IR_ADDR_OF) {
			strGraphEdgeIRP in CLEANUP(strGraphEdgeIRPDestroy) = graphNodeIRIncoming(allNodes[i]);
			strGraphEdgeIRP source CLEANUP(strGraphEdgeIRPDestroy) = IRGetConnsOfType(in, IR_CONN_SOURCE_A);
			if (graphNodeIRValuePtr(graphEdgeIRIncoming(source[0]))) {
				//
				// Check if points to variable or member of variable
				// If so mark it as not being able to be put in a register as it will need to exist in memory
				//
				graphNodeIR node = graphEdgeIRIncoming(source[0]);
				for (;;) {
					struct IRNode *nodeValue = graphNodeIRValuePtr(node);
					if (nodeValue->type == IR_VALUE) {
						struct IRNodeValue *val = (void *)nodeValue;
						if (val->val.type == IR_VAL_VAR_REF) {
							goto foundVariable;
						}
					} else if (nodeValue->type == IR_MEMBERS) {
						strGraphEdgeIRP in CLEANUP(strGraphEdgeIRPDestroy) = graphNodeIRIncoming(allNodes[i]);
						strGraphEdgeIRP source CLEANUP(strGraphEdgeIRPDestroy) = IRGetConnsOfType(in, IR_CONN_SOURCE_A);
						node = graphEdgeIRIncoming(source[0]);
					}
					continue;
				foundVariable:;
					struct IRNodeValue *value = (void *)nodeValue;
					value->val.value.var.addressedByPtr = 0;
				}
			}

			__auto_type start = IRStmtStart(allNodes[i]);
		}
	}
}
graphNodeIR IRCreateFloat(double value) {
	struct IRNodeValue nv;
	nv.base.attrs = NULL;
	nv.base.type = IR_VALUE;
	nv.val.type = IR_VAL_FLT_LIT;
	nv.val.value.fltLit = value;
	return GRAPHN_ALLOCATE(nv);
}
void IRAttrReplace(graphNodeIR node, llIRAttr attribute) {
	__auto_type key = llIRAttrValuePtr(attribute)->name;
	__auto_type find = llIRAttrFind(graphNodeIRValuePtr(node)->attrs, key, IRAttrGetPred);
	if (find) {
		graphNodeIRValuePtr(node)->attrs = llIRAttrRemove(find);
		__auto_type valuePtr = llIRAttrValuePtr(find);
		valuePtr->destroy(valuePtr);
		llIRAttrDestroy(&find, NULL);
	}

	llIRAttrInsert(graphNodeIRValuePtr(node)->attrs, attribute, IRAttrInsertPred);
	graphNodeIRValuePtr(node)->attrs = attribute;
}
void IRNodeDestroy(struct IRNode *node) {
	for (__auto_type attr = llIRAttrFirst(node->attrs); attr != NULL; attr = llIRAttrNext(attr)) {
		__auto_type attrValue = llIRAttrValuePtr(attr);
		if (attrValue->destroy) {
			attrValue->destroy(attrValue);
		}
	}
	if (node->type == IR_VALUE) {
		struct IRNodeValue *valueNode = (void *)node;
		if (valueNode->val.type == IR_VAL_VAR_REF) {
			__auto_type find = ptrMapIRVarRefsGet(IRVars, valueNode->val.value.var.var);
			assert(find);
			for (long i = 0; i != strGraphNodeIRPSize(find->refs); i++) {
				if (graphNodeIRValuePtr(find->refs[i]) == node) {
					memmove(&find->refs[i], &find->refs[i + 1], (strGraphNodeIRPSize(find->refs) - i - 1) * sizeof(find->refs[i]));
					find->refs = strGraphNodeIRPPop(find->refs, NULL); // Decrease size by 1
					break;
				}
			}
		}
	}
}
graphNodeIR IRCreateJumpTable() {
	struct IRNodeJumpTable table;
	table.base.attrs = NULL;
	table.base.type = IR_JUMP_TAB;
	table.count = 0;
	table.startIndex = -1;
	table.labels = NULL;
	__auto_type tableNode = GRAPHN_ALLOCATE(table);
	return tableNode;
}
graphNodeIR IRCreateGlobalVarRef(struct parserVar *var) {
	assert(var->isGlobal);
	struct IRNodeValue val;
	val.base.attrs = NULL;
	val.base.type = IR_VALUE;
	val.val.type = __IR_VAL_MEM_GLOBAL;
	val.val.value.__global.symbol = var;
	return GRAPHN_ALLOCATE(val);
}
graphNodeIR IRCreateFrameAddress(long offset, struct object *obj) {
	struct IRNodeValue val;
	val.base.attrs = NULL;
	val.base.type = IR_VALUE;
	val.val.type = __IR_VAL_MEM_FRAME;
	val.val.value.__frame.offset = offset;
	val.val.value.__frame.type = obj;
	return GRAPHN_ALLOCATE(val);
}
static __thread graphNodeIR argEdgeSortNode = NULL;
static int argEdgeSortPrec(graphEdgeIR edge) {
	__auto_type type = *graphEdgeIRValuePtr(edge);
	switch (type) {
	case IR_CONN_SOURCE_A:
		return 0;
	case IR_CONN_SOURCE_B:
		return 1;
	case IR_CONN_DEST:
		return 2;
	case IR_CONN_COND:
		return 0;
	case IR_CONN_FUNC_ARG_1 ... IR_CONN_FUNC_ARG_128: {
		return 1 + type - IR_CONN_FUNC_ARG_1;
		case IR_CONN_ARRAY_DIM_1...IR_CONN_ARRAY_DIM_16:
				return 1+type-IR_CONN_ARRAY_DIM_1;
	}
	case IR_CONN_FUNC:
		return 0;
	default:
		assert(0);
		return 0;
	}
}
static int argEdgeSort(const void *a, const void *b) {
	const graphEdgeIR *A = a, *B = b;
	return argEdgeSortPrec(*A) - argEdgeSortPrec(*B);
}
strGraphEdgeIRP IREdgesByPrec(graphNodeIR node) {
	argEdgeSortNode = node;
	strGraphEdgeIRP in CLEANUP(strGraphEdgeIRPDestroy) = graphNodeIRIncoming(node);
	strGraphEdgeIRP inExpr = NULL;
	for (long i = 0; i != strGraphEdgeIRPSize(in); i++)
		if (IRIsExprEdge(*graphEdgeIRValuePtr(in[i])))
			inExpr = strGraphEdgeIRPAppendItem(inExpr, in[i]);

	qsort(inExpr, strGraphEdgeIRPSize(inExpr), sizeof(*inExpr), argEdgeSort);
	argEdgeSortNode = NULL;
	return inExpr;
}
static int irEdgeValEq(void *a,void *b) {
		return *(enum IRConnType *)a==*(enum IRConnType *)b;
}
void IRRemoveNeverFlows(graphNodeIR node) {
		strGraphNodeIRP allNodes CLEANUP(strGraphNodeIRPDestroy)=graphNodeIRAllNodes(node);
		for(long n=0;n!=strGraphNodeIRPSize(allNodes);n++) {
				strGraphEdgeIRP out CLEANUP(strGraphEdgeIRPDestroy)=graphNodeIROutgoing(allNodes[n]);
				strGraphEdgeIRP outNeverFlow CLEANUP(strGraphEdgeIRPDestroy)=IRGetConnsOfType(out, IR_CONN_NEVER_FLOW);
				for(long e=0;e!=strGraphEdgeIRPSize(outNeverFlow);e++) {
						enum IRConnType del=IR_CONN_NEVER_FLOW;
						graphEdgeIRKill(allNodes[n], graphEdgeIROutgoing(outNeverFlow[e]), &del, irEdgeValEq, NULL);
				}
		}
		//Reaches all accessible nodes
		strGraphNodeIRP visited CLEANUP(strGraphNodeIRPDestroy)=strGraphNodeIRPAppendItem(NULL, node);
		graphNodeIRVisitForward(node,&visited, NULL,  addNode2List);
		strGraphNodeIRP unacc CLEANUP(strGraphNodeIRPDestroy)=strGraphNodeIRPSetDifference(strGraphNodeIRPClone(allNodes), visited, (gnIRCmpType)ptrPtrCmp);
		for(long n=0;n!=strGraphNodeIRPSize(unacc);n++)
				graphNodeIRKill(&unacc[n], (void(*)(void*))IRNodeDestroy, NULL);
}
