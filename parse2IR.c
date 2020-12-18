#include <IR.h>
#include <assert.h>
#include <base64.h>
#include <exprParser.h>
#include <hashTable.h>
#include <linkedList.h>
#include <parserA.h>
#include <stdarg.h>
#include <stdio.h>
typedef int (*gnIRCmpType)(const graphNodeIR *, const graphNodeIR *);
typedef int (*pnCmpType)(const struct parserNode **,
                         const struct parserNode **);
static int ptrPtrCmp(const void *a, const void *b) {
	if (*(void **)a > *(void **)b)
		return 1;
	else if (*(void **)a < *(void **)b)
		return -1;
	else
		return 0;
}
STR_TYPE_DEF(char, Char);
STR_TYPE_FUNCS(char, Char);
MAP_TYPE_DEF(enum IRNodeType, IRNodeType);
MAP_TYPE_FUNCS(enum IRNodeType, IRNodeType);
#define ALLOCATE(x)                                                            \
	({                                                                           \
		typeof(&x) ptr = malloc(sizeof(x));                                        \
		memcpy(ptr, &x, sizeof(x));                                                \
		ptr;                                                                       \
	})
#define GRAPHN_ALLOCATE(x) ({ __graphNodeCreate(&x, sizeof(x), 0); })
MAP_TYPE_DEF(struct parserNode *, ParserNode);
MAP_TYPE_FUNCS(struct parserNode *, ParserNode);
enum scopeType {
	SCOPE_TYPE_FUNC,
	SCOPE_TYPE_SCOPE,
	SCOPE_TYPE_LOOP,
	SCOPE_TYPE_SWIT,
};
struct IRGenScopeStack {
	enum scopeType type;
	union {
		struct {
			graphNodeIR next;
			graphNodeIR exit;
		} loop;
		graphNodeIR switExitLab;
	} value;
};
STR_TYPE_DEF(struct IRGenScopeStack, ScopeStack);
STR_TYPE_FUNCS(struct IRGenScopeStack, ScopeStack);
MAP_TYPE_DEF(struct IRValue, StrMemLabel);
MAP_TYPE_FUNCS(struct IRValue, StrMemLabel);
MAP_TYPE_DEF(graphNodeIR, IRNodeByPtr);
MAP_TYPE_FUNCS(graphNodeIR, IRNodeByPtr);
struct gotoPair {
		graphNodeIR dummyLabel;
		struct parserNode *node;
};
MAP_TYPE_DEF(struct gotoPair, GotoPairByParserNode);
MAP_TYPE_FUNCS(struct gotoPair, GotoPairByParserNode);
struct IRGenInst {
	mapIRNodeByPtr casesByParserNode;
	mapGraphNode labelsByParserNode;
		mapIRNodeByPtr dummyGotosByParserNode;
		mapGotoPairByParserNode
	strGraphNodeIRP currentNodes;
	strScopeStack scopes;
	mapStrMemLabel strLabels;
};
static __thread struct IRGenInst *currentGen = NULL;

static strChar ptr2Str(const void *a) {
	__auto_type res = base64Enc((const char *)&a, sizeof(a));
	__auto_type retVal = strCharAppendData(NULL, res, strlen(a) + 1);
	free(res);

	return retVal;
}

static void setCurrentNodes(struct IRGenInst *inst, ...) {
	if (inst->currentNodes)
		strGraphNodeIRPDestroy(&inst->currentNodes);

	inst->currentNodes = NULL;
	va_list list;
	va_start(list, inst);
	for (;;) {
		__auto_type node = va_arg(list, graphNodeIR);
		if (!node)
			break;

		inst->currentNodes = strGraphNodePAppendItem(inst->currentNodes, node);
	}
	va_end(list);
}
static void connectCurrentsToNode(graphNodeIR node) {
	for (long i = 0; i != strGraphNodeIRPSize(currentGen->currentNodes); i++) {
		graphNodeIRConnect(currentGen->currentNodes[i], node, IR_CONN_FLOW);
	}
}
static strChar strClone(const char *str) {
	strChar buffer = strCharResize(NULL, strlen(str) + 1);
	strcpy(buffer, str);

	return buffer;
}
static struct IRValue *addStringLiteralLabel(const char *name) {
	if (NULL == mapStrMemLabelGet(currentGen->strLabels, name)) {
		// Key count
		long count;
		mapStrMemLabelKeys(currentGen->strLabels, NULL, &count);

		struct IRValue strLit;
		strLit.type = IR_VAL_STR_LIT;
		strLit.value.strLit = NULL; // Will be assigned to value of map key ahead

		mapStrMemLabelInsert(currentGen->strLabels, name, strLit);
		__auto_type find = mapStrMemLabelGet(currentGen->strLabels, name);
		// NOTE:Look above,we are assign the str literal to the value of map-entry's
		// key
		find->value.strLit = mapStrMemLabelValueKey(find);
		return find;
	}

	__auto_type find = mapStrMemLabelGet(currentGen->strLabels, name);
	assert(find);

