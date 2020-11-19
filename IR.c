#include <IR.h>
#include <assert.h>
#include <base64.h>
#include <exprParser.h>
#include <subExprElim.h>
typedef int (*gnIRCmpType)(const graphNodeIR *, const graphNodeIR *);
#define ALLOCATE(x)                                                            \
	({                                                                           \
		typeof(&x) ptr = malloc(sizeof(x));                                        \
		memcpy(ptr, &x, sizeof(x));                                                \
		ptr;                                                                       \
	})
#define GRAPHN_ALLOCATE(x) ({ __graphNodeCreate(&x, sizeof(x), 0); })
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
	free(res);

	return retVal;
}
graphNodeIR createIntLit(int64_t lit) {
	struct IRNodeValue val;
	val.base.attrs = NULL;
	val.base.type = IR_VALUE;
	val.val.type = IR_VAL_INT_LIT;
	val.val.value.intLit.base = 10;
	val.val.value.intLit.type = INT_SLONG;
	val.val.value.intLit.value.sLong = lit;

	return GRAPHN_ALLOCATE(val);
}
graphNodeIR createBinop(graphNodeIR a, graphNodeIR b, enum IRNodeType type) {
	struct IRNodeBinop binop2;
	binop2.base.type = type;
	binop2.base.attrs = NULL;

	__auto_type retVal = GRAPHN_ALLOCATE(binop2);
	graphNodeIRConnect(a, retVal, IR_CONN_SOURCE_A);
	graphNodeIRConnect(b, retVal, IR_CONN_SOURCE_B);

	return retVal;
}
graphNodeIR createLabel() {
	struct IRNodeLabel lab;
	lab.base.attrs = NULL;
	lab.base.type = IR_LABEL;

	return GRAPHN_ALLOCATE(lab);
}
graphNodeIR createStmtStart() {
	struct IRNodeStatementStart start;
	start.base.attrs = NULL;
	start.base.type = IR_STATEMENT_START;
	start.end = NULL;

	return GRAPHN_ALLOCATE(start);
}
graphNodeIR createStmtEnd(graphNodeIR start) {
	struct IRNodeStatementStart end;
	end.base.attrs = NULL;
	end.base.type = IR_STATEMENT_START;

	__auto_type retVal = GRAPHN_ALLOCATE(end);
	((struct IRNodeStatementStart *)graphNodeIRValuePtr(start))->end = retVal;
	return retVal;
}
struct variable *createVirtVar(struct object *type) {
	struct variable var;
	var.name = NULL;
	var.refs = NULL;
	var.type = type;
	__auto_type alloced = ALLOCATE(var);

	struct IRVarRefs refs;
	refs.var.type = IR_VAR_VAR;
	refs.var.value.var = alloced;
	refs.refs = 0;

	__auto_type ptrStr = ptr2Str(alloced);
	mapIRVarRefsInsert(IRVars, ptrStr, refs);
	strCharDestroy(&ptrStr);

	return alloced;
}
graphNodeIR createVarRef(struct variable *var) {
	__auto_type ptrStr = ptr2Str(var);
	__auto_type find = mapIRVarRefsGet(IRVars, ptrStr);
	strCharDestroy(&ptrStr);
	// TODO add on not find
	assert(find);

	struct IRNodeValue val;
	val.base.attrs = NULL;
	val.base.type = IR_VALUE;
	val.val.type = IR_VAL_VAR_REF;

	val.val.value.var.SSANum = find->refs;
	val.val.value.var.var.type = IR_VAR_VAR;
	val.val.value.var.var.value.var = var;

	return GRAPHN_ALLOCATE(val);
}

