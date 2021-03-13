#include "IR.h"
#include <assert.h>
#include "cleanup.h"
#include "exprParser.h"
#include "hashTable.h"
#include "linkedList.h"
#include "parse2IR.h"
#include "parserA.h"
#include <stdarg.h>
#include <stdio.h>
#include <setjmp.h>
#include "preprocessor.h"
typedef int (*gnIRCmpType)(const graphNodeIR *, const graphNodeIR *);
typedef int (*pnCmpType)(const struct parserNode **, const struct parserNode **);
static int ptrPtrCmp(const void *a, const void *b) {
		if(*(void**)a>*(void**)b)
				return 1;
		else if(*(void**)a<*(void**)b)
				return -1;
		return 0;
}
static struct enterExit __parserNode2IRStmt(const struct parserNode *node);
STR_TYPE_DEF(char, Char);
STR_TYPE_FUNCS(char, Char);
MAP_TYPE_DEF(enum IRNodeType, IRNodeType);
MAP_TYPE_FUNCS(enum IRNodeType, IRNodeType);
#define ALLOCATE(x)                                                                                                                                                \
	({                                                                                                                                                               \
			typeof(&x) ptr = calloc(sizeof(x),1);																																	\
		memcpy(ptr, &x, sizeof(x));                                                                                                                                    \
		ptr;                                                                                                                                                           \
	})
#define GRAPHN_ALLOCATE(x) ({ __graphNodeCreate(&x, sizeof(x), 0); })
MAP_TYPE_DEF(struct parserNode *, ParserNode);
MAP_TYPE_FUNCS(struct parserNode *, ParserNode);
MAP_TYPE_DEF(graphNodeIR, IRCase);
MAP_TYPE_FUNCS(graphNodeIR, IRCase);
PTR_MAP_FUNCS(struct parserNode *, graphNodeIR, GNIRByParserNode);
PTR_MAP_FUNCS(struct parserNode *, struct enterExit, EnterExitByParserNode);
PTR_MAP_FUNCS(struct parserNode *, strGraphNodeIRP, GNsByParserNode);
enum scopeType {
	SCOPE_TYPE_FUNC,
	SCOPE_TYPE_SCOPE,
	SCOPE_TYPE_LOOP,
	SCOPE_TYPE_SWIT,
	SCOPE_TYPE_SUB_SWIT,
};
struct IRGenScopeStack {
	enum scopeType type;
	union {
		struct {
			graphNodeIR next;
			graphNodeIR exit;
		} loop;
		struct {
			mapIRCase casesByRange;
			ptrMapGNIRByParserNode subSwitchsByParserNode;
			ptrMapEnterExitByParserNode subSwitchEnterCodeByParserNode;
			graphNodeIR switExitLab;
		} swit;
		struct {
			struct IRGenScopeStack *parentSwitch;
			graphNodeIR exit;
		} subSwit;
	} value;
};
STR_TYPE_DEF(struct IRGenScopeStack, ScopeStack);
STR_TYPE_FUNCS(struct IRGenScopeStack, ScopeStack);
struct gotoPair {
	strGraphNodeIRP dummyLabels;
	graphNodeIR jumpToLabel;
};
MAP_TYPE_DEF(struct gotoPair, GotoPair);
MAP_TYPE_FUNCS(struct gotoPair, GotoPair);
struct IRGenInst {
	strScopeStack scopes;
};
struct IRGenInst *IRGenInstCreate() {
	struct IRGenInst retVal;
	retVal.scopes = NULL;

	return ALLOCATE(retVal);
}
void IRGenInstDestroy(struct IRGenInst *inst) {
	strScopeStackDestroy(&inst->scopes);
	free(inst);
}

static __thread struct IRGenInst *currentGen = NULL;
static __thread strGraphNodeIRP currentStatement = NULL;
static __thread ptrMapGNIRByParserNode labelsByPN = NULL;
__thread strFileMappings currFileMappings;
void IRGenInit(strFileMappings mappings) {
	if (currentGen != NULL)
		IRGenInstDestroy(currentGen);
	ptrMapGNIRByParserNodeDestroy(labelsByPN, NULL);
	strGraphNodeIRPDestroy(&currentStatement);
	labelsByPN = ptrMapGNIRByParserNodeCreate();
	currentStatement = NULL;
	currentGen = IRGenInstCreate();

	if(currFileMappings)
			strFileMappingsDestroy(&currFileMappings);
	currFileMappings=strFileMappingsClone(mappings);
}
static struct enterExit insSrcMapping(long start,long end,struct enterExit pair) {
		if(currFileMappings) { 
				const char *fn=fileNameFromPos(currFileMappings, start);
				__auto_type mapping=IRCreateSourceMapping(fn, start, end-start);
				graphNodeIRConnect(mapping, pair.enter, IR_CONN_FLOW);
				pair.enter=mapping;
		}
		return pair;
}
static struct enterExit insSrcMappingsForBody(struct parserNode *node,struct enterExit pair) {
		if(node->type==NODE_BINOP||node->type==NODE_UNOP||node->type==NODE_FUNC_CALL||node->type==NODE_VAR_DECLS||node->type==NODE_VAR_DECL)
				return insSrcMapping(node->pos.start, node->pos.end, pair);
		return pair;
}
static struct IRGenScopeStack *IRGenScopePush(enum scopeType type) {
	struct IRGenScopeStack tmp;
	tmp.type = type;

	currentGen->scopes = strScopeStackAppendItem(currentGen->scopes, tmp);
	return &currentGen->scopes[strScopeStackSize(currentGen->scopes) - 1];
}
static void IRGenScopePop(enum scopeType type) {
	struct IRGenScopeStack top;
	currentGen->scopes = strScopeStackPop(currentGen->scopes, &top);
}