	return find;
}
//
// Attaches all entry nodes to a start node
//
static int visitNotVisitedOperand(const struct __graphNode *node,
                                  const struct __graphEdge *ed,
                                  const void *data) {
	strGraphNodeIRP visited = (void *)data;

	// Only visit operand /dest operand
	__auto_type edType = *graphEdgeIRValuePtr((void *)ed);
	switch (edType) {
	case IR_CONN_SOURCE_A:
	case IR_CONN_SOURCE_B:
	case IR_CONN_DEST:
	case IR_CONN_FUNC_ARG:
		break;
	default:
		return 0;
	}

	return NULL == strGraphNodeIRPSortedFind(visited, (void *)node,
	                                         (gnIRCmpType)ptrPtrCmp);
}
static void visitNode(struct __graphNode *node, void *data) {
	strGraphNodeIRP visited = (void *)data;
	if (NULL ==
	    strGraphNodeIRPSortedFind(visited, (void *)node, (gnIRCmpType)ptrPtrCmp))
		visited =
		    strGraphNodeIRPSortedInsert(visited, node, (gnIRCmpType)ptrPtrCmp);
}
static __thread graphNodeIR currentStatement = NULL;
static void enterStatement() {
	assert(currentStatement == NULL);

	struct IRNodeStatementStart start;
	start.base.attrs = NULL;
	start.base.type = IR_STATEMENT_START;

	__auto_type node = GRAPHN_ALLOCATE(start);
	connectCurrentsToNode(node);
	setCurrentNodes(currentGen, node, NULL);
}
static void leaveStatement() {
	struct IRNodeStatementStart end;
	end.base.attrs = NULL;
	end.base.type = IR_STATEMENT_END;

	__auto_type node = GRAPHN_ALLOCATE(end);
	connectCurrentsToNode(node);
	setCurrentNodes(currentGen, node, NULL);

	((struct IRNodeStatementStart *)graphNodeIRValuePtr(currentStatement))->end =
	    node;

	currentStatement = NULL;
}
static char *IRVarHash(const struct parserNode *node) {
	if (node->type == NODE_VAR) {
		struct parserNodeVar *var = (void *)node;

		long len = sprintf(NULL, "V%p", var->var);
		char buffer[len + 1];
		sprintf(NULL, "V%p", var->var);
		return strClone(buffer);
	} else if (node->type == NODE_MEMBER_ACCESS) {
		strChar tail = NULL;

		// Ensure left-most item is var
		// Also make the tail str
		struct parserNodeVar *varNode;
		for (__auto_type node2 = node;;) {
			if (node2->type == NODE_MEMBER_ACCESS) {
				struct parserNodeMemberAccess *access = (void *)node2;
				node2 = access->exp;

				struct parserNodeName *name = (void *)access->name;
				assert(name->base.type == NODE_NAME);
				tail =
				    strCharConcat(strCharAppendItem(strClone(name->text), '.'), tail);
			} else if (node2->type == NODE_VAR) {
				varNode = (void *)node2;
				break;
			} else {
				strCharDestroy(&tail);
				return NULL;
			}
		}

		long len = sprintf(NULL, "V%p", varNode->var);
		char buffer[len + 1];
		sprintf(NULL, "V%p", varNode->var);

		return strCharConcat(strClone(buffer), tail);
	}

	return NULL;
};
static graphNodeIR insertVar(const struct parserNode *node, int makeNewVer) {
	__auto_type hash = IRVarHash(node);
	if (!hash)
		return NULL;

loop:;
	__auto_type find = mapIRVarRefsGet(IRVars, hash);
	if (!find) {
		struct IRVar retVal;
		if (node->type == NODE_MEMBER_ACCESS) {
			retVal.type = IR_VAR_MEMBER;
			retVal.value.member = (void *)node;
		} else if (node->type == NODE_VAR) {
			struct parserNodeVar *varNode = (void *)node;
			retVal.type = IR_VAR_VAR;
			retVal.value.var = varNode->var;
		}

		struct IRVarRefs refs;
		refs.refs = 0;
		refs.var = retVal;
		mapIRVarRefsInsert(IRVars, hash, refs);

		strCharDestroy(&hash);
		goto loop;
	}

	struct IRNodeValue val;
	val.base.attrs = NULL;
	val.base.type = IR_VALUE;
	val.val.value.var = find->var;
	if (makeNewVer)
		val.val.value.var.SSANum = find->refs++;
	else
		val.val.value.var.SSANum = find->refs;

	return GRAPHN_ALLOCATE(val);
}
//
// Assign operations  take 2 inputs "a" and "b","b" is the dest,"a" is the
// source
//
graphNodeIR IRAssign(graphNodeIR to, graphNodeIR fromValue) {
	struct IRGenInst *gen = currentGen;

	struct IRNodeAssign assign;
	assign.base.type = IR_ASSIGN;
	assign.base.attrs = NULL;

	__auto_type retVal = GRAPHN_ALLOCATE(assign);
	graphNodeIRConnect(fromValue, retVal, IR_CONN_SOURCE_A);

	// Increment SSA count(if applicable)
	if (graphNodeIRValuePtr(to)->type == IR_VALUE) {
		struct IRNodeValue *val = (void *)graphNodeIRValuePtr(to);
		if (val->val.type == IR_VAL_VAR_REF) {
			struct IRVarRefs *refs = NULL;

			// Get ptr str
			strChar ptrStr;
			if (val->val.value.var.type == IR_VAR_VAR) {
				ptrStr = ptr2Str(val->val.value.var.value.var);
			} else if (val->val.value.var.type == IR_VAR_MEMBER) {
				ptrStr = ptr2Str(val->val.value.var.value.member);
			}
			__auto_type find = mapIRVarRefsGet(IRVars, ptrStr);
			strCharDestroy(&ptrStr);

			assert(find);
			find->refs++;
		}
	}

	// Assign dest
	graphNodeIRConnect(retVal, to, IR_CONN_DEST);

	return retVal;
}
static mapIRNodeType unop2IRType;
static mapIRNodeType binop2IRType;
static mapIRNodeType assign2IRType;
static mapIRNodeType unopAssign2IRType;
static void init() __attribute__((constructor));
static struct object *U0Ptr;
static void init() {
	U0Ptr = objectPtrCreate(&typeU0);
	//
	// Assign unops
	//
	unopAssign2IRType = mapIRNodeTypeCreate();
	mapIRNodeTypeInsert(unopAssign2IRType, "++", IR_INC);
	mapIRNodeTypeInsert(unopAssign2IRType, "--", IR_DEC);
	//
	// Unops
	//
	unop2IRType = mapIRNodeTypeCreate();
	mapIRNodeTypeInsert(unop2IRType, "~", IR_BNOT);
	mapIRNodeTypeInsert(unop2IRType, "!", IR_LNOT);
	mapIRNodeTypeInsert(unop2IRType, "-", IR_NEG);
	mapIRNodeTypeInsert(unop2IRType, "+", IR_POS);
	//
	// Binops
	//
	binop2IRType = mapIRNodeTypeCreate();
	mapIRNodeTypeInsert(binop2IRType, "+", IR_ADD);
	mapIRNodeTypeInsert(binop2IRType, "-", IR_SUB);
	//
	mapIRNodeTypeInsert(binop2IRType, "*", IR_MULT);
	mapIRNodeTypeInsert(binop2IRType, "/", IR_DIV);
	mapIRNodeTypeInsert(binop2IRType, "%", IR_MOD);
	//
	mapIRNodeTypeInsert(binop2IRType, "<<", IR_LSHIFT);
	mapIRNodeTypeInsert(binop2IRType, ">>", IR_RSHIFT);
	//
	mapIRNodeTypeInsert(binop2IRType, "&", IR_BAND);
	mapIRNodeTypeInsert(binop2IRType, "|", IR_BOR);
	mapIRNodeTypeInsert(binop2IRType, "^", IR_BXOR);
	//
	mapIRNodeTypeInsert(binop2IRType, ">", IR_GT);
	mapIRNodeTypeInsert(binop2IRType, "<", IR_LT);
	mapIRNodeTypeInsert(binop2IRType, ">=", IR_GE);
	mapIRNodeTypeInsert(binop2IRType, "<=", IR_LE);
	mapIRNodeTypeInsert(binop2IRType, "==", IR_EQ);
	mapIRNodeTypeInsert(binop2IRType, "!=", IR_NE);
	//
	mapIRNodeTypeInsert(binop2IRType, "&&", IR_LAND);
	mapIRNodeTypeInsert(binop2IRType, "||", IR_LOR);
	mapIRNodeTypeInsert(binop2IRType, "^^", IR_LXOR);

	//
	// Assigment operators
	//
	assign2IRType = mapIRNodeTypeCreate();
	mapIRNodeTypeInsert(assign2IRType, "=", IR_ASSIGN);
	//
	mapIRNodeTypeInsert(assign2IRType, "+=", IR_ADD);
	mapIRNodeTypeInsert(assign2IRType, "-=", IR_SUB);
	//
	mapIRNodeTypeInsert(assign2IRType, "<<=", IR_LSHIFT);
	mapIRNodeTypeInsert(assign2IRType, ">>=", IR_RSHIFT);
	//
	mapIRNodeTypeInsert(assign2IRType, "*=", IR_MULT);
	mapIRNodeTypeInsert(assign2IRType, "/=", IR_DIV);
	mapIRNodeTypeInsert(assign2IRType, "%=", IR_MOD);
	//
	mapIRNodeTypeInsert(assign2IRType, "*=", IR_MULT);
	mapIRNodeTypeInsert(assign2IRType, "/=", IR_DIV);
	mapIRNodeTypeInsert(assign2IRType, "%=", IR_MOD);
	//
	mapIRNodeTypeInsert(assign2IRType, "&=", IR_BAND);
	mapIRNodeTypeInsert(assign2IRType, "^=", IR_BXOR);
	mapIRNodeTypeInsert(assign2IRType, "|=", IR_BOR);
}
static void deinit() __attribute__((destructor));
static void deinit() {
	mapIRNodeTypeDestroy(assign2IRType, NULL);
	mapIRNodeTypeDestroy(binop2IRType, NULL);
}
// TODO optomize
graphNodeIR IRCondJump(graphNodeIR cond, graphNodeIR successLabel,
                       graphNodeIR failLabel) {
	if (!successLabel && !failLabel)
		return NULL;

	struct IRNodeCondJump jump;
	jump.base.type = IR_COND_JUMP;
	jump.base.attrs = NULL;

	__auto_type jump2 = GRAPHN_ALLOCATE(jump);
	graphNodeIRConnect(cond, jump2, IR_CONN_COND);
	if (successLabel)
		graphNodeIRConnect(jump2, successLabel, IR_CONN_COND_TRUE);
	if (failLabel)
		graphNodeIRConnect(jump2, failLabel, IR_CONN_COND_TRUE);

	if (successLabel && failLabel)
		setCurrentNodes(currentGen, successLabel, failLabel, NULL);
	else if (failLabel)
		setCurrentNodes(currentGen, failLabel, NULL);
	else if (successLabel)
		setCurrentNodes(currentGen, successLabel, NULL);

	return jump2;
};
STR_TYPE_DEF(long, Long);
STR_TYPE_FUNCS(long, Long);
static void getCases(strParserNode casesSubcases, strParserNode *cases,
                     struct parserNode **dft) {
	for (long i = 0; i != strParserNodeSize(casesSubcases); i++) {
		__auto_type node = casesSubcases[i];

		if (node->type == NODE_SWITCH) {
			struct parserNodeSwitch *swit = (void *)node;

			getCases(swit->caseSubcases, cases, dft);
		} else if (node->type == NODE_SUBSWITCH) {
			struct parserNodeSubSwitch *sub = (void *)node;

			getCases(sub->caseSubcases, cases, dft);
		} else if (node->type == NODE_CASE) {
			*cases = strParserNodeAppendItem(*cases, node);
		} else if (node->type == NODE_DEFAULT && dft) {
			assert(*dft == NULL);
			*dft = node;
		} else {
			assert(0);
		}
	}
}
static int caseCmp(const void *a, const void *b) {
	const struct parserNodeCase *A = a, *B = b;
	assert(A->base.type == NODE_CASE);
	assert(B->base.type == NODE_CASE);

	return A->valueLower - B->valueLower;
}
static void copyConnectionsIncomingTo(strGraphEdgeIRP incoming,
                                      graphNodeIR to) {
	for (long i = 0; i != strGraphEdgeIRPSize(incoming); i++) {
		__auto_type eV = *graphEdgeIRValuePtr(incoming[i]);
		__auto_type inNode = graphEdgeIRIncoming(incoming[i]);
		graphNodeIRConnect(inNode, to, eV);
	}
}
static void copyConnectionsOutcomingTo(strGraphEdgeIRP outgoing,
                                       graphNodeIR to) {
	for (long i = 0; i != strGraphEdgeIRPSize(outgoing); i++) {
		__auto_type eV = *graphEdgeIRValuePtr(outgoing[i]);
		__auto_type outNode = graphEdgeIROutgoing(outgoing[i]);
		graphNodeIRConnect(outNode, to, eV);
	}
}
//
// Groups consecutive elemetns in "mini" jump tables,this prevents gaint
// jump-tables
//
static graphNodeIR __createSwitchCodeAfterBody(graphNodeIR cond,
                                               const struct parserNode *node) {
	__auto_type entry = IRCreateLabel();
	connectCurrentsToNode(entry);
	setCurrentNodes(currentGen, entry, NULL);

	strParserNode cases = NULL;
	struct parserNode *dft = NULL;

	// Get cases
	__auto_type bootstrap = strParserNodeAppendItem(NULL, (void *)node);
	getCases(bootstrap, &cases, &dft);
	strParserNodeDestroy(&bootstrap);

	// Sort by order
	qsort(cases, strParserNodeSize(cases), sizeof(*cases), caseCmp);

	long currentV = 0, consecCaseLen = 0, consecStart;
	for (long i = 0; i != strParserNodeSize(cases); i++) {
	consecLoop:;
		// For for consecutive cases to group togheter in a mini-table
		struct parserNodeCase *caseN = (void *)cases[i];
		if (consecCaseLen == 0) {
			// If case represent a range,ensure isnt a big range
			if (caseN->valueUpper - caseN->valueLower >= 5)
				goto makeMiniJumpTable;

			currentV = caseN->valueUpper;
			consecStart = caseN->valueLower;
		}
		if (consecStart + 1 == caseN->valueLower) {
			currentV = caseN->valueUpper;
			consecCaseLen++;

			continue;
		}

		// Make a jump table
	makeMiniJumpTable : {
		struct IRNodeJumpTable miniTab;
		miniTab.base.attrs = NULL;
		miniTab.base.type = IR_JUMP_TAB;
		miniTab.startIndex = consecStart;

		long start = i - consecCaseLen;
		strGraphNodeIRP labels = NULL;
		for (long i2 = start; i2 != i; i2++) {

			// Create copies of ranged labels,if 1 value,it is still a range of line
			struct parserNodeCase *caseN = (void *)cases[i2];
			for (long i3 = caseN->valueLower; i3 != caseN->valueUpper; i3++)
				labels =
				    strGraphNodeIRPAppendItem(labels, parserNode2IRStmt((void *)caseN));
		}

		miniTab.labels = labels;
		__auto_type miniTableNode = GRAPHN_ALLOCATE(miniTab);
		__auto_type mtSkipLab = IRCreateLabel();
		graphNodeIRConnect(miniTableNode, mtSkipLab, IR_CONN_FLOW);

		// Create range check
		__auto_type startNode = IRCreateIntLit(consecStart);
		__auto_type endNode = IRCreateIntLit(currentV);

		__auto_type ge = IRCreateBinop(startNode, cond, IR_GE);
		__auto_type lt = IRCreateBinop(cond, startNode, IR_LT);
		__auto_type and = IRCreateBinop(ge, lt, IR_LAND);
		__auto_type cJmp = IRCondJump(cond, NULL, mtSkipLab);

		// condJump
		//    ||
		//    \/
		// jump table
		//    ||
		//    \/
		// endNode
		connectCurrentsToNode(cJmp);
		graphNodeIRConnect(cJmp, miniTableNode, IR_CONN_FLOW);
		setCurrentNodes(currentGen, endNode, NULL);
	}
	}

	__auto_type switchEndLabel = IRCreateLabel();

	if (dft != NULL) {
		// Jump to default label
		__auto_type dftNode = parserNode2IRStmt(dft);
		__auto_type jmpToDft = IRCreateJmp(dftNode);
		connectCurrentsToNode(jmpToDft);
		setCurrentNodes(currentGen, jmpToDft, NULL);
	} else {
		// Jump to default label
		__auto_type endJmp = IRCreateJmp(switchEndLabel);
		connectCurrentsToNode(endJmp);
		setCurrentNodes(currentGen, endJmp, NULL);
	}

	// Create scope for switch
	struct IRGenScopeStack scope;
	scope.type = SCOPE_TYPE_SWIT;
	scope.value.switExitLab = switchEndLabel;
	currentGen->scopes = strScopeStackAppendItem(currentGen->scopes, scope);

	// Parse Body
	struct parserNodeSwitch *swit = (void *)node;
	__auto_type body = parserNode2IRStmt(swit->body);

	// Pop scope
	currentGen->scopes = strScopeStackResize(
	    currentGen->scopes, strScopeStackSize(currentGen->scopes) - 1);

	//
	// Sub-switches always start at "start:" label,not cases,so
	// replace each sub-switch's case with
	// NOTE 1
	// ```
	// goto end;
	//
	// subSwitch-case:
	//
	// set JUMP_BACK=end;
	// goto subswitchStart;
	//
	// end:
	//```

	// NOTE 2
	// First Sub-switch cases will be replaced with
	// ```
	// if(JUMP_BACK!=NULL)
	//     goto JUMP_BACK
	// ```
	// This allows returning back to case

	// Collect list of sub-switchs
	strParserNode subs = NULL;
	for (long i = 0; i != strParserNodeSize(cases); i++) {
		struct parserNodeCase *cs = (void *)cases[i];

		// Check if sub-case
		if (cs->parent->type != NODE_SUBSWITCH)
			continue;

		if (NULL == strParserNodeSortedFind(subs, cs->parent, (pnCmpType)ptrPtrCmp))
			subs = strParserNodeSortedInsert(subs, cs->parent, (pnCmpType)ptrPtrCmp);
	}
	__auto_type jmpBack = IRCreateVirtVar(U0Ptr);
	for (long i = 0; i != strParserNodeSize(subs); i++) {
		// Will create IR_SUB_SWITCH_START_LABEL
		__auto_type node = parserNode2IRStmt(subs[i]);
		assert(graphNodeIRValuePtr(node)->type == IR_SUB_SWITCH_START_LABEL);

		struct parserNodeSubSwitch *sub = (void *)subs[i];
		// First label,SEE NOTE 1
		struct parserNodeCase *firstCase = NULL;
		for (long i = 0; i != strParserNodeSize(sub->caseSubcases); i++) {
			if (sub->caseSubcases[i]->type == NODE_CASE) {
				firstCase = (void *)sub->caseSubcases[i];
				break;
			}
		}
		if (firstCase) {

			//  previousNodes
			//    ||
			//    \/
			// if JUMP_BACK==NULL) jmp(end);
			// jump(JUMP_BACK)
			// end:
			//    ||
			//    \/
			// firstCase

			__auto_type null = IRCreateIntLit(0);
			// See NOTE 2
			__auto_type nullCmp = IRCreateBinop(null, IRCreateVarRef(jmpBack), IR_EQ);
			__auto_type endLabel = IRCreateLabel();
			__auto_type incoming =
			    graphNodeIRIncoming(parserNode2IRStmt((void *)firstCase));
			__auto_type cJmp = IRCondJump(nullCmp, endLabel, NULL);
			// Pevious nodes->if...
			copyConnectionsIncomingTo(incoming, cJmp);
			setCurrentNodes(currentGen, cJmp, NULL);
			//->jump(JUMP_BACK)
			__auto_type gotoEnd = IRCreateJmp(IRCreateVarRef(jmpBack));
			connectCurrentsToNode(gotoEnd);
			//->end
			setCurrentNodes(currentGen, gotoEnd, NULL);
			connectCurrentsToNode(endLabel);
			//->firstCast
			setCurrentNodes(currentGen, endLabel, NULL);
			connectCurrentsToNode(parserNode2IRStmt((void *)firstCase));
		}

		// See NOTE 1
		for (long i = 0; i != strParserNodeSize(sub->caseSubcases); i++) {
			if (sub->caseSubcases[i] == (void *)firstCase)
				continue;

			// Ignore non-cases
			if (sub->caseSubcases[i]->type != NODE_CASE)
				continue;

			struct parserNodeCase *cs = (void *)sub->caseSubcases[i];

			__auto_type endLabel = IRCreateLabel();
			__auto_type jmp2SubStart =
			             IRCreateJmp(parserNode2IRStmt((void *)firstCase));
			__auto_type jmp2End = IRCreateJmp(endLabel);

			// JUMP_BACK=endLabel
			__auto_type assignJmpBack =
			    IRAssign(IRCreateVarRef(jmpBack), IRCreateValueFromLabel(endLabel));

			//"Replace" case with structure from NOTE 1(collect the in/out connections
			// then disconnect case)
			__auto_type incoming = graphNodeIRIncoming(parserNode2IRStmt((void *)cs));
			__auto_type outgoing = graphNodeIROutgoing(parserNode2IRStmt((void *)cs));

			// incoming ->goto end
			copyConnectionsIncomingTo(incoming, jmp2End);
			//->JUMP_BACK=endLabel
			graphNodeIRConnect(jmp2End, assignJmpBack, IR_CONN_FLOW);
			//->goto firstCase
			graphNodeIRConnect(assignJmpBack, jmp2SubStart, IR_CONN_FLOW);
			//->end:
			graphNodeIRConnect(jmp2SubStart, endLabel, IR_CONN_FLOW);
			//-> outgoing
			copyConnectionsOutcomingTo(outgoing, endLabel);

			// destroy "old" case
			__auto_type csNode = parserNode2IRStmt((void *)cs);
			graphNodeIRKill(&csNode, (void(*)(void*))IRNodeDestroy, NULL);
		}

		// Remove
		graphNodeIRKill(&node, (void(*)(void*))IRNodeDestroy, NULL);
	}
	strParserNodeDestroy(&subs);

	return entry;
}
static graphNodeIR __parserNode2IRNoStmt(const struct parserNode *node);
graphNodeIR parserNode2IRStmt(const struct parserNode *node) {
	if (node == NULL)
		return NULL;

	// Create statement if node is an expression type.
	int inStatement = 0;
	switch (node->type) {
	case NODE_COMMA_SEQ:
	case NODE_BINOP:
	case NODE_FUNC_CALL:
	case NODE_UNOP:
	case NODE_VAR:
	case NODE_LIT_INT:
	case NODE_LIT_STR:
	case NODE_ARRAY_ACCESS: {
		inStatement = 1;
		enterStatement();
	}
	default:;
	}

	__auto_type retVal = __parserNode2IRNoStmt(node);

	// Leave statement if in statement
	if (inStatement)
		leaveStatement();

	return retVal;
}
static graphNodeIR __parserNode2IRNoStmt(const struct parserNode *node) {
	switch (node->type) {
	case NODE_GOTO: {
			//TODO
	}
	case NODE_RETURN: {
			struct IRNodeFuncReturn ret;
			ret.base.attrs=NULL;
			ret.base.type=IR_FUNC_RETURN;
			ret.exp=NULL;
			
			__auto_type retNode=GRAPHN_ALLOCATE(ret);
			connectCurrentsToNode(retNode);
			
			return retNode;
	}
	case NODE_BINOP: {
		//
		// Remember to create statement if (createStatement)
		//
		graphNodeIR retVal = NULL;
		struct parserNodeBinop *binop = (void *)node;
		struct parserNodeOpTerm *op = (void *)binop->op;

		// Compute args
		__auto_type aVal = __parserNode2IRNoStmt(binop->a);
		__auto_type bVal = __parserNode2IRNoStmt(binop->b);

		// If non-assign binop
		__auto_type b = mapIRNodeTypeGet(binop2IRType, op->text);
		if (b) {
			__auto_type retVal = IRCreateBinop(aVal, bVal, *b);

			setCurrentNodes(currentGen, retVal, NULL);
			return retVal;
		}

		__auto_type assign = mapIRNodeTypeGet(assign2IRType, op->text);
		if (assign) {
			retVal = IRAssign(aVal, bVal);
			return retVal;
		}

		assert(0);
	}
	case NODE_UNOP: {
		struct parserNodeUnop *unop = (void *)node;
		struct parserNodeOpTerm *op = (void *)unop->op;
		__auto_type in = __parserNode2IRNoStmt(unop->a);

		// Find assign unop
		__auto_type find = mapIRNodeTypeGet(unopAssign2IRType, op->text);
		if (find) {
			graphNodeIR newNode = NULL;
			if (0 == strcmp(op->text, "++")) {
				struct IRNodeInc inc;
				inc.base.type = IR_INC;
				inc.base.attrs = NULL;
				newNode = GRAPHN_ALLOCATE(inc);
			} else if (0 == strcmp(op->text, "--")) {
				struct IRNodeInc dec;
				dec.base.type = IR_INC;
				dec.base.attrs = NULL;
				newNode = GRAPHN_ALLOCATE(dec);
			} else {
				struct IRNodeUnop unop;
				unop.base.type = *find;
				unop.base.attrs = NULL;
				newNode = GRAPHN_ALLOCATE(unop);
			}
			assert(newNode != NULL);

			graphNodeIRConnect(in, newNode, IR_CONN_SOURCE_A);
			setCurrentNodes(currentGen, newNode, NULL);

			return newNode;
		}
	case NODE_ARRAY_ACCESS: {
		struct parserNodeArrayAccess *access = (void *)node;
		__auto_type exp = __parserNode2IRNoStmt(access->exp);
		__auto_type index = __parserNode2IRNoStmt(access->index);

		int success;
		__auto_type scale = objectSize(assignTypeToOp(node), &success);
		assert(success);

		struct IRNodeArrayAccess access2;
		access2.base.type = IR_ARRAY_ACCESS;
		access2.base.attrs = NULL;
		access2.scale = scale;

		__auto_type retVal = GRAPHN_ALLOCATE(access2);
		graphNodeIRConnect(exp, retVal, IR_CONN_SOURCE_A);
		graphNodeIRConnect(index, retVal, IR_CONN_SOURCE_B);
		return retVal;
	}
	}
	case NODE_DEFAULT:
	case NODE_CASE: {
		__auto_type ptrStr = ptr2Str(node);
		mapParserNodeInsert(currentGen->casesByParserNode, ptrStr,
		                    (struct parserNode *)node);
		strCharDestroy(&ptrStr);

		struct IRNodeLabel lab;
		lab.base.type = IR_LABEL;
		lab.base.attrs = NULL;
		;

		setCurrentNodes(currentGen, GRAPHN_ALLOCATE(lab), NULL);
		return currentGen->currentNodes[0];
	};
	case NODE_FUNC_DEF: {
			//TODO
			struct parserNodeFuncDef *def = (void *)node;
		struct IRNodeFuncStart start;
		start.base.attrs=NULL;
		start.base.type=IR_FUNC_START;
		start.type=def->funcType;
		start.end=NULL;
		__auto_type startNode=GRAPHN_ALLOCATE(start);

		graphNodeIR currentNode=NULL;
		//Assign arguments to variables
  struct objectFunction *func=(void*)def->funcType;
		for(long i=0;strFuncArgSize(func->args);i++) {
				currentNode=startNode;
				
				__auto_type arg=IRCreateFuncArg(func->args[i].type, i);
				__auto_type var=IRCreateVarRef(IRCreateVirtVar(func->args[i].type));
				graphNodeIRConnect(currentNode, arg, IR_CONN_DEST);
				graphNodeIRConnect(arg, var, IR_CONN_DEST);

				currentNode=var;
		}
		setCurrentNodes(currentGen, currentNode,NULL);
		
		//Compute body
		parserNode2IRStmt(def->bodyScope);
		//
		
		struct IRNodeFuncEnd end;
		end.base.attrs=NULL;
		end.base.type=IR_FUNC_END;
		__auto_type endNode=GRAPHN_ALLOCATE(end);
		
		connectCurrentsToNode(endNode);
	}

	case NODE_VAR_DECL:
	case NODE_VAR_DECLS:
	case NODE_OP:
	case NODE_NAME:
	case NODE_META_DATA:
	case NODE_KW:
	case NODE_FUNC_FORWARD_DECL:
	case NODE_UNION_DEF:
	case NODE_CLASS_DEF: {
		// Classes dont exist in code
		return NULL;
	}
	case NODE_COMMA_SEQ: {
		struct parserNodeCommaSeq *seq = (void *)node;

		graphNodeIR lastNode = NULL;
		for (long i = 0; i != strParserNodeSize(seq->items); i++) {
			__auto_type node = __parserNode2IRNoStmt(seq->items[i]);
			for (long i = 0; i != strGraphNodePSize(currentGen->currentNodes); i++)
				graphNodeIRConnect(currentGen->currentNodes[i], node, IR_CONN_FLOW);

			lastNode = node;
			setCurrentNodes(currentGen, node, NULL);
		}

		return lastNode;
	}
	case NODE_DO: {
		struct parserNodeDo *doStmt = (void *)node;

		// Label
		struct IRNodeLabel lab;
		lab.base.type = IR_LABEL;
		lab.base.attrs = NULL;
		__auto_type lab2 = GRAPHN_ALLOCATE(lab);
		setCurrentNodes(currentGen, lab);

		// Body
		__auto_type body = parserNode2IRStmt(doStmt->body);

		// Cond
		__auto_type cond = parserNode2IRStmt(doStmt->cond);
		__auto_type cJump = IRCondJump(cond, lab2, NULL);

		setCurrentNodes(currentGen, cJump, NULL);
		return lab2;
	}
	case NODE_FOR: {
		struct IRNodeLabel lab;
		lab.base.type = IR_LABEL;
		lab.base.attrs = NULL;
		__auto_type labNext = GRAPHN_ALLOCATE(lab);
		__auto_type labExit = GRAPHN_ALLOCATE(lab);
		__auto_type labCond = GRAPHN_ALLOCATE(lab);

		struct parserNodeFor *forStmt = (void *)node;
		__auto_type init = parserNode2IRStmt(forStmt->init);
		connectCurrentsToNode(init);
		setCurrentNodes(currentGen, init, NULL);
		connectCurrentsToNode(labCond);
		setCurrentNodes(currentGen, labCond, NULL);

		//"cond" label
		connectCurrentsToNode(labCond);
		setCurrentNodes(currentGen, labCond, NULL);

		// Cond
		__auto_type cond = parserNode2IRStmt(forStmt->cond);
		IRCondJump(cond, NULL, labExit);
		connectCurrentsToNode(cond);
		setCurrentNodes(currentGen, cond, NULL);

		// Make "scope" for body to hold next loop
		struct IRGenScopeStack scope;
		scope.type = SCOPE_TYPE_LOOP;
		scope.value.loop.next = labNext;
		scope.value.loop.exit = labExit;
		currentGen->scopes = strScopeStackAppendItem(currentGen->scopes, scope);

		// Enter body
		__auto_type body = parserNode2IRStmt(forStmt->body);
		connectCurrentsToNode(body);

		//"Exit" body by connecting to exit label
		connectCurrentsToNode(labExit);
		setCurrentNodes(currentGen, labExit);
		// Pop "scope"
		currentGen->scopes = strScopeStackResize(
		    currentGen->scopes, strScopeStackSize(currentGen->scopes) - 1);

		// "Next" label
		connectCurrentsToNode(labNext);
		setCurrentNodes(currentGen, labNext, NULL);
		// Inc code
		parserNode2IRStmt(forStmt->inc);
		// Connect increment to cond code
		connectCurrentsToNode(labCond);

		return labCond;
	}
	case NODE_FUNC_CALL: {
		struct IRNodeFuncCall call;
		call.base.attrs = NULL;
		call.base.type = IR_FUNC_CALL;
		call.incomingArgs = NULL;

		struct parserNodeFuncCall *call2 = (void *)node;
		__auto_type func = __parserNode2IRNoStmt(call2->func);

		strGraphNodeIRP args = NULL;
		for (long i = 0; i != strParserNodeSize(call2->args); i++) {
			graphNodeIR arg = NULL;
			// If provided
			if (call2->args[i]) {
				arg = __parserNode2IRNoStmt(call2->args[i]);
			} else {
				// Use dft
				struct objectFunction *funcType = (void *)assignTypeToOp(call2->func);
				assert(funcType->args[i].dftVal);
				arg = __parserNode2IRNoStmt(funcType->args[i].dftVal);
			}

			args = strGraphNodeIRPAppendItem(args, arg);
		}

		// Connect args
		call.incomingArgs = args;
		graphNodeIR callNode = GRAPHN_ALLOCATE(call);
		for (long i = 0; i != strGraphNodeIRPSize(args); i++) {
			graphNodeIRConnect(args[i], callNode, IR_CONN_FUNC_ARG);
		}

		// Connect func
		graphNodeIRConnect(parserNode2IRStmt(call2->func), callNode, IR_CONN_FUNC);

		return callNode;
	}
	case NODE_FUNC_REF: {
		struct parserNodeFuncRef *ref = (void *)node;

		struct IRNodeValue val;
		val.base.attrs = NULL;
		val.base.type = IR_VALUE;
		val.val.type = IR_VAL_FUNC;
		val.val.value.func = ref->func;

		return GRAPHN_ALLOCATE(val);
	}
	case NODE_IF: {
		struct parserNodeIf *ifNode = (void *)node;

		graphNodeIR endBranch=IRCreateLabel(),fBranch=NULL;
		// If no else,endBranch if false branch
		if (ifNode->el == NULL) {
			fBranch = endBranch;
		} else {
			fBranch = IRCreateLabel();
		}

		__auto_type cond = parserNode2IRStmt(ifNode->cond);
		__auto_type condJ = IRCondJump(cond, NULL, fBranch);

		connectCurrentsToNode(condJ);
		setCurrentNodes(currentGen, condJ, NULL);
		__auto_type body = parserNode2IRStmt(ifNode->body);

		if (ifNode->el) {
			// Jump to end on true path
			connectCurrentsToNode(endBranch);

			// Connect cond to fBranch label
			setCurrentNodes(currentGen, condJ, NULL);
			connectCurrentsToNode(fBranch);

			setCurrentNodes(currentGen, fBranch, NULL);
			__auto_type elseBody = parserNode2IRStmt(ifNode->body);
			// Connect else to end
			connectCurrentsToNode(endBranch);
		}

		setCurrentNodes(currentGen, endBranch, NULL);
		return condJ;
	}
	case NODE_LIT_INT: {
		struct parserNodeLitInt *intLit = (void *)node;
		struct IRNodeValue il;
		il.base.attrs = NULL;
		il.base.type = IR_VALUE;
		il.val.type = IR_VAL_INT_LIT;
		il.val.value.intLit = intLit->value;

		return GRAPHN_ALLOCATE(il);
	}
	case NODE_LABEL: {
		struct parserNodeLabel *labNode = (void *)node;

		struct IRNodeLabel lab;
		lab.base.attrs = NULL;
		lab.base.type = IR_LABEL;

		__auto_type ptrStr = ptr2Str(labNode);
		mapParserNodeInsert(currentGen->labelsByParserNode, ptrStr,
		                    (struct parserNode *)node);
		strCharDestroy(&ptrStr);

		return GRAPHN_ALLOCATE(lab);
	}
	case NODE_LIT_STR: {
		struct parserNodeLitStr *str = (void *)node;
		if (str->isChar) {
			// Is a int literal
			struct parserNodeLitInt *intLit = (void *)node;
			struct IRNodeValue il;
			il.base.attrs = NULL;
			il.base.type = IR_VALUE;
			il.val.type = IR_VAL_INT_LIT;
			il.val.value.intLit.type = INT_ULONG;
			il.val.value.intLit.base = 10;
			il.val.value.intLit.value.uLong = 0;

			// Fill the literal with char bytes
			for (int i = 0; i != strlen(str->text); i++) {
				uint64_t chr = str->text[i];
				il.val.value.intLit.value.uLong |= chr << i * 8;
			}

			return GRAPHN_ALLOCATE(il);
		} else {
			struct IRNodeValue val;
			val.base.attrs = NULL;
			val.base.type = IR_VALUE;
			val.val = *addStringLiteralLabel(str->text);

			return GRAPHN_ALLOCATE(val);
		}
	}
	case NODE_MEMBER_ACCESS: {
		return insertVar(node, 0);
	}
	case NODE_SCOPE: {
		struct parserNodeScope *scope = (void *)node;

		graphNodeIR top = NULL;
		for (long i = 0; i != strParserNodeSize(scope->stmts); i++) {
			__auto_type body = parserNode2IRStmt(scope->stmts[i]);
			if (top == NULL)
				top = body;
		}

		return top;
	}
	case NODE_SUBSWITCH: {
		struct IRNodeSubSwit subSwitNode;
		subSwitNode.base.attrs = NULL;
		subSwitNode.base.type = IR_SUB_SWITCH_START_LABEL;

		return GRAPHN_ALLOCATE(subSwitNode);
	}
	case NODE_SWITCH: {
		struct parserNodeSwitch *swit = (void *)node;
		__auto_type condRes = parserNode2IRStmt(swit->exp);
		__auto_type condVar = IRCreateVirtVar(assignTypeToOp(swit->exp));
		IRAssign(IRCreateVarRef(condVar), condRes);
		return __createSwitchCodeAfterBody(IRCreateVarRef(condVar), node);
	}
	case NODE_TYPE_CAST: {
		struct parserNodeTypeCast *pnCast = (void *)node;

		struct IRNodeTypeCast cast;
		cast.base.attrs = NULL;
		cast.base.type = IR_TYPECAST;
		cast.in = assignTypeToOp(pnCast->exp);
		cast.out = pnCast->type;

		return GRAPHN_ALLOCATE(cast);
	}
	case NODE_VAR: {
		struct parserNodeVar *var = (void *)node;
		return IRCreateVarRef(var->var);
	}
	case NODE_WHILE: {
		struct parserNodeWhile *wh = (void *)node;

		__auto_type cond = parserNode2IRStmt(wh->cond);

		__auto_type startLab = IRCreateLabel();
		__auto_type endLab = IRCreateLabel();
		__auto_type cJmp = IRCondJump(cond, NULL, endLab);

		// Create scope for break statement
		struct IRGenScopeStack scope;
		scope.type = SCOPE_TYPE_LOOP;
		scope.value.loop.next = startLab;
		scope.value.loop.exit = endLab;
		currentGen->scopes = strScopeStackAppendItem(currentGen->scopes, scope);

		// Body
		parserNode2IRStmt(wh->body);

		// Pop
		currentGen->scopes = strScopeStackPop(currentGen->scopes, NULL);

		return cond;
	}
	}
	return NULL;
}