graphNodeIR createJmp(graphNodeIR to) {
	struct IRNodeJump jmp;
	jmp.base.attrs = NULL;
	jmp.forward = -1;
	jmp.base.type = IR_JUMP;

	__auto_type retVal = GRAPHN_ALLOCATE(jmp);
	graphNodeIRConnect(to, retVal, IR_CONN_FLOW);

	return retVal;
}
graphNodeIR createValueFromLabel(graphNodeIR lab) {
	struct IRNodeValue val;
	val.base.attrs = NULL;
	val.base.type = IR_VALUE;
	val.val.type = __IR_VAL_LABEL;
	val.val.value.memLabel = lab;

	return GRAPHN_ALLOCATE(lab);
}
__thread mapIRVarRefs IRVars;
void IRNodeDestroy(void *item) { struct IRNode *node = item; }
static int ptrPtrCmp(const void *a, const void *b) {
	if (*(void **)a > *(void **)b)
		return 1;
	else if (*(void **)a < *(void **)b)
		return -1;
	else
		return 0;
}
strGraphNodeP getStatementNodes(const graphNodeIR stmtStart,
                                const graphNodeIR stmtEnd) {
	//
	// Visit all nodes from start->end node of statement
	//
	strGraphNodeIRP heads = graphNodeIROutgoingNodes(stmtStart);
	strGraphNodeIRP allNodes = NULL;
	while (strGraphNodeIRPSize(heads)) {
		strGraphNodeIRP unvisitedHeads = NULL;
		// Add unvisted to visited,
		for (size_t i = 0; i != strGraphNodeIRPSize(heads); i++) {
			if (NULL == strGraphNodeIRPSortedFind(allNodes, heads[i],
			                                      (gnIRCmpType)ptrPtrCmp)) {
				allNodes = strGraphNodeIRPSortedInsert(allNodes, heads[i],
				                                       (gnIRCmpType)ptrPtrCmp);
				unvisitedHeads = strGraphNodeIRPSortedInsert(unvisitedHeads, heads[i],
				                                             (gnIRCmpType)ptrPtrCmp);
			}
		}

		strGraphNodeIRPDestroy(&heads);
		heads = NULL;
		// Add outgoing heads to heads
		for (size_t i = 0; i != strGraphNodeIRPSize(unvisitedHeads); i++) {
			__auto_type newHeads = graphNodeIROutgoingNodes(unvisitedHeads[i]);
			for (size_t i = 0; i != strGraphNodeIRPSize(newHeads); i++) {
				// Dont add end node,we want to stop at end node
				if (graphNodeIRValuePtr(newHeads[i])->type == IR_STATEMENT_END)
					continue;

				// Dont re-insert same head
				if (NULL == strGraphNodeIRPSortedFind(heads, newHeads[i],
				                                      (gnIRCmpType)ptrPtrCmp))
					heads = strGraphNodeIRPSortedInsert(heads, newHeads[i],
					                                    (gnIRCmpType)ptrPtrCmp);
			}

			strGraphNodeIRPDestroy(&newHeads);
		}
	}

	return allNodes;
}
void initIR() {
	clearSubExprs();
	IRVars = mapIRVarRefsCreate();
}
graphNodeIR createFuncStart(const struct function *func) {
	struct IRNodeFuncStart start;
	start.base.attrs = NULL;
	start.base.type = IR_FUNC_START;
	start.end = NULL;
	start.func = (void *)func;

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
static struct object *typeU8P = NULL;
static void init() __attribute__((constructor));
static void init() { typeU8P = objectPtrCreate(&typeU8i); };
struct object *IRValueGetType(struct IRValue *node) {
	switch (node->type) {
	case IR_VAL_VAR_REF: {
		if (node->value.var.var.type == IR_VAR_VAR)
			return node->value.var.var.value.var->type;
		else if (node->value.var.var.type == IR_VAR_MEMBER)
			return assignTypeToOp((void *)node->value.var.var.value.member);
		return NULL;
	}
	case IR_VAL_STR_LIT:
		return typeU8P;
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
void IRInsertBefore(graphNodeIR insertBefore, graphNodeIR entry,
																				graphNodeIR exit, enum IRConnType connType) {
		__auto_type incoming = graphNodeIRIncoming(insertBefore);

		for (long i = 0; i != strGraphEdgeIRPSize(incoming); i++) {
				// Connect incoming to entry
				graphNodeIRConnect(entry, graphEdgeIRIncoming(incoming[i]),
																							*graphEdgeIRValuePtr(incoming[i]));

				// Disconnect for insertBefore
				graphEdgeIRKill(graphEdgeIRIncoming(incoming[i]), insertBefore, NULL, NULL,
																				NULL);
		}
		// Connect exit to insertBefore
		graphNodeIRConnect(exit, insertBefore, connType);
		strGraphEdgeIRPDestroy(&incoming);
}
void IRInsertAfter(graphNodeIR insertAfter, graphNodeIR entry,
                         graphNodeIR exit, enum IRConnType connType) {
	__auto_type outgoing = graphNodeIROutgoing(insertAfter);

	for (long i = 0; i != strGraphEdgeIRPSize(outgoing); i++) {
			// Connect outgoing to entry
			graphNodeIRConnect(exit, graphEdgeIROutgoing(outgoing[i]),
																						*graphEdgeIRValuePtr(outgoing[i]));

			// Disconnect for insertBefore
			graphEdgeIRKill(graphEdgeIROutgoing(outgoing[i]), insertAfter, NULL, NULL,
																			NULL);
	}

	graphNodeIRConnect(entry, insertAfter, connType);
	strGraphEdgeIRPDestroy(&outgoing);
}
