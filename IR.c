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
			graphNodeIRConnect(graphEdgeIRIncoming(incoming[i]),entry,
		                   *graphEdgeIRValuePtr(incoming[i]));

		// Disconnect for insertBefore
		graphEdgeIRKill(graphEdgeIRIncoming(incoming[i]), insertBefore, NULL, NULL,
		                NULL);
	}
	// Connect exit to insertBefore
	graphNodeIRConnect(exit, insertBefore, connType);
	strGraphEdgeIRPDestroy(&incoming);
}
void IRInsertAfter(graphNodeIR insertAfter, graphNodeIR entry, graphNodeIR exit,
                   enum IRConnType connType) {
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
graphNodeIR createAssign(graphNodeIR in, graphNodeIR dst) {
	graphNodeIRConnect(in, dst, IR_CONN_DEST);
	return dst;
}
graphNodeIR createCondJmp(graphNodeIR cond, graphNodeIR t, graphNodeIR f) {
	struct IRNodeCondJump cJmp;
	cJmp.base.attrs = NULL;
	cJmp.base.type = IR_COND_JUMP;
	cJmp.cond = -1; // TODO

	graphNodeIR retVal = GRAPHN_ALLOCATE(cJmp);
	graphNodeIRConnect(cond, retVal, IR_CONN_SOURCE_A);
	graphNodeIRConnect(retVal, t, IR_CONN_COND_TRUE);
	graphNodeIRConnect(retVal, f, IR_CONN_COND_TRUE);

	return retVal;
}
static int exprEdgePred(const struct __graphNode *node,
                        const struct __graphEdge *edge, const void *data) {
		__auto_type type=*graphEdgeIRValuePtr((void *)edge);
	switch (type) {
	case IR_CONN_SOURCE_A:
	case IR_CONN_SOURCE_B:
	case IR_CONN_FUNC_ARG:
	case IR_CONN_FUNC:
	case IR_CONN_SIMD_ARG:
	case IR_CONN_DEST:
		return 1;
	default:
		return 0;
	}
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
graphNodeIR IRGetStmtStart(graphNodeIR node) {
	strGraphNodeIRP starts = NULL;
	graphNodeIRVisitBackward(node, &starts, exprEdgePred, addNode2List);
	if (starts == NULL)
		return node;

	// Find a top-level node that has node connections that constirute a statement
	for (long i = 0; i != strGraphNodeIRPSize(starts); i++) {
		strGraphNodeIRP starts2 __attribute__((cleanup(strGraphNodeIRPDestroy))) =
		    NULL;
		graphNodeIRVisitBackward(starts[i], &starts2, exprEdgePred, addNode2List);

		if (starts2 == NULL)
			return starts[i];
	}

	return NULL;
}
int IRVarCmp(const struct IRVar *a, const struct IRVar *b) {
	if (0 != a->type - b->type)
		return a->type - b->type;

	if (a->type == IR_VAR_VAR) {
		return ptrCmp(a->value.var, b->value.var);
	} else {
		// TODO implement
	}

	return 0;
}

/*void IRStmtBlockFromTailNode(graphNodeIR tail,graphNodeIR *enter,graphNodeIR
*exit) {
    //Check if already a statement
    if(graphNodeIRValuePtr(tail)->type==IR_STATEMENT_END) {
        if(exit)
            *exit=tail;

        __auto_type start=((struct
IRNodeStatementEnd*)graphNodeIRValuePtr(tail))->start; if(enter) *enter=start;
        return;
    }

    strGraphNodeIRP starts=NULL;
    graphNodeIRVisitBackward(tail, &starts, exprEdgePred,   addNode2List);
    assert(starts!=NULL);

    struct IRNodeStatementEnd end;
    end.base.attrs=NULL;
    end.base.type=IR_STATEMENT_START;
    __auto_type endNode=GRAPHN_ALLOCATE(end);

    struct IRNodeStatementStart start;
    start.base.attrs=NULL;
    start.base.type=IR_STATEMENT_START;
    start.end=NULL;
    __auto_type startNode=GRAPHN_ALLOCATE(start);

    ((struct IRNodeStatementEnd*)graphNodeIRValuePtr(endNode))->start=startNode;

    //
    // Move incoming nodes
    //
    for(long i=0;i!=strGraphNodeIRPSize(starts);i++) {
        __auto_type in=graphNodeIRIncoming(starts[i]);
        for(long i2=0;i2!=strGraphEdgeIRPSize(in);i2++) {
            switch(*graphEdgeIRValuePtr(in[i2])) {
            case IR_CONN_COND:
                //?
                assert(0);
            case IR_CONN_DEST: {
                //TODO clone dest int block
                break;
            }
            case IR_CONN_COND_FALSE:
            case IR_CONN_COND_TRUE:
            case IR_CONN_FLOW:
                graphNodeIRConnect(graphEdgeIRIncoming(in[i2]),startNode,
IR_CONN_FLOW); graphEdgeIRKill(graphEdgeIRIncoming(in[i2]), starts[i],
NULL,NULL, NULL); break;
                //Alreadt habdled in  exprEdgePred
            case IR_CONN_FUNC:
            case IR_CONN_FUNC_ARG:
            case IR_CONN_SIMD_ARG:
            case IR_CONN_SOURCE_A:
            case IR_CONN_SOURCE_B:
                assert(0);
                break;
            }
        }
    }

    //Move exits out stmt from node to endNode
    strGraphEdgeIRP exits=graphNodeIROutgoing(tail);
    for(long i=0;i!=strGraphEdgeIRPSize(exits);i++) {
        //Make "clone" of edge from endNode to destination
        graphNodeIRConnect(endNode, graphEdgeIROutgoing(exits[i]),
*graphEdgeIRValuePtr(exits[i]));
        //Remove edge
        graphEdgeIRKill(tail, graphEdgeIROutgoing(exits[i]), NULL, NULL, NULL);
    }

    //Coonect node to endNode
    graphNodeIRConnect(tail, endNode, IR_CONN_FLOW);

    if(enter)
        *enter=startNode;
    if(exit)
        *exit=endNode;

    //
    //Assign attribute to nodes pointing to stmt start.
    //
    struct IRAttrStmtStart attr;
    attr.base.name=(void*)IRAttrStmtStart;
    attr.node=startNode;
    for(long i=0;i!=strGraphNodeIRPSize(starts);i++) {
        __auto_type find=llIRAttrFind(graphNodeIRValuePtr(starts[i])->attrs,
IRAttrStmtStart,IRAttrGetPred); if(find) {
            //Remove old attr
            llIRAttrRemove(find);
            llIRAttrDestroy(&find, NULL);
        }
        //Add new attr pointing to current stmt
        __auto_type llNew= __llCreate(&attr, sizeof(attr));
        llIRAttrInsert(graphNodeIRValuePtr(starts[i])->attrs, llNew,
IRAttrInsertPred); graphNodeIRValuePtr(starts[i])->attrs=llNew;
    }
}
    */