static strChar ptr2Str(const void *a) {
		long len=snprintf(NULL, 0, "%p", a);
		char buffer[len+1];
		sprintf(buffer, "%p", a);
		return strCharAppendData(NULL, buffer, len+1);
}
static strChar strClone(const char *str) {
	strChar buffer = strCharResize(NULL, strlen(str) + 1);
	strcpy(buffer, str);

	return buffer;
}
#define DFT_HASH_FORMAT "DFT"
#define CASE_HASH_FORMAT "%li-%li"
static strChar caseHash(const struct parserNode *cs) {
	if (cs->type == NODE_CASE) {
		struct parserNodeCase *cs2 = (void *)cs;
		char buffer[64];
		sprintf(buffer, CASE_HASH_FORMAT, cs2->valueLower, cs2->valueUpper);

		return strClone(buffer);
	} else if (cs->type == NODE_DEFAULT) {
		return strClone(DFT_HASH_FORMAT);
	}

	assert(0);
	return NULL;
}
//
// Attaches all entry nodes to a start node
//
static int visitNotVisitedOperand(const struct __graphNode *node, const struct __graphEdge *ed, const void *data) {
	strGraphNodeIRP visited = (void *)data;

	// Only visit operand /dest operand
	__auto_type edType = *graphEdgeIRValuePtr((void *)ed);
	switch (edType) {
	case IR_CONN_SOURCE_A:
	case IR_CONN_SOURCE_B:
	case IR_CONN_DEST:
	case IR_CONN_FUNC_ARG_1 ... IR_CONN_FUNC_ARG_128:
		break;
	default:
		return 0;
	}

	return NULL == strGraphNodeIRPSortedFind(visited, (void *)node, (gnIRCmpType)ptrPtrCmp);
}
static void visitNode(struct __graphNode *node, void *data) {
	strGraphNodeIRP visited = (void *)data;
	if (NULL == strGraphNodeIRPSortedFind(visited, (void *)node, (gnIRCmpType)ptrPtrCmp))
		visited = strGraphNodeIRPSortedInsert(visited, node, (gnIRCmpType)ptrPtrCmp);
}
static graphNodeIR enterStatement() {
	assert(currentStatement == NULL);

	__auto_type node = IRCreateStmtStart();

	currentStatement = strGraphNodeIRPAppendItem(currentStatement, node);

	return node;
}
static graphNodeIR leaveStatement(struct enterExit expr) {
	graphNodeIR popped;
	currentStatement = strGraphNodeIRPPop(currentStatement, &popped);

	graphNodeIRConnect(popped, expr.enter, IR_CONN_FLOW);
	__auto_type node = IRCreateStmtEnd(popped);
	graphNodeIRConnect(expr.exit, node, IR_CONN_FLOW);
	return node;
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
				tail = strCharConcat(strCharAppendItem(strClone(name->text), '.'), tail);
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
static mapIRNodeType unop2IRType;
static mapIRNodeType binop2IRType;
static mapIRNodeType assign2IRType;
static mapIRNodeType unopAssign2IRType;
void initParse2IR();
static struct object *U0Ptr;
void initParse2IR() {
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
	mapIRNodeTypeInsert(unop2IRType, "*", IR_DERREF);
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
	mapIRNodeTypeInsert(binop2IRType, "`", IR_POW);
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
static void IRCondJump(graphNodeIR cond, struct enterExit successLabel, struct enterExit failLabel) {
	struct IRNodeCondJump jump;
	jump.base.type = IR_COND_JUMP;
	jump.base.attrs = NULL;

	__auto_type jump2 = GRAPHN_ALLOCATE(jump);
	graphNodeIRConnect(cond, jump2, IR_CONN_COND);
	graphNodeIRConnect(jump2, successLabel.enter, IR_CONN_COND_TRUE);
	graphNodeIRConnect(jump2, failLabel.enter, IR_CONN_COND_TRUE);
};
STR_TYPE_DEF(long, Long);
STR_TYPE_FUNCS(long, Long);
static void getCases(strParserNode casesSubcases, strParserNode *cases, struct parserNode **dft) {
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
static void copyConnectionsIncomingTo(strGraphEdgeIRP incoming, graphNodeIR to) {
	for (long i = 0; i != strGraphEdgeIRPSize(incoming); i++) {
		__auto_type eV = *graphEdgeIRValuePtr(incoming[i]);
		__auto_type inNode = graphEdgeIRIncoming(incoming[i]);
		graphNodeIRConnect(inNode, to, eV);
	}
}
static void copyConnectionsOutcomingTo(strGraphEdgeIRP outgoing, graphNodeIR to) {
	for (long i = 0; i != strGraphEdgeIRPSize(outgoing); i++) {
		__auto_type eV = *graphEdgeIRValuePtr(outgoing[i]);
		__auto_type outNode = graphEdgeIROutgoing(outgoing[i]);
		graphNodeIRConnect(outNode, to, eV);
	}
}
PTR_MAP_FUNCS(graphNodeIR, struct parserNode *, ParserNodeByGN);
static struct enterExit __createSwitchCodeAfterBody(const struct parserNode *node) {
	struct parserNode *dft = NULL;
	struct enterExit retVal;

	__auto_type switchEndLabel = IRCreateLabel();

	// Exit enter
	struct parserNodeSwitch *swit = (void *)node;
	strParserNode cases = NULL;
	getCases(swit->caseSubcases, &cases, &dft);
	__auto_type cond = __parserNode2IRStmt(swit->exp);
	cond=insSrcMapping(swit->exp->pos.start, swit->exp->pos.end, cond);
	retVal.enter = cond.enter;

	// "Push scope"
	__auto_type newScope = IRGenScopePush(SCOPE_TYPE_SWIT);
	newScope->value.swit.subSwitchEnterCodeByParserNode = ptrMapEnterExitByParserNodeCreate();
	newScope->value.swit.casesByRange = mapIRCaseCreate();
	newScope->value.swit.subSwitchsByParserNode = ptrMapGNIRByParserNodeCreate();
	newScope->value.swit.switExitLab = switchEndLabel;
	// Parse Body
	__auto_type body = __parserNode2IRStmt(swit->body);
	body=insSrcMappingsForBody(swit->body,body);
	graphNodeIRConnect(body.exit, switchEndLabel, IR_CONN_FLOW);

	// Create a jump Table
	graphNodeIR tableNode = IRCreateJumpTable();
	graphNodeIRConnect(cond.exit, tableNode, IR_CONN_SOURCE_A);

	graphNodeIR *dftNode = mapIRCaseGet(newScope->value.swit.casesByRange, DFT_HASH_FORMAT);

	if (dftNode) {
	} else {
		// Jump to end label
		dftNode = &switchEndLabel;
	}

	// Collect list of sub-switchs
	ptrMapGNsByParserNode casesBySubSwitch = ptrMapGNsByParserNodeCreate(); // DEL
	ptrMapParserNodeByGN subSwitchsByIRPtr = ptrMapParserNodeByGNCreate();
	strGraphNodeIRP subs = NULL;
	struct parserNode *dftSubswitch = NULL;
	for (long i = 0; i != strParserNodeSize(cases); i++) {
		// If defualt,mark defualt as belonging to sub-switch
		if (cases[i]->type == NODE_DEFAULT) {
			__auto_type parent = ((struct parserNodeDefault *)cases[i])->parent;
			if (parent->type == NODE_SUBSWITCH)
				dftSubswitch = parent;

			continue;
		}

		struct parserNodeCase *cs = (void *)cases[i];

		// Check if sub-case
		if (cs->parent->type != NODE_SUBSWITCH)
			continue;

		// Get case label from earilier
		char buffer[64];
		sprintf(buffer, "%li-%li", cs->valueLower, cs->valueUpper);
		__auto_type label = *mapIRCaseGet(newScope->value.swit.casesByRange, buffer);
		ptrMapParserNodeByGNAdd(subSwitchsByIRPtr, label, cs->parent);
		subs = strGraphNodeIRPSortedInsert(subs, label, (gnIRCmpType)ptrPtrCmp);
	}

	long count;
	mapIRCaseKeys(newScope->value.swit.casesByRange, NULL, &count);
	const char *keys[count];
	mapIRCaseKeys(newScope->value.swit.casesByRange, keys, NULL);

	//
	// If sub-switches exist, have a variable that will tell if the sub-expression has been encountered
	// There will be 2 visits to the jump table,the first will execute subswitch-enter code by jumping to the enter code (and will jump back to start if in a
	// sub-switch) The second run wont jumpback. This allows for running the enter code once for every label
	// ```
	// run=0
	// jump-table:
	// [jump to label]
	//
	// sub-swtich-enter-code:
	// [code]
	// if(!run) {
	//     run=1
	//     goto jump-table
	// }
	//
	// sub-switch-label:
	// if(!run) {
	//     goto sub-swtich-enter-code
	// }
	// ```
	//
	struct parserVar *enteredSubCondition = NULL;
	if (strGraphNodeIRPSize(subs) != 0) {
			enteredSubCondition = IRCreateVirtVar(&typeI32i);
		__auto_type asn=IRCreateAssign(IRCreateIntLit(0),IRCreateVarRef(enteredSubCondition));
		graphNodeIRConnect(asn,retVal.enter,IR_CONN_FLOW);
		retVal.enter=IRStmtStart(asn);
	}

	for (long i = 0; i != count; i++) {
		// Keys are garneteed to exist
		__auto_type find = *mapIRCaseGet(newScope->value.swit.casesByRange, keys[i]);
		int isSubCase = NULL != strGraphNodeIRPSortedFind(subs, find, (gnIRCmpType)ptrPtrCmp);

		// Check if default
		if (0 == strcmp(DFT_HASH_FORMAT, keys[i])) {
			if (isSubCase)
				goto insertJumpBackCode;
			continue;
		}

		// Get range of case based on key
		long start, end;
		sscanf(keys[i], "%li-%li", &start, &end);

		// Connect jump table to case if not a sub-case
		if (!isSubCase) {
			// Connect to table
			struct IRJumpTableRange range;
			range.start = start, range.end = end, range.to = find;
			__auto_type table = (struct IRNodeJumpTable *)graphNodeIRValuePtr(tableNode);
			table->labels = strIRTableRangeAppendItem(table->labels, range);
			graphNodeIRConnect(tableNode, find, IR_CONN_CASE);
		} else {
			// Is a sub case

			// Connect case to table
			struct IRJumpTableRange range;
			range.start = start, range.end = end, range.to = find;
			__auto_type table = (struct IRNodeJumpTable *)graphNodeIRValuePtr(tableNode);
			table->labels = strIRTableRangeAppendItem(table->labels, range);
			graphNodeIRConnect(tableNode, find, IR_CONN_CASE);
			goto insertJumpBackCode;
		}

		continue;
	insertJumpBackCode : {
		// See above
		// ```
		// if(!run) {
		//     goto subSwitchcode
		// }
		//
		//
		__auto_type sub = ptrMapParserNodeByGNGet(subSwitchsByIRPtr, find);
		__auto_type enter = ptrMapEnterExitByParserNodeGet(newScope->value.swit.subSwitchEnterCodeByParserNode, *sub)->enter;
		if (enter) {
			__auto_type endIf = IRCreateLabel();
			// run=1,goto subSwitchCode

			// if(!run)
			__auto_type cond = IRCreateVarRef(enteredSubCondition);
			IRCreateCondJmp(cond, endIf, enter);

			// Insert after
			IRInsertAfter(find, IRStmtStart(cond), endIf, IR_CONN_FLOW);
		}
	}
	}
	count = ptrMapEnterExitByParserNodeSize(newScope->value.swit.subSwitchEnterCodeByParserNode);
	struct parserNode *keys2[count];
	ptrMapEnterExitByParserNodeKeys(newScope->value.swit.subSwitchEnterCodeByParserNode, keys2);
	if (count) {
		//
		// If we have sub-switchs,store result of cond in a variable so we dont have to recompute it upon re-entering switch
		// ```
		//  label
		// backup=cond
		// jump-table
		// ...
		// sub-switch-start:
		// if(!run) {run=1,goto label}
		// ```
		//

		//```
		// backup=cond
		//	label:
		// jumpTable(backup)
		//```
		struct parserVar *cond2Var = IRCreateVirtVar(IRNodeType(cond.exit));
		__auto_type cond2Store = IRCreateVarRef(cond2Var);
		graphNodeIRConnect(cond.exit, cond2Store, IR_CONN_DEST);

		__auto_type label = IRCreateLabel();
		graphNodeIRConnect(cond2Store, label, IR_CONN_FLOW);

		__auto_type cond2Load = IRCreateVarRef(cond2Var);
		graphNodeIRConnect(label, cond2Load, IR_CONN_FLOW);

		// Replace incoming connection to cond.eixt with cond2Load
		graphEdgeIRKill(cond.exit, tableNode, NULL, NULL, NULL);
		graphNodeIRConnect(cond2Load, tableNode, IR_CONN_SOURCE_A);

		for (long i = 0; i != count; i++) {
			__auto_type enterCode = *ptrMapEnterExitByParserNodeGet(newScope->value.swit.subSwitchEnterCodeByParserNode, keys2[i]);
			// run=1
			__auto_type assign = IRCreateAssign(IRCreateIntLit(1), IRCreateVarRef(enteredSubCondition));
			// goto switch-start(label)
			__auto_type cond = IRCreateUnop(IRCreateVarRef(enteredSubCondition), IR_LNOT);
			graphNodeIRConnect(assign, label, IR_CONN_FLOW);
			__auto_type endIf = IRCreateLabel();
			__auto_type condJmp = IRCreateCondJmp(cond, IRStmtStart(assign), endIf);
			IRInsertAfter(enterCode.exit, IRStmtStart(cond), endIf, IR_CONN_FLOW);
		}
	}

	graphNodeIRConnect(tableNode, *dftNode, IR_CONN_DFT);
	
	// Kill switch scope data
	mapIRCaseDestroy(newScope->value.swit.casesByRange, NULL);
	ptrMapGNIRByParserNodeDestroy(newScope->value.swit.subSwitchEnterCodeByParserNode, NULL);
	ptrMapGNIRByParserNodeDestroy(newScope->value.swit.subSwitchsByParserNode, NULL);
	// Pop scope
	IRGenScopePop(SCOPE_TYPE_SWIT);

	retVal.exit = switchEndLabel;
	return retVal;
}
graphNodeIR parserNode2Expr(const struct parserNode *node) {
	switch (node->type) {
	default:
		return NULL;
	case NODE_FUNC_FORWARD_DECL: {
			struct parserNodeFuncForwardDec *fwd=(void*)node;
			struct IRNodeValue val;
		val.base.attrs = NULL;
		val.base.type = IR_VALUE;
		val.val.type = IR_VAL_FUNC;
		val.val.value.func = fwd->func;

		__auto_type retVal = GRAPHN_ALLOCATE(val);
		return retVal;
	}
	case NODE_LIT_FLT: {
			struct parserNodeLitFlt *flt=(void*)node;
			return IRCreateFloat(flt->value);
	}
	case NODE_SIZEOF_EXP: {
		struct parserNodeSizeofExp *exp = (void *)node;
		__auto_type size = IRCreateIntLit(objectSize(assignTypeToOp(exp->exp), NULL));
		return size;
	}
	case NODE_SIZEOF_TYPE: {
		struct parserNodeSizeofType *t = (void *)node;
		__auto_type size = IRCreateIntLit(objectSize(t->type, NULL));
		return size;
	}
	case NODE_VAR: {
		struct parserNodeVar *var = (void *)node;
		return IRCreateVarRef(var->var);
	}
	case NODE_LIT_INT: {
		struct parserNodeLitInt *intLit = (void *)node;
		struct IRNodeValue il;
		il.base.attrs = NULL;
		il.base.type = IR_VALUE;
		il.val.type = IR_VAL_INT_LIT;
		il.val.value.intLit = intLit->value;

		__auto_type retVal = GRAPHN_ALLOCATE(il);
		return retVal;
	}
	case NODE_LIT_STR: {
		struct parserNodeLitStr *str = (void *)node;
		if (str->str.isChar) {
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
			for (int i = 0; i != __vecSize(str->str.text); i++) {
					uint64_t chr = ((char*)str->str.text)[i];
				il.val.value.intLit.value.uLong |= chr << i * 8;
			}

			__auto_type retVal = GRAPHN_ALLOCATE(il);
			return retVal;
		} else {
			struct IRNodeValue val;
			val.base.attrs = NULL;
			val.base.type = IR_VALUE;
			val.val.type=IR_VAL_STR_LIT;
			val.val.value.strLit = __vecAppendItem( NULL,  str->str.text, __vecSize(str->str.text));
			__auto_type retVal = GRAPHN_ALLOCATE(val);
			return retVal;
		}
	}
	case NODE_FUNC_REF: {
		struct parserNodeFuncRef *ref = (void *)node;

		struct IRNodeValue val;
		val.base.attrs = NULL;
		val.base.type = IR_VALUE;
		val.val.type = IR_VAL_FUNC;
		val.val.value.func = ref->func;

		__auto_type retVal = GRAPHN_ALLOCATE(val);
		return retVal;
	}
	case NODE_FUNC_CALL: {
		struct IRNodeFuncCall call;
		call.base.attrs = NULL;
		call.base.type = IR_FUNC_CALL;

		struct parserNodeFuncCall *call2 = (void *)node;
		__auto_type func = parserNode2Expr(call2->func);

		strGraphNodeIRP args = NULL;
		for (long i = 0; i != strParserNodeSize(call2->args); i++) {
			graphNodeIR arg = NULL;
			// If provided
			if (call2->args[i]) {
				arg = parserNode2Expr(call2->args[i]);
			} else {
				// Use dft
				struct objectFunction *funcType = (void *)assignTypeToOp(call2->func);
				assert(funcType->args[i].dftVal);
				arg = parserNode2Expr(funcType->args[i].dftVal);
			}

			args = strGraphNodeIRPAppendItem(args, arg);
		}

		// Connect args
		graphNodeIR callNode = GRAPHN_ALLOCATE(call);
		assert(strGraphNodeIRPSize(args) <= 128);
		for (long i = 0; i != strGraphNodeIRPSize(args); i++) {
			graphNodeIRConnect(args[i], callNode, IR_CONN_FUNC_ARG_1 + i);
		}

		// Connect func
		graphNodeIRConnect(__parserNode2IRStmt(call2->func).exit, callNode, IR_CONN_FUNC);

		return callNode;
	}
	case NODE_BINOP: {
		//
		// Remember to create statement if (createStatement)
		//
		graphNodeIR retVal = NULL;
		struct parserNodeBinop *binop = (void *)node;
		struct parserNodeOpTerm *op = (void *)binop->op;

		// Compute args
		__auto_type aVal = parserNode2Expr(binop->a);
		__auto_type bVal = parserNode2Expr(binop->b);

		// If non-assign binop
		__auto_type b = mapIRNodeTypeGet(binop2IRType, op->text);
		if (b) {
			__auto_type retVal = IRCreateBinop(aVal, bVal, *b);

			return retVal;
		}

		__auto_type assign = mapIRNodeTypeGet(assign2IRType, op->text);
		if (assign) {
			retVal = IRCreateAssign(IRCreateBinop(aVal, bVal, *assign), parserNode2Expr(binop->a));
			return retVal;
		} else if (0 == strcmp(op->text, "=")) {
			return IRCreateAssign(bVal, aVal);
		}

		assert(0);
	}
	case NODE_ARRAY_ACCESS: {
		struct parserNodeArrayAccess *access = (void *)node;
		__auto_type exp = parserNode2Expr(access->exp);
		__auto_type index = parserNode2Expr(access->index);
		return IRCreateArrayAccess(exp, index);
	}
	case NODE_UNOP: {
			struct parserNodeUnop *unop = (void *)node;
			struct parserNodeOpTerm *op = (void *)unop->op;
			__auto_type in = parserNode2Expr(unop->a);
			
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
					
					return newNode;
			} else {
					__auto_type find = mapIRNodeTypeGet(unop2IRType, op->text);
					graphNodeIR newNode;
					if (find) {
							struct IRNodeUnop unop;
							unop.base.type = *find;
							unop.base.attrs = NULL;
							newNode = GRAPHN_ALLOCATE(unop);
							graphNodeIRConnect(in, newNode, IR_CONN_SOURCE_A);
							return newNode;
			} else if (0 == strcmp(op->text, "&")) {
				return IRCreateAddrOf(in);
			}
		}
		assert(0);
		return NULL;
	}
	case NODE_TYPE_CAST: {
		struct parserNodeTypeCast *pnCast = (void *)node;

		struct IRNodeTypeCast cast;
		cast.base.attrs = NULL;
		cast.base.type = IR_TYPECAST;
		cast.in = assignTypeToOp(pnCast->exp);
		cast.out = pnCast->type;

		__auto_type tcNode = GRAPHN_ALLOCATE(cast);
		graphNodeIRConnect(parserNode2Expr(pnCast->exp), tcNode, IR_CONN_SOURCE_A);
		return tcNode;
	}
	case NODE_MEMBER_ACCESS: {
		struct parserNodeMemberAccess *access = (void *)node;
		assert(access->name->type == NODE_NAME);
		struct parserNodeName *name = (void *)access->name;
		__auto_type expr = parserNode2Expr(access->exp);
		struct parserNodeOpTerm *op = (void *)access->op;
		graphNodeIR retVal = NULL;
		retVal = IRCreateMemberAccess(expr, name->text);
		return retVal;
	}
	}
	return NULL;
}
static struct enterExit __parserNode2IRNoStmt(const struct parserNode *node);
static graphNodeIR createRepeatAccesses(struct parserVar *assignTo,strLong indexes) {
		__auto_type curr=IRCreateVarRef(assignTo);
		for(long i=0;i!=strLongSize(indexes);i++)
				curr=IRCreateArrayAccess(curr, IRCreateIntLit(indexes[i]));
		return curr;
}
static graphNodeIR dumpArrayLiterals(graphNodeIR *parent,struct parserVar *assignTo,strLong *currDim,long dimCount,struct parserNode *toDump) {
		if(toDump->type==NODE_ARRAY_LITERAL) {
				struct parserNodeArrayLit *lit=(void*)toDump;
				for(long i=0;i!=strParserNodeSize(lit->items);i++) {
						*currDim=strLongAppendItem(*currDim, i);
						dumpArrayLiterals(parent,assignTo,currDim,dimCount,lit->items[i]);
						*currDim=strLongPop(*currDim, NULL);
				}
		} else if(toDump->type==NODE_LIT_STR&&strLongSize(*currDim)!=dimCount) {
				struct parserNodeLitStr *str=(void*)toDump;
				for(long d=0;d!=__vecSize(str->str.text);d++) {
						*currDim=strLongAppendItem(*currDim, d);
								
						__auto_type access= createRepeatAccesses(assignTo, *currDim);
						IRCreateAssign(IRCreateIntLit(((unsigned char*)str->str.text)[d]), access);
						if(*parent)
								graphNodeIRConnect(*parent,IRStmtStart(access), IR_CONN_FLOW);
						*parent=access;

						*currDim=strLongPop(*currDim, NULL);
				}
		} else {
				assert(strLongSize(*currDim)==dimCount);
				//Not an array literal or array-literal-like string
				__auto_type access= createRepeatAccesses(assignTo, *currDim);
				IRCreateAssign(__parserNode2IRNoStmt(toDump).exit, access);		

				if(*parent)
						graphNodeIRConnect(*parent,IRStmtStart(access), IR_CONN_FLOW);
				*parent=access;
		}
		return *parent;
}
static struct enterExit varDecl2IR(const struct parserNode *node) {
	struct enterExit retVal;
	struct parserNodeVarDecl *decl = (void *)node;
	if(decl->type->type==TYPE_ARRAY) {
			strGraphNodeIRP dims CLEANUP(strGraphNodeIRPDestroy)=NULL;
			for(__auto_type type=decl->type;type->type==TYPE_ARRAY;) {
					struct objectArray *arr=(void*)type;
					dims=strGraphNodeIRPAppendItem(dims,__parserNode2IRStmt(arr->dim).exit);
					//Use base array
					type=arr->type;
			}
			dims=strGraphNodeIRPReverse(dims);
			__auto_type arr=IRCreateArrayDecl(((struct parserNodeVar*)decl->var)->var, decl->type, dims);
			
			//Assign initial value
			if(decl->dftVal) {
					assert(decl->dftVal->type==NODE_ARRAY_LITERAL);
					strGraphNodeIRP initialValues CLEANUP(strGraphNodeIRPDestroy)=NULL;
					struct objectArray  *arrType=(void*)decl->type;
					long depth=0;
					while(arrType->base.type==TYPE_ARRAY) {
							arrType=(void*)arrType->type;
							depth++;
					}
					strLong currDim CLEANUP(strLongDestroy)=NULL;
					graphNodeIR parent=arr;
					__auto_type end=dumpArrayLiterals(&parent,((struct parserNodeVar*)decl->var)->var,&currDim, depth, decl->dftVal);
					return (struct enterExit){IRStmtStart(arr),end};
			}
			
			return (struct enterExit){IRStmtStart(arr),arr};
	} else {
			__auto_type varRef = IRCreateVarRef(((struct parserNodeVar*)decl->var)->var);
			retVal.enter = retVal.exit = varRef;
			// Assign defualt value(if present)
			if (decl->dftVal) {
					__auto_type dft = parserNode2Expr(decl->dftVal);
					IRCreateAssign(dft, varRef);
					retVal.enter = IRStmtStart(dft);
			}
	}
	
	return retVal;
}
static struct enterExit __parserNode2IRStmt(const struct parserNode *node) {
	__auto_type retVal = __parserNode2IRNoStmt(node);
	return retVal;
}
const void *IR_ATTR_LABEL_NAME = "LABEL_NAME";
static void IRAttrLabelNameDestroy(struct IRAttr *attr) {
		struct IRAttrLabelName *nm = (void *)attr;
	free(nm->name);
}
static void debugShowGraphIR(graphNodeIR enter) {
		const char *name = tmpnam(NULL);
	__auto_type map = graphNodeCreateMapping(enter, 1);
	IRGraphMap2GraphViz(map, "viz", name, NULL, NULL, NULL, NULL);
	char buffer[1024];
	sprintf(buffer, "sleep 0.1 &&dot -Tsvg %s > /tmp/dot.svg && firefox /tmp/dot.svg & ", name);

	system(buffer);
}
static struct enterExit __parserNode2IRNoStmt(const struct parserNode *node) {
	switch (node->type) {
	case NODE_ARRAY_LITERAL: {
			__auto_type lab=IRCreateLabel();
			fputs("Array literals must be in initializer\n", stderr);
			abort();
			return  (struct enterExit){lab,lab};
	}
	case NODE_SIZEOF_EXP:
	case NODE_SIZEOF_TYPE:
	case NODE_LINKAGE:
	case NODE_CLASS_FORWARD_DECL:
	case NODE_UNION_FORWARD_DECL:
	case NODE_ASM_REG:
	case NODE_ASM_ADDRMODE:
		assert(0);
	case NODE_LIT_FLT: {
		struct parserNodeLitFlt *flt = (void *)node;
		__auto_type gn = IRCreateFloat(flt->value);
		return (struct enterExit){gn, gn};
	}
	case NODE_ASM: {
		// These are used to samwich local nodes between them
		__auto_type enterLab = IRCreateLabel();
		graphNodeIR current = enterLab;
		struct parserNodeAsm *Asm = (void *)node;
		for (long i = 0; i != strParserNodeSize(Asm->body); i++) {
			__auto_type pair = __parserNode2IRNoStmt(Asm->body[i]);
			graphNodeIRConnect(current, pair.enter, IR_CONN_FLOW);
			current = pair.exit;
		}
		__auto_type exitLab = IRCreateLabel();
		graphNodeIRConnect(current, exitLab, IR_CONN_FLOW);
		return (struct enterExit){enterLab, exitLab};
	}
	case NODE_ASM_BINFILE: {
		struct parserNodeAsmBinfile *bf = (void *)node;
		struct IRNodeAsmImport import;
		import.base.attrs = NULL;
		import.base.type = IR_ASM_IMPORT;
		struct parserNodeLitStr *lit = (void *)bf->fn;
		assert(lit->base.type == NODE_LIT_STR);
		import.fileName = calloc(__vecSize(lit->str.text) + 1,1);
		strcpy(import.fileName, (char*)lit->str.text);
		__auto_type gn = GRAPHN_ALLOCATE(import);
		__auto_type pair=(struct enterExit){gn, gn};
		pair=insSrcMapping(node->pos.start,node->pos.end,pair);
		return pair;
	}
	case NODE_ASM_DU8:
	case NODE_ASM_DU16:
	case NODE_ASM_DU32:
	case NODE_ASM_DU64: {
		long itemSize = 0;
		if (node->type == NODE_ASM_DU8)
			itemSize = 1;
		else if (node->type == NODE_ASM_DU16)
			itemSize = 2;
		else if (node->type == NODE_ASM_DU32)
			itemSize = 4;
		else if (node->type == NODE_ASM_DU64)
			itemSize = 8;
		else
			assert(0);
		struct parserNodeDUX *du = (void *)node;
		__auto_type count = __vecSize(du->bytes) / itemSize;
		void *clone = calloc(count * itemSize,1);
		memcpy(clone, du->bytes, count * itemSize);
		graphNodeIR gn;
		if (node->type == NODE_ASM_DU8) {
			struct IRNodeAsmDU8 du8;
			du8.base.attrs = NULL, du8.base.type = IR_ASM_DU8;
			du8.count = count, du8.data = clone;
			gn = GRAPHN_ALLOCATE(du8);
		} else if (node->type == NODE_ASM_DU16) {
			struct IRNodeAsmDU16 du16;
			du16.base.attrs = NULL, du16.base.type = IR_ASM_DU16;
			du16.count = count, du16.data = clone;
			gn = GRAPHN_ALLOCATE(du16);
		} else if (node->type == NODE_ASM_DU32) {
			struct IRNodeAsmDU32 du32;
			du32.base.attrs = NULL, du32.base.type = IR_ASM_DU32;
			du32.count = count, du32.data = clone;
			gn = GRAPHN_ALLOCATE(du32);
		} else if (node->type == NODE_ASM_DU64) {
			struct IRNodeAsmDU64 du64;
			du64.base.attrs = NULL, du64.base.type = IR_ASM_DU64;
			du64.count = count, du64.data = clone;
			gn = GRAPHN_ALLOCATE(du64);
		}
		return insSrcMapping(node->pos.start,node->pos.end,(struct enterExit){gn, gn});
	}
	case NODE_ASM_USE16:
	case NODE_ASM_USE32:
	case NODE_ASM_USE64:
	case NODE_ASM_ORG:
	case NODE_ASM_ALIGN:
	case NODE_ASM_IMPORT: {
		// Do nothing,this is for the assenbler and doesn't appear in the final result
		// Use a dummy label
		__auto_type lab = IRCreateLabel();
		return (struct enterExit){lab, lab};
	}
	case NODE_ASM_INST: {
		struct parserNodeAsmInstX86 *inst = (void *)node;
		strX86AddrMode addrModes = strX86AddrModeResize(NULL, strParserNodeSize(inst->args));
		for (long a = 0; a != strParserNodeSize(inst->args); a++)
			addrModes[a] = parserNode2X86AddrMode(inst->args[a]);
		struct parserNodeName *nm = (void *)inst->name;
		struct IRNodeX86Inst inst2;
		inst2.base.attrs = NULL;
		inst2.base.type = IR_X86_INST;
		inst2.args = addrModes;
		inst2.name = calloc(strlen(nm->text) + 1,1);
		strcpy(inst2.name, nm->text);
		__auto_type node = GRAPHN_ALLOCATE(inst2);
		return (struct enterExit){node, node};
	}
	case NODE_BREAK: {
		__auto_type lab = IRCreateLabel();
		__auto_type dummy = IRCreateLabel();
		graphNodeIRConnect(lab, dummy, IR_CONN_NEVER_FLOW);
		for (long i = strScopeStackSize(currentGen->scopes) - 1; i >= 0; i--) {
			if (currentGen->scopes[i].type == SCOPE_TYPE_LOOP) {
				graphNodeIRConnect(lab, currentGen->scopes[i].value.loop.exit, IR_CONN_FLOW);
				break;
			} else if (currentGen->scopes[i].type == SCOPE_TYPE_SWIT) {
				graphNodeIRConnect(lab, currentGen->scopes[i].value.swit.switExitLab, IR_CONN_FLOW);
				break;
			} else if (currentGen->scopes[i].type == SCOPE_TYPE_SUB_SWIT) {
				graphNodeIRConnect(lab, currentGen->scopes[i].value.subSwit.exit, IR_CONN_FLOW);
				break;
			}
		}
		return (struct enterExit){lab, dummy};
	}
	case NODE_GOTO: {
		// Create label,we will connect later after all labels are garenteed to exist
		__auto_type label = IRCreateLabel();
		struct parserNodeGoto *gt = (void *)node;
		// Parser nodes point to label name,so get the name out of the label
		assert(gt->pointsTo);
		__auto_type find = ptrMapGNIRByParserNodeGet(labelsByPN, gt->pointsTo);
		if (find) {
			graphNodeIRConnect(label, *find, IR_CONN_FLOW);
		} else {
			__auto_type pointsToLabel = IRCreateLabel();
			ptrMapGNIRByParserNodeAdd(labelsByPN, gt->pointsTo, pointsToLabel);
			graphNodeIRConnect(label, pointsToLabel, IR_CONN_FLOW);
		}

		__auto_type dummy = IRCreateLabel();
		graphNodeIRConnect(label, dummy, IR_CONN_NEVER_FLOW);

		return insSrcMapping(node->pos.start,node->pos.end,(struct enterExit){label, dummy});
	}
	case NODE_RETURN: {
		struct parserNodeReturn *retNode = (void *)node;
		graphNodeIR value = NULL;
		struct enterExit pair = {NULL, NULL};
		if (retNode->value) {
			pair = __parserNode2IRStmt(retNode->value);
			value = pair.exit;
		}

		pair.exit = IRCreateReturn(value, NULL); // TODO
		if (!pair.enter)
			pair.enter = pair.exit;

		return insSrcMapping(node->pos.start,node->pos.end,pair);
	}
	case NODE_DEFAULT:
	case NODE_CASE: {
		// Find switch scope for cases
		long i;
		for (i = strScopeStackSize(currentGen->scopes) - 1; i >= 0; i--) {
			if (currentGen->scopes[i].type == SCOPE_TYPE_SWIT)
				break;
		}
		assert(i != -1);

		strChar key CLEANUP(strCharDestroy) = caseHash(node);
		__auto_type retVal = IRCreateLabel();
		mapIRCaseInsert(currentGen->scopes[i].value.swit.casesByRange, key, retVal);
		return insSrcMapping(node->pos.start,node->pos.end,(struct enterExit){retVal, retVal});
	};
	case NODE_FUNC_DEF: {
		struct parserNodeFuncDef *def = (void *)node;
		struct IRNodeFuncStart start;
		start.base.attrs = NULL;
		start.base.type = IR_FUNC_START;
		start.func = def->func;
		start.end = NULL;
		__auto_type startNode = GRAPHN_ALLOCATE(start);

		graphNodeIR currentNode = startNode;
		// Assign arguments to variables
		struct objectFunction *func = (void *)def->funcType;
		for (long i = 0; i != strFuncArgSize(func->args); i++) {
			__auto_type arg = IRCreateFuncArg(func->args[i].type, i);
			__auto_type var = IRCreateVarRef(func->args[i].var);
			graphNodeIRConnect(currentNode, arg, IR_CONN_FLOW);
			graphNodeIRConnect(arg, var, IR_CONN_DEST);

			currentNode = var;
		}

		if(func->hasVarLenArgs) {
				__auto_type argcNode=IRCreateFuncVaArgArgc();
				__auto_type argcVar=IRCreateVarRef(func->argcVar);
				graphNodeIRConnect(argcNode,argcVar, IR_CONN_DEST);
				graphNodeIRConnect(currentNode, argcNode, IR_CONN_FLOW);
				currentNode=argcVar;
				
				__auto_type argvNode=IRCreateFuncVaArgArgv();
				__auto_type argvVar=IRCreateVarRef(func->argvVar);
				graphNodeIRConnect(argvNode,argvVar, IR_CONN_DEST);
				graphNodeIRConnect(currentNode, argvNode, IR_CONN_FLOW);
				currentNode=argvVar;
		}

		// Compute body
		__auto_type body = __parserNode2IRStmt(def->bodyScope);
		graphNodeIRConnect(currentNode, body.enter, IR_CONN_FLOW);
		//

		struct IRNodeFuncEnd end;
		end.base.attrs = NULL;
		end.base.type = IR_FUNC_END;
		__auto_type endNode = GRAPHN_ALLOCATE(end);
		((struct IRNodeFuncStart *)graphNodeIRValuePtr(startNode))->end = endNode;

		graphNodeIRConnect(body.exit, endNode, IR_CONN_FLOW);

		return (struct enterExit){startNode, endNode};
	}
	case NODE_DO: {
		struct parserNodeDo *doStmt = (void *)node;

		// Label
		__auto_type exitLabel = IRCreateLabel();
		__auto_type cond = __parserNode2IRStmt(doStmt->cond);
		insSrcMapping(doStmt->cond->pos.start,doStmt->cond->pos.end,cond);
		
		__auto_type scope = IRGenScopePush(SCOPE_TYPE_LOOP);
		scope->value.loop.exit = exitLabel;
		scope->value.loop.next = cond.enter;
		__auto_type body = __parserNode2IRStmt(doStmt->body);
		body=insSrcMappingsForBody(doStmt->body,body);
		IRGenScopePop(SCOPE_TYPE_LOOP);
		graphNodeIRConnect(body.exit, cond.enter, IR_CONN_FLOW);
		__auto_type cJump = IRCreateCondJmp(cond.exit, body.enter, exitLabel);
		return (struct enterExit){body.enter, exitLabel};
	}
	case NODE_TRY: {
			struct parserNodeTry *try=(void*)node;
			struct enterExit retVal;
			// I32i run noreg=1;
			// HCRT_ExceptStoreState();
			// if(run==0) {
			//     run=0
			//     try-body
			// } else {
			//     catch-body
			// }
			//
			__auto_type run=IRCreateVirtVar(&typeI8i);
			run->isNoreg=1;
			__auto_type runVar1=IRCreateVarRef(run);
			graphNodeIRConnect(IRCreateIntLit(1), runVar1, IR_CONN_DEST);
			retVal.enter=IRStmtStart(runVar1); 

			
			struct IRNodeX86Inst inst;
			inst.base.attrs=NULL;
			inst.base.type=IR_X86_INST;
			inst.name=strClone("CALL");
			inst.args=strX86AddrModeAppendItem(NULL, X86AddrModeLabel("HCRT_ExceptStoreState"));
			__auto_type instNode=GRAPHN_ALLOCATE(inst);
			graphNodeIRConnect(runVar1, instNode, IR_CONN_FLOW);
			
			struct enterExit tryBody=__parserNode2IRNoStmt(try->body);
			//Insert run=0 before body
			__auto_type runRef2=IRCreateVarRef(run);
			graphNodeIRConnect(IRCreateIntLit(0), runRef2, IR_CONN_DEST);
			IRInsertBefore(tryBody.enter,IRStmtStart(runRef2), runRef2, IR_CONN_FLOW);
			tryBody.enter=IRStmtStart(runRef2);
			
			struct enterExit catchBody=__parserNode2IRNoStmt(try->catch);
			
			__auto_type runRef3=IRCreateVarRef(run);
			graphNodeIRConnect(instNode, runRef3, IR_CONN_FLOW);
			IRCreateCondJmp(runRef3, tryBody.enter,  catchBody.enter);
			
			__auto_type lab=IRCreateLabel();
			graphNodeIRConnect(tryBody.exit, lab, IR_CONN_FLOW);
			graphNodeIRConnect(catchBody.exit, lab, IR_CONN_FLOW);
			retVal.exit=lab;
			return retVal;
	}
	case NODE_FOR: {
		struct enterExit retVal;

		struct IRNodeLabel lab;
		lab.base.type = IR_LABEL;
		lab.base.attrs = NULL;
		__auto_type labNext = GRAPHN_ALLOCATE(lab);
		__auto_type labExit = GRAPHN_ALLOCATE(lab);
		__auto_type labCond = GRAPHN_ALLOCATE(lab);

		struct parserNodeFor *forStmt = (void *)node;
		graphNodeIR current=IRCreateLabel();
		retVal.enter=current;
		if(forStmt->init) {
				__auto_type init = __parserNode2IRStmt(forStmt->init);
				init=insSrcMapping(forStmt->init->pos.start, forStmt->init->pos.end, init);
				retVal.enter = init.enter;
				__auto_type initExit=IRCreateLabel();
				current=init.exit;
				graphNodeIRConnect(current, initExit, IR_CONN_FLOW);
				current=initExit;
		}
		__auto_type trueLab = IRCreateLabel();
		graphNodeIR condEnter;
		if(forStmt->cond) {
				__auto_type cond = __parserNode2IRStmt(forStmt->cond);
				cond=insSrcMapping(forStmt->cond->pos.start, forStmt->cond->pos.end, cond);
				graphNodeIRConnect(current, cond.enter, IR_CONN_FLOW);
				IRCreateCondJmp(cond.exit, trueLab, labExit);
				condEnter=cond.enter;
		} else {
				graphNodeIRConnect(current, trueLab, IR_CONN_FLOW);
				graphNodeIRConnect(current, labExit, IR_CONN_NEVER_FLOW);
				condEnter=current;
		}
		
		__auto_type scope = IRGenScopePush(SCOPE_TYPE_LOOP);
		scope->value.loop.next = labNext;
		scope->value.loop.exit = labExit;

		// Enter body
		__auto_type body = __parserNode2IRStmt(forStmt->body);
		body=insSrcMappingsForBody(forStmt->body,body);
		graphNodeIRConnect(trueLab, body.enter, IR_CONN_FLOW);
		graphNodeIRConnect(body.exit, labNext, IR_CONN_FLOW);
		// Pop "scope"
		IRGenScopePop(0);

		// Inc code
		if(forStmt->inc) {
				__auto_type inc = __parserNode2IRStmt(forStmt->inc);
				graphNodeIRConnect(labNext, inc.enter, IR_CONN_FLOW);
				retVal.exit = labExit;
				// Connect increment to cond code
				graphNodeIRConnect(inc.exit, condEnter, IR_CONN_FLOW);
		} else {
				graphNodeIRConnect(labNext, condEnter, IR_CONN_FLOW);
				retVal.exit = labExit;
		}
		
		return retVal;
	}
	case NODE_IF: {
		struct enterExit retVal;
		struct parserNodeIf *ifNode = (void *)node;

		graphNodeIR endBranch = IRCreateLabel(), fBranch = NULL;
		// If no else,endBranch if false branch
		if (ifNode->el == NULL) {
			fBranch = endBranch;
		} else {
			fBranch = IRCreateLabel();
		}
		__auto_type body = __parserNode2IRStmt(ifNode->body);
		body=insSrcMappingsForBody(ifNode->body, body);
		__auto_type cond = __parserNode2IRStmt(ifNode->cond);
		cond=insSrcMapping(ifNode->cond->pos.start, ifNode->cond->pos.end, cond);
		__auto_type condJ = IRCreateCondJmp(cond.exit, body.enter, fBranch);
		retVal.enter = cond.enter;
		graphNodeIRConnect(body.exit, endBranch, IR_CONN_FLOW);
		if (ifNode->el) {
			__auto_type elseBody = __parserNode2IRStmt(ifNode->el);
			elseBody=insSrcMappingsForBody(ifNode->el, elseBody);
			graphNodeIRConnect(fBranch, elseBody.enter, IR_CONN_FLOW);
			graphNodeIRConnect(elseBody.exit, endBranch, IR_CONN_FLOW);
		} else {
			// If el is NULL,endBranch is already the else branch
		}

		retVal.exit = endBranch;
		return retVal;
	}
		// Share same type as NODE_LABEL
	case NODE_ASM_LABEL:
	case NODE_LABEL: {
		struct parserNodeLabel *labNode = (void *)node;
		struct parserNodeName *name = (void *)labNode->name;

		graphNodeIR lab = IRCreateLabel();
		// Insert into pairs
		__auto_type find = ptrMapGNIRByParserNodeGet(labelsByPN, (struct parserNode *)node);
		if (find)
			lab = *find;
		else
			ptrMapGNIRByParserNodeAdd(labelsByPN, (struct parserNode *)node, lab);

		struct IRAttrLabelName attr;
		attr.base.name = (void *)IR_ATTR_LABEL_NAME;
		attr.base.destroy = IRAttrLabelNameDestroy;
		attr.name = calloc(strlen(name->text) + 1,1);
		strcpy(attr.name, name->text);
		IRAttrReplace(lab, __llCreate(&attr, sizeof(attr)));

		return (struct enterExit){lab, lab};
	}
	case NODE_ASM_LABEL_LOCAL: {
		struct parserNodeLabelLocal *local = (void *)node;
		struct parserNodeName *name = (void *)local->name;
		struct IRNodeLabelLocal nv;
		nv.base.attrs = NULL;
		nv.base.type = IR_LABEL_LOCAL;
		__auto_type lab = GRAPHN_ALLOCATE(nv);

		// Insert into pairs
		__auto_type find = ptrMapGNIRByParserNodeGet(labelsByPN, (struct parserNode *)node);
		if (find)
			lab = *find;
		else
			ptrMapGNIRByParserNodeAdd(labelsByPN, (struct parserNode *)node, lab);

		return (struct enterExit){lab, lab};
	}
	case NODE_ASM_LABEL_GLBL: {
		struct parserNodeLabelGlbl *glbl = (void *)node;
		struct parserNodeName *name = (void *)glbl->name;
		graphNodeIR lab = IRCreateLabel();

		// Insert into pairs
		__auto_type find = ptrMapGNIRByParserNodeGet(labelsByPN, (struct parserNode *)node);
		if (find)
			lab = *find;
		else
			ptrMapGNIRByParserNodeAdd(labelsByPN, (struct parserNode *)node, lab);

		struct IRAttrLabelName attr;
		attr.base.name = (void *)IR_ATTR_LABEL_NAME;
		attr.base.destroy = IRAttrLabelNameDestroy;
		attr.name = calloc(strlen(name->text) + 1,1);
		strcpy(attr.name, name->text);
		IRAttrReplace(lab, __llCreate(&attr, sizeof(attr)));

		return (struct enterExit){lab, lab};
	}
	case NODE_SCOPE: {
		struct parserNodeScope *scope = (void *)node;

		graphNodeIR top = NULL, bottom = NULL;
		;
		for (long i = 0; i != strParserNodeSize(scope->stmts); i++) {
			__auto_type body = __parserNode2IRStmt(scope->stmts[i]);
			body=insSrcMappingsForBody(scope->stmts[i], body);
			if (top == NULL)
				top = body.enter;
			else
				graphNodeIRConnect(bottom, body.enter, IR_CONN_FLOW); // Bottom is old exit
			
			bottom = body.exit;
		}

		return (struct enterExit){top, bottom};
	}
	case NODE_SUBSWITCH: {
		struct IRNodeSubSwit subSwitNode;
		subSwitNode.base.attrs = NULL;
		subSwitNode.base.type = IR_SUB_SWITCH_START_LABEL;

		__auto_type scope = IRGenScopePush(SCOPE_TYPE_SUB_SWIT);
		__auto_type exitNode = IRCreateLabel();
		long i;
		for (i = strScopeStackSize(currentGen->scopes) - 1; i >= 0; i--)
			if (currentGen->scopes[i].type == SCOPE_TYPE_SUB_SWIT || currentGen->scopes[i].type == SCOPE_TYPE_SWIT)
				break;
		scope->value.subSwit.parentSwitch = &currentGen->scopes[i];
		scope->value.subSwit.exit = exitNode;
		long iSwitOnly;
		for (iSwitOnly = strScopeStackSize(currentGen->scopes) - 1; iSwitOnly >= 0; iSwitOnly--)
			if (currentGen->scopes[iSwitOnly].type == SCOPE_TYPE_SWIT)
				break;
		struct parserNodeSubSwitch *subSwit = (void *)node;
		__auto_type enterCode = parserNodes2IR(subSwit->startCodeStatements);
		ptrMapEnterExitByParserNodeAdd(currentGen->scopes[iSwitOnly].value.swit.subSwitchEnterCodeByParserNode, (struct parserNode *)node, enterCode);
		__auto_type bodyCode = parserNodes2IR(subSwit->body);
		graphNodeIRConnect(enterCode.exit, bodyCode.enter, IR_CONN_FLOW);
		IRGenScopePop(SCOPE_TYPE_SUB_SWIT);
		graphNodeIRConnect(bodyCode.exit, exitNode, IR_CONN_FLOW);
		return (struct enterExit){enterCode.enter, exitNode};
	}
	case NODE_SWITCH: {
		return __createSwitchCodeAfterBody(node);
	}
	case NODE_WHILE: {
		struct parserNodeWhile *wh = (void *)node;
		__auto_type cond = __parserNode2IRStmt(wh->cond);
		cond=insSrcMapping(wh->cond->pos.start, wh->cond->pos.end, cond);
		
		__auto_type endLab = IRCreateLabel();
		__auto_type scope = IRGenScopePush(SCOPE_TYPE_LOOP);
		scope->value.loop.exit = endLab;
		scope->value.loop.next = cond.enter;
		__auto_type body = __parserNode2IRStmt(wh->body);
		body=insSrcMappingsForBody(wh->body, body);
		currentGen->scopes = strScopeStackPop(currentGen->scopes, NULL);
		__auto_type cJmp = IRCreateCondJmp(cond.exit, body.enter, endLab);
		graphNodeIRConnect(body.exit, cond.enter, IR_CONN_FLOW);
		return (struct enterExit){cond.enter, endLab};
	}
	case NODE_COMMA_SEQ: {
		struct parserNodeCommaSeq *seq = (void *)node;

		graphNodeIR lastNode = NULL, firstNode = NULL;
		for (long i = 0; i != strParserNodeSize(seq->items); i++) {
			__auto_type node = parserNode2Expr(seq->items[i]);
			if (i == 0)
				firstNode = IRStmtStart(node);
			else
				graphNodeIRConnect(lastNode, IRStmtStart(node), IR_CONN_FLOW);
			lastNode = node;
		}

		return (struct enterExit){firstNode, lastNode};
	}
	case NODE_CLASS_DEF:
	case NODE_UNION_DEF:
	case NODE_FUNC_FORWARD_DECL: {
		__auto_type lab = IRCreateLabel();
		return (struct enterExit){lab, lab};
	}
	case NODE_KW:
	case NODE_META_DATA:
	case NODE_NAME:
	case NODE_OP:
	case NODE_UNOP:
	case NODE_BINOP:
	case NODE_FUNC_CALL:
	case NODE_FUNC_REF:
	case NODE_LIT_INT:
	case NODE_LIT_STR:
	case NODE_TYPE_CAST:
	case NODE_ARRAY_ACCESS:
	case NODE_MEMBER_ACCESS:
	case NODE_VAR: {
		__auto_type retVal = parserNode2Expr(node);
		//debugShowGraphIR(retVal);
		__auto_type start = IRStmtStart(retVal);
		return (struct enterExit){start, retVal};
	}
	case NODE_VAR_DECL: {
		return varDecl2IR(node);
	}
	case NODE_VAR_DECLS: {
		struct parserNodeVarDecls *decls = (void *)node;
		graphNodeIR currentNode = NULL;
		struct enterExit retVal = {NULL, NULL};
		for (long i = 0; i != strParserNodeSize(decls->decls); i++) {
			__auto_type tmp = varDecl2IR(decls->decls[i]);
			if (!retVal.enter)
				retVal.enter = tmp.enter;
			if (currentNode)
				graphNodeIRConnect(currentNode, tmp.enter, IR_CONN_FLOW);
			currentNode = tmp.exit;
			retVal.exit = tmp.exit;
		}
		return retVal;
	}
	}
	return (struct enterExit){NULL, NULL};
}
struct enterExit parserNodes2IR(strParserNode nodes) {
	parserMapGotosToLabels();
	struct enterExit retVal = {NULL, NULL};

	for (long i = 0; i != strParserNodeSize(nodes); i++) {
		if (nodes[i]) {
			__auto_type tmp = __parserNode2IRStmt(nodes[i]);
			tmp=insSrcMappingsForBody(nodes[i], tmp);
			if (!retVal.enter)
				retVal.enter = tmp.enter;
			if (retVal.exit)
				graphNodeIRConnect(retVal.exit, tmp.enter, IR_CONN_FLOW); // retVal.exit at this point contains previous exit
			retVal.exit = tmp.exit;
		}
	}

	return retVal;
}
