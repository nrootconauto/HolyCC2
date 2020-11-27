#include <IR.h>
#include <assert.h>
#include <base64.h>
#include <exprParser.h>
#include <subExprElim.h>
#include <stdint.h>
typedef int (*gnIRCmpType)(const graphNodeIR *, const graphNodeIR *);
typedef int (*geMapCmpType)(const graphEdgeMapping *, const graphEdgeMapping *);
typedef int (*gnMapCmpType)(const graphNodeMapping *, const graphNodeMapping *);
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
static char *strClone(const char *text) {
	char *retVal = malloc(strlen(text) + 1);
	strcpy(retVal, text);

	return retVal;
}
graphNodeIR createFuncCall(graphNodeIR func,...) {
		struct IRNodeFuncCall call;
		call.base.attrs=NULL;
		call.base.type=IR_FUNC_CALL;
		call.incomingArgs=NULL;
		
		va_list args;
		va_start(args, func);
		for(;;) {
				__auto_type arg=va_arg(args, graphNodeIR);
				if(!arg)
						break;

				call.incomingArgs=strGraphNodeIRPAppendItem(call.incomingArgs, arg);
		}
		va_end(args);

		return GRAPHN_ALLOCATE(call);
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

graphNodeIR createStrLit(const char *text) {
		struct IRNodeValue val;
		val.base.attrs=NULL;
		val.base.type=IR_VALUE;
		val.val.type=IR_VAL_STR_LIT;
		val.val.value.strLit=strClone(text);

		return GRAPHN_ALLOCATE(val);
}
graphNodeIR createUnop(graphNodeIR a, enum IRNodeType type) {
		struct IRNodeBinop unop;
		unop.base.type = type;
		unop.base.attrs = NULL;

		__auto_type retVal = GRAPHN_ALLOCATE(unop);
	graphNodeIRConnect(a, retVal, IR_CONN_SOURCE_A);

	return retVal;
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
graphNodeIR createTypecast(graphNodeIR in,struct object *inType,struct object *outType) {
		struct IRNodeTypeCast cast;
		cast.base.attrs=NULL;
		cast.base.type=IR_TYPECAST;
		cast.in=inType;
		cast.out=outType;

		__auto_type retVal=GRAPHN_ALLOCATE(cast);
		graphNodeIRConnect(in, retVal, IR_CONN_SOURCE_A);

		return retVal;
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
	graphNodeIRConnect(retVal,to, IR_CONN_FLOW);

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
		graphNodeIRConnect(graphEdgeIRIncoming(incoming[i]), entry,
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
	__auto_type type = *graphEdgeIRValuePtr((void *)edge);
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
static strChar lexerInt2Str(struct lexerInt *i) {
		strChar retVal=strCharAppendItem(NULL, '\0');
		int64_t Signed;
		uint64_t Unsigned;
		switch(i->type) {
		case INT_SINT:
				Signed=i->value.sInt;
				goto dumpS;
		case INT_UINT:
				Unsigned=i->value.uInt;
				goto dumpU;
		case INT_SLONG:
				Signed=i->value.sLong;
				goto dumpS;
		case INT_ULONG:;
				Unsigned=i->value.uLong;
				goto dumpU;
		}
		const char digits[]="0123456789";
	dumpS:
		do {
				retVal=strCharAppendItem(retVal, '\0');
				memmove(retVal+1, retVal, strlen(retVal));
				retVal[0]=digits[Signed%10];
				Signed/=10;
		}while(Signed!=0);

		return retVal;
	dumpU:
		do {
				retVal=strCharAppendItem(retVal, digits[Unsigned%10]);
				memmove(retVal+1, retVal, strlen(retVal));
				retVal[0]=digits[Signed%10];
				Unsigned/=10;
		}while(Unsigned!=0);

		return retVal;
}
char *graphEdgeIR2Str(struct __graphEdge *edge) {
	switch (*graphEdgeIRValuePtr(edge)) {
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
	case IR_CONN_FUNC_ARG:
		return strClone("IR_CONN_FUNC_ARG");
	case IR_CONN_SIMD_ARG:
		return strClone("IR_CONN_SIMD_ARG");
	}

	return NULL;
}
graphNodeIR createReturn(graphNodeIR exp,graphNodeIR func) {
		struct IRNodeFuncReturn ret;
		ret.base.attrs=NULL;
		ret.base.type=IR_FUNC_RETURN;
		ret.funcStart=func;
		ret.exp=exp;

		__auto_type retVal=GRAPHN_ALLOCATE(ret);
		graphNodeIRConnect(exp, retVal, IR_CONN_SOURCE_A);

		return retVal;
}
strChar cloneFromStr(const char *text) {
		return strCharAppendData(NULL, text,strlen(text+1));
}
static strChar opToText(enum IRNodeType type) {
		switch(type) {
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
		case IR_ARRAY_ACCESS:
				return strClone("[]");
		case IR_ASSIGN:
				return strClone("=");
		case IR_BAND:
				return strClone("=");
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
static const char *rainbowColors[]={
		COLOR_RED,
		COLOR_ORANGE,
		COLOR_YELLOW,
		COLOR_BLUE,
		COLOR_PURPLE,
		COLOR_PINK,
};
#define FROM_FORMAT(fmt,...) ({long len=snprintf(NULL,0,fmt,__VA_ARGS__);char buffer[len+1];sprintf(buffer,fmt,__VA_ARGS__);strClone(buffer);})
MAP_TYPE_DEF(int,LabelNum);
MAP_TYPE_FUNCS(int,LabelNum);

static strChar IRValue2GraphVizLabel(struct IRNode *nodeData) {
		//Choose a label based on type
		struct IRNodeValue *val=(void*)nodeData;
		switch(val->val.type) {
		case IR_VAL_FUNC: {
				const char *name=val->val.value.func->name;

				const char *format="VAL FUNC:%s";
				return FROM_FORMAT(format,name);
		}
		case IR_VAL_INT_LIT: {
				__auto_type intStr=lexerInt2Str(&val->val.value.intLit);
								
				__auto_type labelText=FROM_FORMAT("VAL INT:%s",intStr);
								
				strCharDestroy(&intStr);
				return labelText;
		}
		case IR_VAL_REG:
				//TODO
				break;
		case IR_VAL_STR_LIT: {
				const char *format="VAL STR:\"%s\"";
				return FROM_FORMAT(format, val->val.value.strLit);
		}
		case IR_VAL_VAR_REF: {
				strChar tmp=NULL;
				if(val->val.value.var.var.type==IR_VAR_MEMBER) {
						//TODO
				} else if(val->val.value.var.var.type==IR_VAR_VAR) {
						if(val->val.value.var.var.value.var->name)
								tmp=strClone(val->val.value.var.var.value.var->name);
						else {
								tmp=FROM_FORMAT("%p", val->val.value.var.var.value.var);
						}
				}

				const char *format="VAL VAR :%s-%li";
				__auto_type labelText=FROM_FORMAT(format, tmp,val->val.value.var.SSANum);

				free(tmp);
				return labelText;
		}
		case __IR_VAL_LABEL: {
				return strClone("LABEL"); //TODO
		}
		case __IR_VAL_MEM_FRAME: {
				return FROM_FORMAT("FRAME OFFSET:%li",val->val.value.__frame.offset);
		}
		case __IR_VAL_MEM_GLOBAL: {
				strChar tmp;
				if(val->val.value.__global.symbol->name)
						tmp=strClone(val->val.value.__global.symbol->name);
				else
						tmp=FROM_FORMAT("%p", val->val.value.__global.symbol);

				__auto_type labelText=FROM_FORMAT("SYM %s", tmp);
				strCharDestroy(&tmp);
				return labelText;
		}
		}
		return NULL;
}

static void  makeGVTerminalNode(mapGraphVizAttr *attrs) {
		mapGraphVizAttrInsert(*attrs, "shape", strClone("ellipse"));
		mapGraphVizAttrInsert(*attrs, "fillcolor", strClone(COLOR_BLUE));
}
static void  makeGVProcessNode(mapGraphVizAttr *attrs) {
		mapGraphVizAttrInsert(*attrs, "shape", strClone("rectangle"));
		mapGraphVizAttrInsert(*attrs, "fillcolor", strClone(COLOR_RED));
}
static void makeGVDecisionNode(mapGraphVizAttr *attrs) {
		mapGraphVizAttrInsert(*attrs, "shape", strClone("diamond"));
		mapGraphVizAttrInsert(*attrs, "fillcolor", strClone(COLOR_YELLOW));
}
struct graphVizDataNode {
		char *(*nodeOverride)(graphNodeIR node,mapGraphVizAttr *attrs,const void *data);
		void *data;
		mapLabelNum *labelNums;
};
static char *IRCreateGraphVizNode(const struct __graphNode * node,mapGraphVizAttr *attrs,const void *data) {
		const struct graphVizDataNode *data2=data;

		//Try override first
		if(data2->nodeOverride) {
				long oldCount;
				mapGraphVizAttrKeys(*attrs, NULL, &oldCount);
				
				char *overrideName=data2->nodeOverride(*graphNodeMappingValuePtr((struct __graphNode *)node),attrs,data2->data);
				if(overrideName) {
				changed:
						return strClone(overrideName);
				}

				//attribute size change
				long newCount;
				mapGraphVizAttrKeys(*attrs, NULL, &newCount);
				if(newCount!=oldCount)
						goto changed;
				}

		struct IRNode *value=graphNodeIRValuePtr(*graphNodeMappingValuePtr((struct __graphNode*)node));
		switch(value->type) {
		case IR_CHOOSE:
				makeGVDecisionNode(attrs);
				return strClone("CHOOSE");
		case IR_COND_JUMP:
				makeGVDecisionNode(attrs);
				return strClone("IF");
		case IR_FUNC_RETURN:
				makeGVTerminalNode(attrs);
				return strClone("RETURN");
		case IR_FUNC_START:
				return strClone("FUNC-START");
		case IR_FUNC_END:
				return strClone("FUNC-START");
		case IR_JUMP:
				makeGVProcessNode(attrs);
				return strClone("GOTO");
		case IR_LABEL: {
				__auto_type ptrStr=ptr2Str(node);
				__auto_type labelNum=*mapLabelNumGet(*data2->labelNums,ptrStr);
				strCharDestroy(&ptrStr);
				
				__auto_type  str=FROM_FORMAT("LABEL: #%i",labelNum);
				__auto_type str2=strClone(str);
				free(str);

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
		case IR_VALUE:
				return IRValue2GraphVizLabel(value);
		case IR_TYPECAST:  {
				struct IRNodeTypeCast *cast=(void*)value;
				char *typeNameIn=object2Str(cast->in);
				char *typeNameOut=object2Str(cast->out);

				__auto_type retVal=FROM_FORMAT("TYPECAST(%s->%s)", typeNameIn,typeNameOut);
						free(typeNameIn),free(typeNameOut);

						makeGVProcessNode(attrs);
						return retVal;
		}
		case IR_STATEMENT_START:
				makeGVTerminalNode(attrs);
				return strClone("STMT-START");
		case IR_SUB_SWITCH_START_LABEL:
				makeGVTerminalNode(attrs);
				return strClone("SUB-SWITCH START");
				
		default:;
				//Check for operator type
				__auto_type retVal=opToText(value->type);
				if(retVal) {
						makeGVProcessNode(attrs);
						return retVal;
				}

				//Unkown type
				return strClone("\?\?\?!");
		}
}
#define COLOR_GREY "grey"
struct graphVizDataEdge {
		char *(*edgeOverride)(graphEdgeIR node,mapGraphVizAttr *attrs,const void *data);
		void *data;
		mapLabelNum *labelNums;
};
static char *IRCreateGraphVizEdge(const struct __graphEdge *edge,mapGraphVizAttr *attrs,const void *data) {
		//Frist check ovveride
		const struct graphVizDataEdge *data2=data;
		if(data2->edgeOverride) {
				//old count
				long oldCount;
				mapGraphVizAttrKeys(*attrs, NULL, &oldCount);
				
				char *name=data2->edgeOverride(*graphEdgeMappingValuePtr((struct  __graphEdge*)edge),attrs,data);
				//Check if name provided
				if(name) {
				changed:
						return strClone(name);
				}

				//Check if new attrs added
				long newCount;
				mapGraphVizAttrKeys(*attrs, NULL, &newCount);
				if(oldCount!=newCount)
						goto changed;
		}

		
		__auto_type edgeVal=graphEdgeIRValuePtr((struct __graphEdge*)edge);
		if(edgeVal==NULL) {
						mapGraphVizAttrInsert(*attrs, "color", strClone(COLOR_GREY));
						return NULL;
				}
		
		switch(*edgeVal) {
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
				mapGraphVizAttrInsert(*attrs, "color",strClone(rainbowColors[0]));
				return NULL;
		case IR_CONN_FUNC_ARG: {
				struct IRNodeFuncCall *funcNode=(void*)graphNodeIRValuePtr(graphEdgeIROutgoing((struct __graphEdge*)edge));
				//Look at func node and find index of edge
				if(funcNode) {
						if(funcNode->base.type==IR_FUNC_CALL) {
								__auto_type incomingNode=graphEdgeIRIncoming((struct __graphEdge*)edge);

								//Look for index of edge 
								for(long i=0;i!=strGraphNodeIRPSize(funcNode->incomingArgs);i++) {
										if(funcNode->incomingArgs[i]==incomingNode) {
												//Color based on rainbow
												__auto_type color=rainbowColors[i%(sizeof(rainbowColors)/sizeof(*rainbowColors))];
												mapGraphVizAttrInsert(*attrs, "color", strClone(color));

												//Label name is index 
												__auto_type str=FROM_FORMAT("Arg %li", i);
												__auto_type retVal=strClone(str);
												free(&str);
												
												return retVal;
										}
								}
						}
				}

				//Isnt connected to a  func-call node
				mapGraphVizAttrInsert(*attrs, "color", strClone(COLOR_GREY));
				return NULL;
		}
		case IR_CONN_SIMD_ARG:
				//TODO
				return NULL;
		case IR_CONN_SOURCE_A: {
				//Check if result is binop(if has both IR_CONN_SOURCE_A and IR_CONN_SOURCE_B).
				__auto_type outNode=graphEdgeIROutgoing((struct __graphEdge*)edge);
				__auto_type incoming=graphNodeIRIncoming(outNode);
				__auto_type filtered=IRGetConnsOfType(incoming, IR_CONN_SOURCE_B);
				int isBinop=strGraphEdgeIRPSize(filtered)!=0;

				strGraphEdgeIRPDestroy(&incoming);
				strGraphEdgeIRPDestroy(&filtered);

				//If binop,color red and label "A"
				if(isBinop) {
						mapGraphVizAttrInsert(*attrs, "color", strClone(COLOR_RED));
						return strClone("A");
				}
				
				return NULL;
		}
		case IR_CONN_SOURCE_B:
				mapGraphVizAttrInsert(*attrs, "color", strClone(COLOR_BLUE));
				return strClone("B");
		}

		return strClone("\?\?\?!");
}

void IRGraphMap2GraphViz(graphNodeMapping graph,const char *title,const char *fn,char *(*nodeLabelOverride)(graphNodeIR node,mapGraphVizAttr *attrs,const void *data),char *(*edgeLabelOverride)(graphEdgeIR node,mapGraphVizAttr *attrs,const void *data),const void *dataNodes,const void *dataEdge) {
		__auto_type allNodes=graphNodeMappingAllNodes(graph);
		
		//
		//Number labels
		//
		mapLabelNum labelNums=mapLabelNumCreate();
		
		long labelCount=0;
		for(long i=0;i!=strGraphNodeIRPSize(allNodes);i++) {
				struct IRNode *nodeVal=graphNodeIRValuePtr(*graphNodeMappingValuePtr(allNodes[i]));
				if(nodeVal->type==IR_LABEL) {
						//Regsiter a unique label number
						char *key=ptr2Str(allNodes[i]);
						mapLabelNumInsert(labelNums, key, labelCount+1);
						strCharDestroy(&key);
						//inc label count
						labelCount++;
				}
		}

		//Create edge data tuple
		struct graphVizDataEdge edgeData;
		edgeData.data=(void*)dataEdge;
		edgeData.edgeOverride=edgeLabelOverride;
		edgeData.labelNums=&labelNums;

		//Create node data tuple
		struct graphVizDataNode nodeData;
		nodeData.data=(void*)dataEdge;
		nodeData.nodeOverride=nodeLabelOverride;
		nodeData.labelNums=&labelNums;

		FILE *dumpTo=fopen(fn, "w");		
		graph2GraphViz(dumpTo, graph, title, IRCreateGraphVizNode, IRCreateGraphVizEdge, &nodeData, &edgeData);
		fclose(dumpTo);
}
