#include <assert.h>
#include <lexer.h>
#include <object.h>
#include <parserA.h>
#define DEBUG_PRINT_ENABLE 1
#include <cleanup.h>
#include <asmEmitter.h>
#include <debugPrint.h>
#include <diagMsg.h>
#include <exprParser.h>
#include <hashTable.h>
#include <opcodesParser.h>
#include <parserB.h>
#include <registers.h>
static __thread struct parserNode *currentScope = NULL;
static __thread struct parserNode *currentLoop = NULL;
MAP_TYPE_DEF(struct parserNode *, ParserNode);
MAP_TYPE_FUNCS(struct parserNode *, ParserNode);
MAP_TYPE_DEF(strParserNode, ParserNodes);
MAP_TYPE_FUNCS(strParserNode, ParserNodes);
static __thread mapParserNode localLabels = NULL;
static __thread mapParserNode labels = NULL;
static __thread mapParserNodes labelReferences = NULL;
static __thread int isAsmMode = 0;
static __thread mapParserNode asmImports = NULL;
static __thread int allowsArrayLiterals = 0;
static int isGlobalScope() {
	return currentScope == NULL;
}
static void addLabelRef(struct parserNode *node, const char *name) {
loop:;
	__auto_type find = mapParserNodesGet(labelReferences, name);
	if (!find) {
		mapParserNodesInsert(labelReferences, name, NULL);
		goto loop;
	}
	*find = strParserNodeAppendItem(*find, node);
}
void __initParserA() {
		allowsArrayLiterals=0;
		isAsmMode = 0;
	currentScope = NULL;
	currentLoop = NULL;
	mapParserNodeDestroy(asmImports, NULL);
	mapParserNodeDestroy(labels, NULL);
	mapParserNodeDestroy(localLabels, NULL);
	mapParserNodesDestroy(labelReferences, NULL); //
	labels = mapParserNodeCreate();
	localLabels = mapParserNodeCreate();
	labelReferences = mapParserNodesCreate();
	asmImports = mapParserNodeCreate();
}
static strParserNode switchStack = NULL;
static char *strCopy(const char *text) {
	char *retVal = malloc(strlen(text) + 1);
	strcpy(retVal, text);

	return retVal;
}
#define ALLOCATE(x)                                                                                                                                                \
	({                                                                                                                                                               \
		__auto_type len = sizeof(x);                                                                                                                                   \
		void *$retVal = malloc(len);                                                                                                                                   \
		memcpy($retVal, &x, len);                                                                                                                                      \
		$retVal;                                                                                                                                                       \
	})
static void assignPosByLexerItems(struct parserNode *node, llLexerItem start, llLexerItem end) {
	node->pos.start = llLexerItemValuePtr(start)->start;
	if (end)
		node->pos.end = llLexerItemValuePtr(end)->end;
	else
		node->pos.end = llLexerItemValuePtr(llLexerItemLast(start))->end;
}
static char *strClone(const char *str) {
	if (!str)
		return NULL;
	__auto_type len = strlen(str);
	char *retVal = malloc(len + 1);
	strcpy(retVal, str);
	return retVal;
}
// Expected a type that can used as a condition
static void getStartEndPos(llLexerItem start, llLexerItem end, long *startP, long *endP) {
	long endI, startI;
	if (end == NULL)
		endI = llLexerItemValuePtr(llLexerItemLast(start))->end;
	else
		endI = llLexerItemValuePtr(end)->start;
	startI = llLexerItemValuePtr(start)->start;

	if (startP)
		*startP = startI;
	if (endP)
		*endP = endI;
}
static void whineExpectedCondExpr(llLexerItem start, llLexerItem end, struct object *type) {
	long startI, endI;
	getStartEndPos(start, end, &startI, &endI);

	__auto_type typeText = object2Str(type);

	diagErrorStart(startI, endI);
	char buffer[1024];
	sprintf(buffer, "Type '%s' cannot be used as condition.", typeText);
	diagPushText(buffer);
	diagHighlight(startI, endI);
	diagEndMsg();
}
static void whineExpectedExpr(llLexerItem item) {
	if (item == NULL) {
		long end = diagInputSize();
		diagErrorStart(end, end);
		diagPushText("Expected expression but got end of input.");
		diagErrorStart(end, end);
		return;
	}

	__auto_type item2 = llLexerItemValuePtr(item);
	diagErrorStart(item2->start, item2->end);

	diagHighlight(item2->start, item2->end);
	diagPushText("Expected an expression,got ");

	diagPushQoutedText(item2->start, item2->end);
	diagPushText(".");

	diagHighlight(item2->start, item2->end);
	diagEndMsg();
}
static void whineExpected(llLexerItem item, const char *text) {
	char buffer[256];

	if (item == NULL) {
		long end = diagInputSize();
		diagErrorStart(end, end);
		sprintf(buffer, "Expected '%s',got end of input.", text);
		diagPushText(buffer);
		diagEndMsg();
		return;
	}

	__auto_type item2 = llLexerItemValuePtr(item);
	diagErrorStart(item2->start, item2->end);
	sprintf(buffer, "Expected '%s', got ", text);
	diagPushText(buffer);
	diagPushText(",got ");
	diagPushQoutedText(item2->start, item2->end);
	diagPushText(".");

	diagHighlight(item2->start, item2->end);

	diagEndMsg();
}
static struct parserNode *expectOp(llLexerItem _item, const char *text) {
	if (_item == NULL)
		return NULL;

	__auto_type item = llLexerItemValuePtr(_item);

	if (item->template == &opTemplate) {
		__auto_type opText = *(const char **)lexerItemValuePtr(item);
		if (0 == strcmp(opText, text)) {
			struct parserNodeOpTerm term;
			term.base.type = NODE_OP;
			term.base.pos.start = item->start;
			term.base.pos.end = item->end;
			term.text = text;

			return ALLOCATE(term);
		}
	}
	return NULL;
}
static struct parserNode *nameParse(llLexerItem start, llLexerItem end, llLexerItem *result);
static struct parserNode *parenRecur(llLexerItem start, llLexerItem end, llLexerItem *result);
static struct parserNode *literalRecur(llLexerItem start, llLexerItem end, llLexerItem *result) {
		if (result != NULL)
				*result = start;

		__auto_type item = llLexerItemValuePtr(start);
		if (item->template == &floatTemplate) {
				if (result != NULL)
						*result = llLexerItemNext(start);

				struct parserNodeLitFlt lit;
				lit.base.type = NODE_LIT_FLT;
				lit.value = ((struct lexerFloating *)lexerItemValuePtr(item))->value;
				lit.base.pos.start = item->start;
				lit.base.pos.end = item->end;

				return ALLOCATE(lit);
		} else if (item->template == &intTemplate) {
				if (result != NULL)
						*result = llLexerItemNext(start);

				struct parserNodeLitInt lit;
				lit.base.type = NODE_LIT_INT;
				lit.value = *(struct lexerInt *)lexerItemValuePtr(item);
				lit.base.pos.start = item->start;
				lit.base.pos.end = item->end;

				return ALLOCATE(lit);
		} else if (item->template == &opTemplate) {
				if(0==strcmp(*(char**)lexerItemValuePtr(item),"-")) {
						start=llLexerItemNext(start);
						struct parserNode *next CLEANUP(parserNodeDestroy)=literalRecur(start,end,result);
						if(next->type==NODE_LIT_INT) {
								struct parserNodeLitInt i=*(struct parserNodeLitInt*)next;
								i.base.pos.start=item->start;
								i.base.pos.end=next->pos.end;
								i.base.type=NODE_LIT_INT;
								i.value.value.sLong*=-1;
								return ALLOCATE(i);
						}
				}
		} else if (item->template == &strTemplate) {
				if (result != NULL)
						*result = llLexerItemNext(start);

				__auto_type str = *(struct parsedString *)lexerItemValuePtr(item);
				struct parserNodeLitStr lit;
				lit.base.type = NODE_LIT_STR;
				lit.str=str;
				//Clone the string,lexer item string will be free'd
				lit.str.text=__vecAppendItem(NULL, lit.str.text, __vecSize(lit.str.text));
				lit.base.pos.start = item->start;
				lit.base.pos.end = item->end;
		
				return ALLOCATE(lit);
		} else if (item->template == &nameTemplate) {
				__auto_type reg = parseAsmRegister(start, result);
				if (reg)
						return reg;

				__auto_type name = nameParse(start, end, result);
				// Look for var
				__auto_type findVar = parserGetVar(name);
				if (findVar) {
						struct parserNodeVar var;
						var.base.type = NODE_VAR;
						var.var = findVar;
						var.base.pos.start = name->pos.start;
						var.base.pos.end = name->pos.end;

						return ALLOCATE(var);
				}

				// Look for func
				__auto_type findFunc = parserGetFunc(name);
				if (findFunc) {
						struct parserNodeFuncRef ref;
						ref.base.pos.start = name->pos.start;
						ref.base.pos.end = name->pos.end;
						ref.base.type = NODE_FUNC_REF;
						ref.func = findFunc;
						ref.name = name;

						return ALLOCATE(ref);
				}

				return name;
		}  else if(allowsArrayLiterals) {
				__auto_type arrLit=parseArrayLiteral(start ,result);
				return arrLit;
		} else {
				return NULL;
		}

		// TODO add float template.
		return NULL;
}
static struct parserNode *prec0Binop(llLexerItem start, llLexerItem end, llLexerItem *result);
static struct parserNode *prec1Recur(llLexerItem start, llLexerItem end, llLexerItem *result);
static struct parserNode *prec2Recur(llLexerItem start, llLexerItem end, llLexerItem *result);
static struct parserNode *prec3Recur(llLexerItem start, llLexerItem end, llLexerItem *result);
static struct parserNode *prec4Recur(llLexerItem start, llLexerItem end, llLexerItem *result);
static struct parserNode *prec5Recur(llLexerItem start, llLexerItem end, llLexerItem *result);
static struct parserNode *prec6Recur(llLexerItem start, llLexerItem end, llLexerItem *result);
static struct parserNode *prec7Recur(llLexerItem start, llLexerItem end, llLexerItem *result);
static struct parserNode *prec8Recur(llLexerItem start, llLexerItem end, llLexerItem *result);
static struct parserNode *prec9Recur(llLexerItem start, llLexerItem end, llLexerItem *result);
static struct parserNode *prec10Recur(llLexerItem start, llLexerItem end, llLexerItem *result);
static struct parserNode *prec11Recur(llLexerItem start, llLexerItem end, llLexerItem *result);
static struct parserNode *prec12Recur(llLexerItem start, llLexerItem end, llLexerItem *result);
static struct parserNode *prec13Recur(llLexerItem start, llLexerItem end, llLexerItem *result);
STR_TYPE_DEF(int, Int);
STR_TYPE_FUNCS(int, Int);
static llLexerItem findOtherSide(llLexerItem start, llLexerItem end) {
		const char *lefts[] = {"(", "[","{"};
		const char *rights[] = {")", "]","}"};
	__auto_type count = sizeof(lefts) / sizeof(*lefts);

	strInt stack = NULL;
	int dir = 0;
	do {
		if (start == NULL)
			return NULL;
		if (start == end)
			return NULL;
		
		if (llLexerItemValuePtr(start)->template == &opTemplate) {
			__auto_type text = *(const char **)lexerItemValuePtr(llLexerItemValuePtr(start));
			int i;
			for (i = 0; i != count; i++) {
				if (0 == strcmp(lefts[i], text))
					goto foundLeft;
				if (0 == strcmp(rights[i], text))
					goto foundRight;
			}
		}  else if(llLexerItemValuePtr(start)->template==&kwTemplate) {
				__auto_type text=*(const char **)lexerItemValuePtr(llLexerItemValuePtr(start));
				int i;
				for (i = 0; i != count; i++) {
						if (0 == strcmp(lefts[i], text))
								goto foundLeft;
						if (0 == strcmp(rights[i], text))
								goto foundRight;
				}
		}
		goto next;
		foundLeft : {
			if (dir == 0)
				dir = 1;

			if (dir == 1)
				stack = strIntAppendItem(stack, 0);
			else
				stack = strIntResize(stack, strIntSize(stack) - 1);

			goto next;
		}
		foundRight : {
			if (dir == 0)
				dir = -1;

			if (dir == -1)
				stack = strIntAppendItem(stack, 0);
			else
				stack = strIntResize(stack, strIntSize(stack) - 1);
			goto next;
		}
	next:
		if (dir == 0)
			return NULL;

		if (strIntSize(stack) == 0) {
			return start;
		}
		if (dir == 0)
			return NULL;
		else if (dir == -1)
			start = llLexerItemPrev(start);
		else if (dir == 1)
			start = llLexerItemNext(start);
	} while (strIntSize(stack));

	// TODO whine about unbalanced
	return NULL;
}
static struct parserNode *precCommaRecur(llLexerItem start, llLexerItem end, llLexerItem *result) {
	__auto_type originalStart = start;

	if (start == NULL)
		return NULL;

	struct parserNodeCommaSeq seq;
	seq.base.type = NODE_COMMA_SEQ;
	seq.items = NULL;
	seq.type = NULL;
	seq.base.pos.start = llLexerItemValuePtr(start)->start;

	__auto_type node = NULL;
	for (; start != NULL && start != end;) {
		__auto_type comma = expectOp(start, ",");
		if (comma) {
			start = llLexerItemNext(start);
			seq.items = strParserNodeAppendItem(seq.items, node);
			node = NULL;
		} else if (node == NULL) {
			node = prec13Recur(start, end, &start);
			if (node == NULL)
				break;
		} else {
			break;
		}
	}
	if (result != NULL)
		*result = start;

	if (seq.items == NULL) {
		return node;
	} else {
		// Append last item
		seq.items = strParserNodeAppendItem(seq.items, node);

		if (start)
			seq.base.pos.end = llLexerItemValuePtr(start)->end;
		else
			seq.base.pos.end = llLexerItemValuePtr(llLexerItemLast(originalStart))->end;
		return ALLOCATE(seq);
	}
}
static struct parserNode *parenRecur(llLexerItem start, llLexerItem end, llLexerItem *result) {
	if (start == NULL)
		return NULL;

	struct parserNode *left = expectOp(start, "(");

	struct parserNode *right = NULL;
	struct parserNode *retVal = NULL;

	if (left != NULL) {
		__auto_type pairEnd = findOtherSide(start, end);
		if (pairEnd == NULL)
			goto fail;

		start = llLexerItemNext(start);
		llLexerItem res;
		retVal = precCommaRecur(start, pairEnd, &res);
		if (!retVal)
			goto fail;

		start = res;
		right = expectOp(res, ")");
		start = llLexerItemNext(start);
		if (!right)
			goto fail;

		retVal->pos.start = left->pos.start;
		retVal->pos.end = right->pos.end;
		if (result != NULL)
			*result = start;

		goto success;
	} else {
		retVal = literalRecur(start, end, result);
		goto success;
	}
fail:
	retVal = NULL;
success:

	return retVal;
}
struct parserNode *parseExpression(llLexerItem start, llLexerItem end, llLexerItem *result) {
	__auto_type res = precCommaRecur(start, end, result);
	if (res)
		assignTypeToOp(res);

	return res;
}
struct pnPair {
	struct parserNode *a, *b;
};
STR_TYPE_DEF(struct pnPair, PNPair);
STR_TYPE_FUNCS(struct pnPair, PNPair);
static strPNPair tailBinop(llLexerItem start, llLexerItem end, llLexerItem *result, const char **ops, long opCount,
                           struct parserNode *(*func)(llLexerItem, llLexerItem, llLexerItem *)) {
	strPNPair retVal = NULL;
	for (; start != NULL;) {
		struct parserNode *op;
		for (long i = 0; i != opCount; i++) {
			op = expectOp(start, ops[i]);
			if (op != NULL) {
				break;
			}
		}
		if (op == NULL)
			break;

		start = llLexerItemNext(start);
		__auto_type find = func(start, end, &start);
		if (find == NULL)
			goto fail;

		struct pnPair pair = {op, find};
		retVal = strPNPairAppendItem(retVal, pair);
	}

	if (result != NULL)
		*result = start;

	return retVal;
fail:
	return NULL;
}
static struct parserNode *nameParse(llLexerItem start, llLexerItem end, llLexerItem *result) {
	if (start == NULL)
		return NULL;

	__auto_type item = llLexerItemValuePtr(start);
	if (item->template == &nameTemplate) {
		if (result != NULL)
			*result = llLexerItemNext(start);

		__auto_type ptr = strClone(lexerItemValuePtr(item));
		struct parserNodeName retVal;
		retVal.base.type = NODE_NAME;
		retVal.base.pos.start = item->start;
		retVal.base.pos.end = item->end;
		retVal.text = ptr;

		return ALLOCATE(retVal);
	}
	return NULL;
}
static struct parserNode *pairOperator(const char *left, const char *right, llLexerItem start, llLexerItem end, llLexerItem *result, int *success, long *startP,
                                       long *endP) {
	if (success != NULL)
		*success = 0;

	if (start == NULL)
		return NULL;

	if (result != NULL)
		*result = start;

	llLexerItem result2;
	struct parserNode *l = NULL, *r = NULL, *exp = NULL;

	l = expectOp(start, left);
	result2 = llLexerItemNext(start);
	if (l == NULL)
		goto end;
	__auto_type pairEnd = findOtherSide(start, end);

	exp = precCommaRecur(result2, pairEnd, &result2);

	r = expectOp(result2, right);
	result2 = llLexerItemNext(result2);
	if (r == NULL)
		goto end;

	if (result != NULL)
		*result = result2;
	if (success != NULL)
		*success = 1;

end:
	if (r == NULL)
		exp = NULL;

	if (l && startP)
		*startP = l->pos.start;
	if (r && endP)
		*endP = r->pos.end;

	return exp;
}
static struct object *parseVarDeclTail(llLexerItem start, llLexerItem *end, struct object *baseType, struct parserNode **name, struct parserNode **dftVal,
                                       strParserNode *metaDatas);
static struct parserNode *expectKeyword(llLexerItem __item, const char *text) {
	if (__item == NULL)
		return NULL;

	__auto_type item = llLexerItemValuePtr(__item);

	if (item->template == &kwTemplate) {
		__auto_type itemText = *(const char **)lexerItemValuePtr(item);
		if (0 == strcmp(itemText, text)) {
			struct parserNodeKeyword kw;
			kw.base.type = NODE_KW;
			kw.base.pos.start = item->start;
			kw.base.pos.end = item->end;
			kw.text = text;

			return ALLOCATE(kw);
		}
	}
	return NULL;
}
static struct parserNode *parseSizeof(llLexerItem start, llLexerItem end, llLexerItem *result) {
	__auto_type original = start;
	struct parserNode *sz CLEANUP(parserNodeDestroy) = expectKeyword(start, "sizeof");
	if (!sz)
		return NULL;
	start = llLexerItemNext(start);
	struct parserNode *l CLEANUP(parserNodeDestroy) = expectOp(start, "(");
	__auto_type leftItem = start;
	if (!l) {
		whineExpected(start, "(");
	} else
		start = llLexerItemNext(start);

	struct parserNode *retVal = NULL;
	if (llLexerItemValuePtr(start)->template == &nameTemplate) {
		struct parserNode *nm CLEANUP(parserNodeDestroy) = nameParse(start, NULL, NULL);
		__auto_type type = objectByName(((struct parserNodeName *)nm)->text);
		if (type) {
			start = llLexerItemNext(start);
			type = parseVarDeclTail(start, &start, type, NULL, NULL, NULL);
			struct parserNodeSizeofType node;
			node.base.type = NODE_SIZEOF_TYPE;
			node.type = type;
			retVal = ALLOCATE(node);
		} else {
			__auto_type expr = parseExpression(start, NULL, &start);
			struct parserNodeSizeofExp node;
			node.base.type = NODE_SIZEOF_EXP;
			node.exp = expr;
			retVal = ALLOCATE(node);
		}
	} else {
		whineExpectedExpr(start);
		if (result)
			*result = start;
		return NULL;
	}
	struct parserNode *r CLEANUP(parserNodeDestroy) = expectOp(start, ")");
	if (r)
		start = llLexerItemNext(start);
	else
		whineExpected(start, ")");
	if (result)
		*result = start;
	assignPosByLexerItems(retVal, original, start);
	return retVal;
}

static struct parserNode *prec0Binop(llLexerItem start, llLexerItem end, llLexerItem *result) {
	if (start == NULL)
		return NULL;

	__auto_type sz = parseSizeof(start, end, result);
	if (sz)
		return sz;

	if (result != NULL)
		*result = start;

	llLexerItem result2;
	struct parserNode *head = parenRecur(start, end, &result2);
	if (head == NULL)
		return NULL;
	const char *binops[] = {".", "->"};
	const char *unops[] = {"--", "++"};
	strPNPair tails = NULL;
	for (; result2 != NULL && result2 != end;) {
		for (long i = 0; i != 2; i++) {
			__auto_type ptr = expectOp(result2, unops[i]);
			if (ptr != NULL) {
				result2 = llLexerItemNext(result2);

				struct parserNodeUnop unop;
				unop.a = head;
				unop.base.type = NODE_UNOP;
				unop.isSuffix = 1;
				unop.op = ptr;
				unop.type = NULL;
				unop.base.pos.start = head->pos.start;
				unop.base.pos.end = ptr->pos.end;

				head = ALLOCATE(unop);
				goto loop1;
			}
		}
		for (long i = 0; i != 2; i++) {
			__auto_type ptr = expectOp(result2, binops[i]);
			if (ptr)
				result2 = llLexerItemNext(result2);
			else
				continue;

			__auto_type next = nameParse(result2, end, &result2);
			if (next == NULL)
				goto fail;
			if (ptr != NULL) {
				struct parserNodeMemberAccess access;
				access.exp = head;
				access.base.type = NODE_MEMBER_ACCESS;
				access.base.pos.start = head->pos.start;
				access.base.pos.end = head->pos.end;
				access.name = next;
				access.op = ptr;

				head = ALLOCATE(access);
				goto loop1;
			}
		}

		// Check for typecast before func call
		struct parserNode *lP = expectOp(result2, "(");
		if (lP) {
			llLexerItem end2 = findOtherSide(result2, NULL);
			if (end2) {
				__auto_type item = llLexerItemValuePtr(llLexerItemNext(result2));
				if (item->template == &nameTemplate) {
					__auto_type baseType = objectByName((char *)lexerItemValuePtr(item));
					if (baseType != NULL) {
						strParserNode dims;
						long ptrLevel;
						struct parserNode *name;
						llLexerItem end3;

						struct parserNode *dft;
						strParserNode metas = NULL;
						__auto_type type = parseVarDeclTail(start, &end3, baseType, &name, &dft, &metas);
						result2 = end2;

						// Create type

						struct parserNodeTypeCast cast;
						cast.base.type = NODE_TYPE_CAST;
						cast.exp = head;
						cast.type = type;
						cast.base.pos.start = lP->pos.start;
						cast.base.pos.end = llLexerItemValuePtr(end2)->end;

						head = ALLOCATE(cast);

						// Move past ")"
						result2 = llLexerItemNext(end2);
						goto loop1;
					}
				}
			}
		}
		int success;
		long startP, endP;
		__auto_type funcCallArgs = pairOperator("(", ")", result2, end, &result2, &success, &startP, &endP);
		if (success) {
			struct parserNodeFuncCall newNode;
			newNode.base.type = NODE_FUNC_CALL;
			newNode.func = head;
			newNode.args = NULL;
			newNode.base.pos.start = startP;
			newNode.base.pos.end = endP;
			newNode.type = NULL;

			if (funcCallArgs != NULL) {
				if (funcCallArgs->type == NODE_COMMA_SEQ) {
					struct parserNodeCommaSeq *seq = (void *)funcCallArgs;
					for (long i = 0; i != strParserNodeSize(seq->items); i++)
						newNode.args = strParserNodeAppendItem(newNode.args, seq->items[i]);
				} else if (funcCallArgs != NULL) {
					newNode.args = strParserNodeAppendItem(newNode.args, funcCallArgs);
				}
			}

			head = ALLOCATE(newNode);
			goto loop1;
		}
		__auto_type oldResult2 = result2;
		__auto_type array = pairOperator("[", "]", result2, end, &result2, &success, &startP, &endP);
		if (success) {
			struct parserNodeArrayAccess access;
			access.exp = head;
			access.base.type = NODE_ARRAY_ACCESS;
			access.index = array;
			access.base.pos.start = startP;
			access.base.pos.end = endP;
			access.type = NULL;

			head = ALLOCATE(access);

			goto loop1;
		}

		// Nothing found
		break;
	loop1:;
	}

	if (result != NULL)
		*result = result2;
	return head;
fail:
	return NULL;
}
static struct parserNode *prec1Recur(llLexerItem start, llLexerItem end, llLexerItem *result) {
		__auto_type originalStart=start;
		if (start == NULL)
		return NULL;

	llLexerItem result2 = start;
	const char *unops[] = {"--", "++", "+", "-", "!", "~", "*", "&"};
	__auto_type count = sizeof(unops) / sizeof(*unops);
	strParserNode opStack = NULL;

	for (; result2 != NULL;) {
	loop1:
		for (long i = 0; i != count; i++) {
			__auto_type op = expectOp(result2, unops[i]);
			if (op != NULL) {
				result2 = llLexerItemNext(result2);
				opStack = strParserNodeAppendItem(opStack, op);
				goto loop1;
			}
		}
		break;
	}
	struct parserNode *tail = prec0Binop(result2, end, &result2);
	if (opStack != NULL) {
		if (tail == NULL) {
			goto fail;
		}

		for (long i = strParserNodeSize(opStack) - 1; i >= 0; i--) {
			struct parserNodeUnop unop;
			unop.base.type = NODE_UNOP;
			unop.a = tail;
			unop.isSuffix = 0;
			unop.op = opStack[i];
			unop.type = NULL;
			tail = ALLOCATE(unop);
			assignPosByLexerItems(tail, originalStart, result2);
		}
	}

	if (result != NULL)
		*result = result2;
	return tail;

fail:
	return tail;
}
static struct parserNode *binopLeftAssoc(const char **ops, long count, llLexerItem start, llLexerItem end, llLexerItem *result,
                                         struct parserNode *(*next)(llLexerItem, llLexerItem, llLexerItem *)) {
	if (start == NULL)
		return NULL;

	llLexerItem result2 = NULL;
	__auto_type head = next(start, end, &result2);
	strPNPair tail = NULL;
	if (head == NULL)
		goto end;

	tail = tailBinop(result2, end, &result2, ops, count, next);
	for (long i = 0; i != strPNPairSize(tail); i++) {
		struct parserNodeBinop binop;
		binop.base.type = NODE_BINOP;
		binop.a = head;
		binop.op = tail[i].a;
		binop.b = tail[i].b;
		binop.base.pos.start = head->pos.start;
		binop.base.pos.end = tail[i].b->pos.end;
		binop.type = NULL;

		head = ALLOCATE(binop);
	}
end:
	if (result != NULL)
		*result = result2;

	return head;
}
static struct parserNode *binopRightAssoc(const char **ops, long count, llLexerItem start, llLexerItem end, llLexerItem *result,
                                          struct parserNode *(*next)(llLexerItem, llLexerItem, llLexerItem *)) {
	if (start == NULL)
		return NULL;

	llLexerItem result2 = start;
	struct parserNode *retVal = NULL;
	__auto_type head = next(result2, end, &result2);

	__auto_type tail = tailBinop(result2, end, &result2, ops, count, next);
	if (tail == NULL) {
		retVal = head;
		goto end;
	}

	struct parserNode *right = NULL;
	for (long i = strPNPairSize(tail) - 1; i >= 0; i--) {
		if (right == NULL)
			right = tail[i].b;

		struct parserNode *left;
		if (i == 0)
			left = head;
		else
			left = tail[i - 1].b;

		struct parserNodeBinop binop;
		binop.a = left;
		binop.op = tail[i].a;
		binop.b = right;
		binop.base.type = NODE_BINOP;
		binop.base.pos.start = left->pos.start;
		binop.base.pos.end = right->pos.end;
		binop.type = NULL;

		right = ALLOCATE(binop);
	}

	retVal = right;
end:;
	if (result != NULL)
		*result = result2;

	return retVal;
}
#define LEFT_ASSOC_OP(name, next, ...)                                                                                                                             \
	static const char *name##Ops[] = {__VA_ARGS__};                                                                                                                  \
	static struct parserNode *name##Recur(llLexerItem start, llLexerItem end, llLexerItem *result) {                                                                 \
		__auto_type count = sizeof(name##Ops) / sizeof(*name##Ops);                                                                                                    \
		return binopLeftAssoc(name##Ops, count, start, end, result, next);                                                                                             \
	}
#define RIGHT_ASSOC_OP(name, next, ...)                                                                                                                            \
	static const char *name##Ops[] = {__VA_ARGS__};                                                                                                                  \
	static struct parserNode *name##Recur(llLexerItem start, llLexerItem end, llLexerItem *result) {                                                                 \
		__auto_type count = sizeof(name##Ops) / sizeof(*name##Ops);                                                                                                    \
		return binopRightAssoc(name##Ops, count, start, end, result, next);                                                                                            \
	}
RIGHT_ASSOC_OP(prec13, prec12Recur, "=", "-=", "+=", "*=", "/=", "%=", "<<=", ">>=", "&=", "|=", "^=");
LEFT_ASSOC_OP(prec12, prec11Recur, "||");
LEFT_ASSOC_OP(prec11, prec10Recur, "^^");
LEFT_ASSOC_OP(prec10, prec9Recur, "&&");
LEFT_ASSOC_OP(prec9, prec8Recur, "==", "!=");
LEFT_ASSOC_OP(prec8, prec7Recur, ">", "<", ">=", "<=");
LEFT_ASSOC_OP(prec7, prec6Recur, "+", "-");
LEFT_ASSOC_OP(prec6, prec5Recur, "|");
LEFT_ASSOC_OP(prec5, prec4Recur, "^");
LEFT_ASSOC_OP(prec4, prec3Recur, "&");
LEFT_ASSOC_OP(prec3, prec2Recur, "*", "%", "/");
LEFT_ASSOC_OP(prec2, prec1Recur, "`", "<<", ">>");

struct linkagePair {
	typeof(((struct linkage *)NULL)->type) link;
	const char *text;
};
static struct linkage getLinkage(llLexerItem start, llLexerItem *result) {
	if (result != NULL)
		*result = start;
	if (start == NULL)
		return (struct linkage){LINKAGE_LOCAL, NULL};

	if (llLexerItemValuePtr(start)->template != &kwTemplate)
		return (struct linkage){LINKAGE_LOCAL, NULL};
	;

	struct linkagePair pairs[] = {
	    {LINKAGE_PUBLIC, "public"},   {LINKAGE_STATIC, "static"}, {LINKAGE_EXTERN, "extern"},
	    {LINKAGE__EXTERN, "_extern"}, {LINKAGE_IMPORT, "import"}, {LINKAGE__IMPORT, "_import"},
	};
	__auto_type count = sizeof(pairs) / sizeof(*pairs);

	for (long i = 0; i != count; i++) {
		if (expectKeyword(start, pairs[i].text)) {
			start = llLexerItemNext(start);
			struct linkage retVal;
			retVal.type = pairs[i].link;
			retVal.fromSymbol = NULL;
			if (retVal.type == LINKAGE__EXTERN || retVal.type == LINKAGE__IMPORT) {
				struct parserNode *nm CLEANUP(parserNodeDestroy) = nameParse(start, NULL, &start);
				if (!nm) {
					whineExpected(start, "name");

				} else
					retVal.fromSymbol = strClone(((struct parserNodeName *)nm)->text);
			}
			if (result)
				*result = start;
			return retVal;
		}
	}

	return (struct linkage){LINKAGE_LOCAL, NULL};
	;
}
static void getPtrsAndDims(llLexerItem start, llLexerItem *end, struct parserNode **name, long *ptrLevel, strParserNode *dims) {
	long ptrLevel2 = 0;
	for (; start != NULL; start = llLexerItemNext(start)) {
		__auto_type op = expectOp(start, "*");
		if (op) {
			ptrLevel2++;
		} else
			break;
	}

	__auto_type name2 = nameParse(start, NULL, &start);

	strParserNode dims2 = NULL;
	for (;;) {
		__auto_type left = expectOp(start, "[");
		if (left == NULL)
			break;

		__auto_type dim = pairOperator("[", "]", start, NULL, &start, NULL, NULL, NULL);
		dims2 = strParserNodeAppendItem(dims2, dim);
	}

	if (dims != NULL)
		*dims = dims2;

	if (ptrLevel != NULL)
		*ptrLevel = ptrLevel2;

	if (name != NULL)
		*name = name2;

	if (end != NULL)
		*end = start;
}
static struct object *parseVarDeclTail(llLexerItem start, llLexerItem *end, struct object *baseType, struct parserNode **name, struct parserNode **dftVal,
                                       strParserNode *metaDatas);
struct parserNode *parseSingleVarDecl(llLexerItem start, llLexerItem *end) {
	__auto_type originalStart = start;
	struct parserNode *base;
	base = nameParse(start, NULL, &start);
	if (base != NULL) {
		struct parserNodeName *baseName = (void *)base;
		__auto_type baseType = objectByName(baseName->text);
		if (baseType == NULL)
			return NULL;

		struct parserNodeVarDecl decl;
		decl.base.type = NODE_VAR_DECL;
		decl.type = parseVarDeclTail(start, end, baseType, &decl.name, &decl.dftVal, &decl.metaData);

		if (end)
			assignPosByLexerItems((struct parserNode *)&decl, originalStart, *end);
		else
			assignPosByLexerItems((struct parserNode *)&decl, originalStart, NULL);
		return ALLOCATE(decl);
	}
	return NULL;
}
struct parserNode *parseVarDecls(llLexerItem start, llLexerItem *end) {
	__auto_type originalStart = start;

	struct parserNode *base;
	struct object *baseType = NULL;
	struct parserNode *cls = NULL;
	struct parserNode *un = NULL;
	int foundType = 0;
	base = nameParse(start, NULL, &start);

	if (base) {
		struct parserNodeName *baseName = (void *)base;
		baseType = objectByName(baseName->text);
		foundType = baseType != NULL;
	}
	__auto_type backupStart=start;
	cls = parseClass(start, &start, 0);
	if(!cls)
			start=backupStart;
	if (cls != NULL) {
			foundType = 1;

			if (cls->type == NODE_CLASS_DEF) {
					struct parserNodeClassDef *clsDef = (void *)cls;
					struct objectClass *cls=(void*)clsDef->type;
					cls->baseType=baseType;
					baseType = clsDef->type;
			} else if (cls->type == NODE_UNION_DEF) {
					struct parserNodeUnionDef *unDef = (void *)cls;
					struct objectUnion *un=(void*)unDef->type;
					un->baseType=baseType;
					baseType = unDef->type;
			}
	}

	if (foundType) {
		strParserNode decls;
		decls = NULL;

		for (int firstRun = 1;; firstRun = 0) {
			if (!firstRun) {
				struct parserNode *op;
				op = expectOp(start, ",");
				if (!op)
					goto end;

				start = llLexerItemNext(start);
			}

			struct parserNodeVarDecl decl;
			decl.base.type = NODE_VAR_DECL;
			__auto_type originalStart=start;
			decl.type = parseVarDeclTail(start, &start, baseType, &decl.name, &decl.dftVal, &decl.metaData);
			assignPosByLexerItems((struct parserNode*)&decl, originalStart, start);
			if (!decl.name) {
				struct parserNode *alloced = ALLOCATE(decl);
				parserNodeDestroy(&alloced);
				break;
			} else {
				decls = strParserNodeAppendItem(decls, ALLOCATE(decl));
			}
		}
	end:;
		if (end != NULL)
			*end = start;

		if (strParserNodeSize(decls) == 1) {
			return decls[0];
		} else if (strParserNodeSize(decls) > 1) {
			struct parserNodeVarDecls retVal;
			retVal.base.type = NODE_VAR_DECLS;
			retVal.decls = strParserNodeAppendData(NULL, (const struct parserNode **)decls, strParserNodeSize(decls));
			if (end)
				assignPosByLexerItems((struct parserNode *)&retVal, originalStart, *end);
			else
				assignPosByLexerItems((struct parserNode *)&retVal, originalStart, NULL);

			return ALLOCATE(retVal);
		} else {
			return cls;
		}
	}
fail:
	if (end != NULL)
		*end = start;

	return NULL;
}
static llLexerItem findEndOfExpression(llLexerItem start, int stopAtComma) {
	for (; start != NULL; start = llLexerItemNext(start)) {
		__auto_type item = llLexerItemValuePtr(start);
		__auto_type otherSide = findOtherSide(start, NULL);
		start = (otherSide != NULL) ? otherSide : start;

		if (stopAtComma)
			if (item->template == &opTemplate)
				if (0 == strcmp(",", *(const char **)lexerItemValuePtr(item)))
					return start;

		// TODO add floating template
		if (item->template == &intTemplate)
			continue;
		if (item->template == &strTemplate)
			continue;
		if (item->template == &nameTemplate)
			continue;
		if (item->template == &opTemplate)
			continue;
		if (item->template == &floatTemplate)
			continue;
		if(item->template==&kwTemplate) {
				if(0==strcmp(*(char**)lexerItemValuePtr(item),"sizeof"))
						continue;
		}
		return start;
	}

	return NULL;
} 
static int64_t intLitValue(struct parserNode *lit) {
	__auto_type node = (struct parserNodeLitInt *)lit;
	int64_t retVal;
	if (node->value.type == INT_SLONG)
		retVal = node->value.value.sLong;
	else if (node->value.type == INT_ULONG)
		retVal = node->value.value.uLong;
	else if (node->base.type == NODE_LIT_STR) {
			struct parserNodeLitStr *str=(void*)lit;
			retVal=0;
			for(long c=__vecSize(str->str.text)-1;c>=0;c--) {
					retVal<<=8;
					retVal|=((char*)str->str.text)[c];
			}
	}
	return retVal;
}
static int validateArrayDim(struct object *obj,struct parserNode *parent,strParserNode toValidate) {
		long expectedDim=-1;
		struct objectArray *arr=(void*)obj;
		if(arr->dim) {
				if(arr->dim->type==NODE_LIT_INT) {
						expectedDim=intLitValue(arr->dim);
				}
		}
		
		long firstLen=0;
		for(long v=0;v!=strParserNodeSize(toValidate);v++) {
				long len;
				if(toValidate[v]->type==NODE_LIT_STR) {
						struct parserNodeLitStr *str=(void*)toValidate[v];
						len=__vecSize(str->str.text);
				} if(toValidate[v]->type!=NODE_ARRAY_LITERAL) {
						diagErrorStart(toValidate[v]->pos.start, toValidate[v]->pos.end);
						diagPushText("Expected an array literal.");
						diagPushQoutedText(toValidate[v]->pos.start, toValidate[v]->pos.end);
						diagEndMsg();
						return 1;
				} else if(toValidate[v]->type==NODE_ARRAY_LITERAL) {
						struct parserNodeArrayLit *lit=(void*)toValidate[v];
						len=strParserNodeSize(lit->items);
				}
				if(expectedDim!=-1&&len!=expectedDim) {
								diagErrorStart(toValidate[v]->pos.start, toValidate[v]->pos.end);
								diagPushText("Dimension mismatch in array literal");
								diagPushQoutedText(toValidate[v]->pos.start, toValidate[v]->pos.end);
								diagEndMsg();
								return 1;
						}
				if(v==0) {
						firstLen=len;
				} else if(firstLen!=len) {
						diagErrorStart(toValidate[v]->pos.start, toValidate[v]->pos.end);
						diagPushText("Inconsistent array dim size.");
						diagPushQoutedText(toValidate[v]->pos.start, toValidate[v]->pos.end);
						diagEndMsg();
						return 1;
				}
		}
		if(!arr->dim) {
				struct parserNodeLitInt intLit;
				intLit.base.pos.start=0;
				intLit.base.pos.end=0;
				intLit.base.type=NODE_LIT_INT;
				intLit.value.type=INT_SLONG;
				intLit.value.value.sLong=firstLen;
				__auto_type lit=ALLOCATE(intLit);
				parserNodeDestroy(&arr->dim);
				arr->dim=lit;
		}

		if(arr->type->type==TYPE_ARRAY) {
				strParserNode next CLEANUP(strParserNodeDestroy)=NULL;
				for(long v=0;v!=strParserNodeSize(toValidate);v++) {
						//Should be array literals is didn't  return an error 
						struct parserNodeArrayLit *lit=(void*)toValidate[v];
						assert(lit->base.type==NODE_ARRAY_LITERAL);
						next=strParserNodeAppendData(next, (const struct parserNode **)lit->items, strParserNodeSize(lit->items));
				}
				validateArrayDim(arr->type, parent, next);
		}
		return 0;
}
static struct object *parseVarDeclTail(llLexerItem start, llLexerItem *end, struct object *baseType, struct parserNode **name, struct parserNode **dftVal,
                                       strParserNode *metaDatas) {
	int failed = 0;

	if (metaDatas != NULL)
		*metaDatas = NULL;

	__auto_type funcPtrLeft = expectOp(start, "(");
	long ptrLevel = 0;
	struct parserNode *eq;
	eq = NULL;
	strParserNode dims;
	dims = NULL;
	struct object *retValType = NULL;

	if (funcPtrLeft != NULL) {
		start = llLexerItemNext(start);

		getPtrsAndDims(start, &start, name, &ptrLevel, &dims);

		__auto_type r = expectOp(start, ")");
		if (r == NULL) {
			failed = 1;
			whineExpected(start, ")");
		} else {
			start = llLexerItemNext(start);
			failed = 0;
		}

		struct parserNode *l2;
		l2 = expectOp(start, "(");
		if (l2) {
			start = llLexerItemNext(start);
		} else {
			whineExpected(start, ")");
			failed = 1;
		}

		strFuncArg args;
		args = NULL;
		int foundDotDotDot=0;
		for (int firstRun = 1;; firstRun = 0) {
			struct parserNode *r2;
			r2 = expectOp(start, ")");
			if (r2 != NULL) {
				start = llLexerItemNext(start);
				break;
			}

			// Check for comma if not firt run
			if (!firstRun) {
				struct parserNode *comma;
				comma = expectOp(start, ",");
				if (comma == NULL) {
					whineExpected(start, ",");
				} else
					start = llLexerItemNext(start);
			}

			struct parserNode *dotdotdot CLEANUP(parserNodeDestroy)=expectKeyword(start, "...");
			if(dotdotdot) {
					foundDotDotDot=1;
					start=llLexerItemNext(start);
					r2 = expectOp(start, ")");
					if(!r2)
							whineExpected(start, ")");
					else
							start=llLexerItemNext(start);
					break;
			}
			
			struct parserNodeVarDecl *argDecl = (void *)parseSingleVarDecl(start, &start);
			if (argDecl == NULL)
				goto fail;

			struct objectFuncArg arg;
			arg.dftVal = argDecl->dftVal;
			arg.name = argDecl->name;
			arg.type = argDecl->type;

			args = strFuncArgAppendItem(args, arg);
		}
		retValType = objectFuncCreate(baseType, args,foundDotDotDot);
	} else {
		getPtrsAndDims(start, &start, name, &ptrLevel, &dims);
		retValType = baseType;
	}

	// Make type has pointers and dims
	for (long i = 0; i != ptrLevel; i++)
		retValType = objectPtrCreate(retValType);
	for (long i = strParserNodeSize(dims)-1;i>=0; i--)
			retValType = objectArrayCreate(retValType, dims[i],NULL);

	// Look for metaData
	strParserNode metaDatas2 = NULL;
metaDataLoop:;

	__auto_type metaName = nameParse(start, NULL, &start);
	if (metaName != NULL) {
		// Find end of expression,comma is reserved for next declaration.
		__auto_type expEnd = findEndOfExpression(start, 1);
		struct parserNode *metaValue = parseExpression(start, expEnd, &start);
		struct parserNodeMetaData meta;
		meta.base.type = NODE_META_DATA;
		meta.name = metaName;
		meta.value = metaValue;

		metaDatas2 = strParserNodeAppendItem(metaDatas2, ALLOCATE(meta));
		goto metaDataLoop;
	}
	// Look for default value

	eq = expectOp(start, "=");
	if (eq != NULL) {
		start = llLexerItemNext(start);
		__auto_type expEnd = findEndOfExpression(start, 1);
		struct parserNode *dftVal2=NULL;
		if(retValType->type==TYPE_ARRAY)  {
				allowsArrayLiterals=1;
				dftVal2=parseArrayLiteral(start, &start);
				allowsArrayLiterals=0;
				if(!dftVal2)
						dftVal2 = parseExpression(start, expEnd, &start);
		} else {
				dftVal2 = parseExpression(start, expEnd, &start);
		}
		
		//
		//If is an array type,if the array bounds are undefined we can infer them if array initializer is present
		//
		if(retValType->type==TYPE_ARRAY) {
				if(dftVal2) {
						if(dftVal2->type==NODE_ARRAY_LITERAL) {
								strParserNode array CLEANUP(strParserNodeDestroy)=strParserNodeAppendItem(NULL, dftVal2);
								validateArrayDim(retValType, dftVal2, array);
						}
				}
		}
		
		if (dftVal != NULL)
			*dftVal = dftVal2;
	} else {
		if (dftVal != NULL)
			*dftVal = NULL;
	}

	if (metaDatas != NULL)
		*metaDatas = metaDatas2;

	if (end != NULL)
		*end = start;

	return retValType;
fail:
	if (end != NULL)
		*end = start;
	return NULL;
}
static strObjectMember parseMembers(llLexerItem start, llLexerItem *end) {
	__auto_type decls = parseVarDecls(start, &start);
	if (decls != NULL) {
		strParserNode decls2 = NULL;
		if (decls->type == NODE_VAR_DECL) {
			decls2 = strParserNodeAppendItem(decls2, decls);
		} else if (decls->type == NODE_VAR_DECLS) {
			struct parserNodeVarDecls *d = (void *)decls;
			decls2 = strParserNodeAppendData(NULL, (const struct parserNode **)d->decls, strParserNodeSize(d->decls));
		}

		strObjectMember members = NULL;
		for (long i = 0; i != strParserNodeSize(decls2); i++) {
			if (decls2[i]->type != NODE_VAR_DECL)
				continue;

			struct objectMember member;

			struct parserNodeVarDecl *decl = (void *)decls2[i];
			struct parserNodeName *name = (void *)decl->name;
			member.name = NULL;
			if (name != NULL)
				if (name->base.type == NODE_NAME)
					member.name = strCopy(name->text);

			member.type = decl->type;
			member.offset = -1;
			member.attrs = NULL;

			for (long i = 0; i != strParserNodeSize(decl->metaData); i++) {
				struct parserNodeMetaData *meta = (void *)decl->metaData[i];
				assert(meta->base.type == NODE_META_DATA);

				struct objectMemberAttr attr;
				name = (void *)meta->name;
				assert(name->base.type == NODE_NAME);
				attr.name = strCopy(name->text);
				attr.value = meta->value;

				member.attrs = strObjectMemberAttrAppendItem(member.attrs, attr);
			}

			members = strObjectMemberAppendItem(members, member);
		}

		if (end != NULL)
			*end = start;
		return members;
	}
	if (end != NULL)
		*end = start;
	return NULL;
}
static void referenceType(struct object *type) {
	struct parserNodeName *existingName = NULL;
	if (type->type == TYPE_CLASS) {
		struct objectClass *cls = (void *)type;
		existingName = (void *)cls->name;
	} else if (type->type == TYPE_UNION) {
		struct objectUnion *un = (void *)type;
		existingName = (void *)un->name;
	} else if (type->type == TYPE_FORWARD) {
		struct objectForwardDeclaration *f = (void *)type;
		existingName = (void *)f->name;
	}

	if (existingName) {
		assert(existingName->base.type == NODE_NAME);
		diagNoteStart(existingName->base.pos.start, existingName->base.pos.end);
		diagPushText("Previous definition here:");
		diagHighlight(existingName->base.pos.start, existingName->base.pos.end);
		diagEndMsg();
	}
}
struct parserNode *parseClass(llLexerItem start, llLexerItem *end, int allowForwardDecl) {
	__auto_type originalStart = start;
	// Name for use with _extern/_impot
	struct parserNode *_fromSymbol;
	struct linkage link = getLinkage(start, &start);
	__auto_type baseName = nameParse(start, NULL, &start);
	struct parserNode *cls = NULL, *un = NULL;
	struct object *retValObj = NULL;
	struct parserNode *retVal = NULL;

	struct object *baseType = NULL;
	if (baseName != NULL) {
		struct parserNodeName *name2 = (void *)baseName;
		baseType = objectByName(name2->text);
		if (baseType == NULL)
			goto end;
	}
	cls = expectKeyword(start, "class");
	un = expectKeyword(start, "union");

	struct parserNode *name2 = NULL;
	if (cls || un) {
			start = llLexerItemNext(start);

			name2 = nameParse(start, NULL, &start);
			
			//Create a forward declaration So the class can be referenced within the class
			objectForwardDeclarationCreate(name2, (cls)?TYPE_CLASS:TYPE_UNION);
			
			struct parserNode *l = expectKeyword(start, "{");
		if (l)
			start = llLexerItemNext(start);
		if (l == NULL) {
			if (!allowForwardDecl)
				goto end;
			// Is a forward declaration(!?!)
			__auto_type semi = expectKeyword(start, ";");
			if (semi) {
				start = llLexerItemNext(start);

				struct parserNodeName *name = (void *)name2;
				__auto_type type = objectByName(name->text);
				struct object *forwardType = NULL;
				if (NULL == type) {
					forwardType = objectForwardDeclarationCreate(name2, (cls != NULL) ? TYPE_CLASS : TYPE_UNION);
				} else if (cls) {
					if (type->type != TYPE_CLASS)
						goto incompat;
				} else if (un) {
					if (type->type != TYPE_UNION)
						goto incompat;
				} else if (type->type == TYPE_FORWARD) {
					struct objectForwardDeclaration *f = (void *)type;
					if (f->type != (cls != NULL) ? TYPE_CLASS : TYPE_UNION)
						goto incompat;
					forwardType = type;
				} else {
					// Whine about forward declaration of incompatible existing type

				incompat:;
					diagErrorStart(name->base.pos.start, name->base.pos.end);
					diagPushText("Forward declaration ");
					diagPushQoutedText(name->base.pos.start, name->base.pos.end);
					diagPushText(" conflicts with existing type.");
					diagHighlight(name->base.pos.start, name->base.pos.end);
					diagEndMsg();

					referenceType(type);
				}
				if (cls) {
					struct parserNodeClassFwd fwd;
					fwd.base.type = NODE_CLASS_FORWARD_DECL;
					fwd.name = name2;
					fwd.type = forwardType;
					retVal = ALLOCATE(fwd);
				} else if (un) {
					struct parserNodeUnionFwd fwd;
					fwd.base.type = NODE_UNION_FORWARD_DECL;
					fwd.name = name2;
					fwd.type = forwardType;
					retVal = ALLOCATE(fwd);
				}
			}
			goto end;
		}

		strObjectMember members = NULL;
		int findOtherSide = 0;
		for (int firstRun = 1;; firstRun = 0) {
			struct parserNode *r = expectKeyword(start, "}");
			if (r != NULL) {
				findOtherSide = 1;
				start = llLexerItemNext(start);
				break;
			}

			__auto_type newMembers = parseMembers(start, &start);
			members = strObjectMemberConcat(members, newMembers);

			__auto_type kw = expectKeyword(start, ";");
			if (kw != NULL) {
				start = llLexerItemNext(start);
			} else {
				whineExpected(start, ";");
			}
		}

		struct parserNode *className = NULL;
		if (name2 != NULL) {
			className = name2;
		}
		
		if (cls) {
				retValObj = objectClassCreate(className, members, strObjectMemberSize(members));
		} else if (un) {
				retValObj = objectUnionCreate(className, members, strObjectMemberSize(members));
		}
	}
	if (cls) {
		struct parserNodeClassDef def;
		// retValObj is NULL if forward decl
		def.base.type = (retValObj) ? NODE_CLASS_DEF : NODE_CLASS_FORWARD_DECL;
		def.name = name2;
		def.type = retValObj;
		;

		retVal = ALLOCATE(def);
	} else if (un) {
		struct parserNodeUnionDef def;
		// retValObj is NULL if forward decl
		def.base.type = (retValObj) ? NODE_UNION_DEF : NODE_UNION_FORWARD_DECL;
		def.name = name2;
		def.type = retValObj;

		retVal = ALLOCATE(def);
	}
end:
	if (end != NULL)
		*end = start;
	if (retVal && (link.type != LINKAGE_LOCAL || isGlobalScope())) {
		parserAddGlobalSym(retVal, link);
	}

	if (retVal) {
		if (end)
			assignPosByLexerItems(retVal, originalStart, *end);
		else
			assignPosByLexerItems(retVal, originalStart, NULL);
	}
	return retVal;
}
static void addDeclsToScope(struct parserNode *varDecls, struct linkage link) {
	if (varDecls->type == NODE_VAR_DECL) {
		struct parserNodeVarDecl *decl = (void *)varDecls;
		parserAddVar(decl->name, decl->type);
		struct parserNodeVar var;
		var.base.pos=decl->name->pos;
		var.base.type=NODE_VAR;
		var.var=parserGetVar(decl->name);
		struct parserNode *varNode=ALLOCATE(var);
		decl->var=varNode;
		
		if (link.type != LINKAGE_LOCAL || isGlobalScope())
			parserAddGlobalSym(varNode, link);
	} else if (varDecls->type == NODE_VAR_DECLS) {
		struct parserNodeVarDecls *decls = (void *)varDecls;
		for (long i = 0; i != strParserNodeSize(decls->decls); i++) {
			struct parserNodeVarDecl *decl = (void *)decls->decls[i];
			parserAddVar(decl->name, decl->type);
			struct parserNodeVar var;
			var.base.pos=decl->name->pos;
			var.base.type=NODE_VAR;
			var.var=parserGetVar(decl->name);
			struct parserNode *varNode=ALLOCATE(var);
			decl->var = varNode;
			
			if (link.type != LINKAGE_LOCAL || isGlobalScope())
				parserAddGlobalSym(varNode, link);
		}
	}
}
struct parserNode *parseScope(llLexerItem start, llLexerItem *end, struct objectFunction *func) {
	struct parserNodeScope *oldScope = (void *)currentScope;

	struct parserNodeScope *retVal = malloc(sizeof(struct parserNodeScope));
	retVal->base.pos.start = -1;
	retVal->base.pos.end = -1;
	retVal->base.type = NODE_SCOPE;
	retVal->stmts = NULL;
	// Enter new scope
	currentScope = (void *)retVal;

	__auto_type originalStart = start;

	struct parserNode *lC = NULL, *rC = NULL;
	lC = expectKeyword(start, "{");

	if (lC) {
		enterScope();

		if(func)
				if(func->hasVarLenArgs)
						parserAddVarLenArgsVars2Func(&func->argcVar,&func->argvVar);
		
		// Add vars to scope
		if(func)
				for (long i = 0; i != strFuncArgSize(func->args); i++) {
						parserAddVar(func->args[i].name, func->args[i].type);
						func->args[i].var=parserGetVarByText(((struct parserNodeName*)func->args[i].name)->text);
				}

		start = llLexerItemNext(start);

		int foundOtherSide = 0;
		for (; start != NULL;) {
			rC = expectKeyword(start, "}");

			if (!rC) {
				__auto_type expr = parseStatement(start, &start);
				if (expr)
					retVal->stmts = strParserNodeAppendItem(retVal->stmts, expr);
			} else {
				foundOtherSide = 1;
				leaveScope();
				start = llLexerItemNext(start);
				break;
			}
		}

		if (!foundOtherSide) {
			struct parserNodeKeyword *kw = (void *)lC;
			diagErrorStart(kw->base.pos.start, kw->base.pos.end);
			diagPushText("Expecte other '}'.");
			diagHighlight(kw->base.pos.start, kw->base.pos.end);
			diagEndMsg();
		}
	}

	if (end != NULL)
		*end = start;

	if (retVal->stmts != NULL) {
		if (end)
			assignPosByLexerItems((struct parserNode *)retVal, originalStart, *end);
		else
			assignPosByLexerItems((struct parserNode *)retVal, originalStart, NULL);

		currentScope = (void *)oldScope;
		return (void *)retVal;
	}

	currentScope = (void *)oldScope;
	return NULL;
}
static void getSubswitchStartCode(struct parserNodeSubSwitch *sub) {
	strParserNode *nodes = &sub->body;
	for (long i = 0; i != strParserNodeSize(*nodes); i++) {
		if (nodes[0][i]->type == NODE_CASE || nodes[0][i]->type == NODE_DEFAULT) {
			// Found a case so yay
			sub->startCodeStatements = strParserNodeAppendData(NULL, (const struct parserNode **)&nodes[0][0], i);

			// Remove "consumed" items form nodes
			memmove(&nodes[0][0], &nodes[0][i], (strParserNodeSize(*nodes) - i) * sizeof(**nodes));
			*nodes = strParserNodeResize(*nodes, strParserNodeSize(*nodes) - i);
			return;
		}

		if (nodes[0][i]->type == NODE_SUBSWITCH) {
			diagErrorStart(sub->base.pos.start, sub->base.pos.end);
			diagPushText("Nested sub-switches are require cases between them.");
			diagEndMsg();
			return;
		}
	}

failLexical:
	return;
}
struct parserNode *parseStatement(llLexerItem start, llLexerItem *end) {
	// If end is NULL,make a dummy version for testing for ";" ahead
	llLexerItem endDummy;
	if (!end)
		end = &endDummy;

	__auto_type originalStart = start;

	__auto_type opcode = parseAsmInstructionX86(start, &start);
	if (opcode) {
		if (end != NULL)
			*end = start;
		return opcode;
	}

	__auto_type semi = expectKeyword(originalStart, ";");
	if (semi) {
		if (end != NULL)
			*end = llLexerItemNext(start);

		return NULL;
	}
	struct parserNode *retVal = NULL;
	__auto_type brk = parseBreak(originalStart, end);
	if (brk)
		return brk;

	__auto_type try=parseTry(originalStart , end);
	if(try)
			return try;
	
	__auto_type asmBlock = parseAsm(originalStart, end);
	if (asmBlock)
		return asmBlock;

	__auto_type funcStart=originalStart;
	struct linkage link = getLinkage(originalStart, &funcStart);
	__auto_type func = parseFunction(funcStart, end);
	if (func) {
			parserAddGlobalSym(func, link);
		return func;
	}

	__auto_type start2 = originalStart;
	 link = getLinkage(start2, &start2);
	__auto_type varDecls = parseVarDecls(start2, end);
	if (varDecls) {
		if (varDecls->type == NODE_VAR_DECL || varDecls->type == NODE_VAR_DECLS)
			addDeclsToScope(varDecls, link);

		if (end) {
			__auto_type semi = expectKeyword(*end, ";");
			if (!semi)
				whineExpected(*end, ";");
		}

		retVal = varDecls;
		goto end;
	}

	__auto_type gotoStmt = parseGoto(originalStart, end);
	if (gotoStmt) {
		retVal = gotoStmt;
		goto end;
	}

	__auto_type retStmt = parseReturn(originalStart, end);
	if (retStmt) {
		retVal = retStmt;
		goto end;
	}

	__auto_type labStmt = parseLabel(originalStart, end);
	if (labStmt) {
		retVal = labStmt;
		goto end;
	}

	start = originalStart;
	__auto_type expr = parseExpression(start, NULL, end);
	if (expr) {
		if (end) {
			__auto_type semi = expectKeyword(*end, ";");
			if (!semi)
				whineExpected(*end, ";");
		}

		retVal = expr;
		goto end;
	}

	start = originalStart;
	__auto_type ifStat = parseIf(start, end);
	if (ifStat) {
		retVal = ifStat;
		goto end;
	}

	start = originalStart;
 	__auto_type scope = parseScope(start, end, NULL);
	if (scope) {
		retVal = scope;
		goto end;
	}

	__auto_type forStmt = parseFor(originalStart, end);
	if (forStmt) {
		retVal = forStmt;
		goto end;
	}

	__auto_type whileStmt = parseWhile(originalStart, end);
	if (whileStmt) {
		retVal = whileStmt;
		goto end;
	}

	__auto_type doStmt = parseDo(originalStart, end);
	if (doStmt) {
		if (end) {
			__auto_type semi = expectKeyword(*end, ";");
			if (!semi)
				whineExpected(*end, ";");
		}

		retVal = doStmt;
		goto end;
	}

	__auto_type caseStmt = parseCase(originalStart, end);
	if (caseStmt) {
		retVal = caseStmt;
		goto end;
	}

	__auto_type stStatement = parseSwitch(originalStart, end);
	if (stStatement) {
		retVal = stStatement;
		goto end;
	}

	__auto_type cls = parseClass(originalStart, end, 1);
	if (cls) {
		retVal = cls;
		goto end;
	}

	return NULL;
end : {
	if (end) {
		__auto_type semi = expectKeyword(*end, ";");
		if (semi) {
			*end = llLexerItemNext(*end);
		}
	}
	return retVal;
}
}
struct parserNode *parseBreak(llLexerItem item, llLexerItem *end) {
	if (expectKeyword(item, "break")) {
		struct parserNodeBreak retVal;
		retVal.base.type = NODE_BREAK;
		__auto_type next = llLexerItemNext(item);
		assignPosByLexerItems((struct parserNode *)&retVal, item, next);
		if (!currentLoop && strParserNodeSize(switchStack) == 0) {
			diagErrorStart(retVal.base.pos.start, retVal.base.pos.end);
			diagPushText("Break appears in non-loop!");
			diagEndMsg();
		}
		if (!expectKeyword(next, ";")) {
			whineExpected(next, ";");
		} else
			next = llLexerItemNext(next);
		if (end)
			*end = next;

		return ALLOCATE(retVal);
	}
	return NULL;
}
struct parserNode *parseWhile(llLexerItem start, llLexerItem *end) {
	__auto_type originalStart = start;
	// Store old loop for push ahead(if while)
	__auto_type oldLoop = currentLoop;

	struct parserNodeWhile *retVal = NULL;

	struct parserNode *kwWhile = NULL, *lP = NULL, *rP = NULL, *cond = NULL, *body = NULL;
	kwWhile = expectKeyword(start, "while");

	int failed = 0;
	if (kwWhile) {
		start = llLexerItemNext(start);
		struct parserNodeWhile node;
		node.base.type = NODE_WHILE;
		node.body = NULL;
		node.cond = NULL;
		retVal = ALLOCATE(node);
		lP = expectOp(start, "(");
		if (!lP) {
			failed = 1;
			whineExpected(start, "(");
		} else
			start = llLexerItemNext(start);

		cond = parseExpression(start, NULL, &start);
		if (!cond) {
			failed = 1;
			whineExpectedExpr(start);
		}

		rP = expectOp(start, ")");
		if (!rP) {
			failed = 1;
			whineExpected(start, ")");
		}
		start = llLexerItemNext(start);

		// Push loop
		currentLoop = (struct parserNode *)retVal;
		body = parseStatement(start, &start);

		retVal->body = body;
		retVal->cond = cond;
		if (end)
			assignPosByLexerItems((struct parserNode *)retVal, originalStart, *end);
		else
			assignPosByLexerItems((struct parserNode *)retVal, originalStart, NULL);
	} else
		goto fail;

	goto end;
fail:
end:
	if (end != NULL)
		*end = start;

	// Restore old loop stack
	currentLoop = oldLoop;
	return (struct parserNode *)retVal;
}
struct parserNode *parseFor(llLexerItem start, llLexerItem *end) {
	__auto_type originalStart = start;
	// Store old loop for push/pop of loop(if for is present)
	__auto_type oldLoop = currentLoop;

	struct parserNode *lP = NULL, *rP = NULL, *kwFor = NULL, *semi1 = NULL, *semi2 = NULL, *cond = NULL, *inc = NULL, *body = NULL, *init = NULL;
	struct parserNode *retVal = NULL;

	kwFor = expectKeyword(start, "for");
	int leaveScope2 = 0;
	if (kwFor) {
		start = llLexerItemNext(start);

		lP = expectOp(start, "(");
		enterScope();
		leaveScope2 = 1;
		start = llLexerItemNext(start);

		__auto_type originalStart = start;
		init = parseVarDecls(originalStart, &start);
		if (!init)
			init = parseExpression(originalStart, NULL, &start);
		else
			addDeclsToScope(init, (struct linkage){LINKAGE_LOCAL, NULL});

		semi1 = expectKeyword(start, ";");
		start = llLexerItemNext(start);

		cond = parseExpression(start, NULL, &start);

		semi2 = expectKeyword(start, ";");
		start = llLexerItemNext(start);

		inc = parseExpression(start, NULL, &start);

		rP = expectOp(start, ")");
		start = llLexerItemNext(start);

		struct parserNodeFor forStmt;
		forStmt.base.type = NODE_FOR;
		forStmt.body = NULL;
		forStmt.cond = cond;
		forStmt.init = init;
		forStmt.inc = inc;

		retVal = ALLOCATE(forStmt);
		//"Push" loop
		currentLoop = retVal;
		body = parseStatement(start, &start);

		((struct parserNodeFor *)retVal)->body = body;
		if (end)
			assignPosByLexerItems(retVal, originalStart, *end);
		else
			assignPosByLexerItems(retVal, originalStart, NULL);
		goto end;
	}
fail:
end:
	if (leaveScope2)
		leaveScope();

	if (end != NULL)
		*end = start;

	// Restore old loop stack
	currentLoop = oldLoop;
	return retVal;
}
struct parserNode *parseDo(llLexerItem start, llLexerItem *end) {
	__auto_type originalStart = start;
	// Store old loop stack for push/pop
	__auto_type oldLoop = currentLoop;

	__auto_type kwDo = expectKeyword(start, "do");
	if (kwDo == NULL)
		return NULL;

	struct parserNodeDo doNode;
	doNode.base.type = NODE_DO;
	doNode.body = NULL;
	doNode.cond = NULL;
	struct parserNodeDo *retVal = ALLOCATE(doNode);

	currentLoop = (struct parserNode *)retVal;

	struct parserNode *body = NULL, *cond = NULL, *kwWhile = NULL, *lP = NULL, *rP = NULL;
	start = llLexerItemNext(start);
	body = parseStatement(start, &start);

	int failed = 0;

	kwWhile = expectKeyword(start, "while");
	if (kwWhile) {
		start = llLexerItemNext(start);
	} else {
		failed = 1;
		whineExpected(start, "while");
	}

	lP = expectOp(start, "(");
	if (!lP) {
		failed = 1;
		whineExpected(start, "(");
	} else
		start = llLexerItemNext(start);

	cond = parseExpression(start, NULL, &start);
	if (cond == NULL) {
		failed = 1;
		whineExpectedExpr(start);
	}

	rP = expectOp(start, ")");
	if (!rP) {
		failed = 1;
		whineExpected(start, ")");
	}
	start = llLexerItemNext(start);

	retVal->body = body;
	retVal->cond = cond;
	if (end)
		assignPosByLexerItems((struct parserNode *)retVal, originalStart, *end);
	else
		assignPosByLexerItems((struct parserNode *)retVal, originalStart, NULL);

	goto end;
fail:
end:

	if (end != NULL)
		*end = start;

	// Restore loop stack
	currentLoop = oldLoop;
	return (struct parserNode *)retVal;
}
struct parserNode *parseIf(llLexerItem start, llLexerItem *end) {
	__auto_type originalStart = start;

	__auto_type kwIf = expectKeyword(start, "if");
	struct parserNode *lP = NULL, *rP = NULL, *cond = NULL, *elKw = NULL, *elBody = NULL;

	struct parserNode *retVal = NULL;
	int failed = 0;
	if (kwIf) {
		start = llLexerItemNext(start);

		lP = expectOp(start, "(");
		if (!lP) {
			failed = 1;
			whineExpected(start, "(");
		}
		start = llLexerItemNext(start);

		cond = parseExpression(start, NULL, &start);
		if (!cond) {
			failed = 1;
			whineExpectedExpr(start);
		}

		rP = expectOp(start, ")");
		if (!rP) {
			failed = 1;
			whineExpected(start, ")");
		} else
			start = llLexerItemNext(start);

		__auto_type body = parseStatement(start, &start);

		elKw = expectKeyword(start, "else");
		if (elKw)
			start = llLexerItemNext(start);
		if (elKw) {
			elBody = parseStatement(start, &start);
			failed |= elBody == NULL;
		}

		struct parserNodeIf ifNode;
		ifNode.base.type = NODE_IF;
		ifNode.cond = cond;
		ifNode.body = body;
		ifNode.el = elBody;

		if (end)
			*end = start;

		if (end)
			assignPosByLexerItems((struct parserNode *)&ifNode, originalStart, *end);
		else
			assignPosByLexerItems((struct parserNode *)&ifNode, originalStart, NULL);

		// Dont free cond ahead
		cond = NULL;
		elBody = NULL;

		retVal = ALLOCATE(ifNode);
	}

	goto end;
fail:
	retVal = NULL;

end:;

	return retVal;
}
/**
 * Switch section
 */
static long getNextCaseValue(struct parserNode *parent) {
	struct parserNode *entry = NULL;

	if (parent->type == NODE_SWITCH) {
		struct parserNodeSwitch *node = (void *)parent;

		__auto_type len = strParserNodeSize(node->caseSubcases);
		if (len > 0) {
			entry = node->caseSubcases[len - 1];
			goto getValue;
		} else {
			return 0;
		}
	} else if (parent->type == NODE_SUBSWITCH) {
		struct parserNodeSubSwitch *node = (void *)parent;
		__auto_type len = strParserNodeSize(node->caseSubcases);
		if (len > 0) {
			entry = node->caseSubcases[len - 1];
			goto getValue;
		} else {
			return 0;
		}
	} else {
		assert(0);
	}
getValue : {
	if (entry->type == NODE_CASE) {
		struct parserNodeCase *node = (void *)entry;
		return node->valueUpper;
	} else if (entry->type == NODE_SUBSWITCH) {
		return getNextCaseValue(entry);
	} else {
		assert(0);
	}
	return -1;
}
}
struct parserNode *parseSwitch(llLexerItem start, llLexerItem *end) {
	__auto_type originalStart = start;
	struct parserNode *kw = NULL, *lP = NULL, *rP = NULL, *exp = NULL, *body = NULL, *retVal = NULL;

	kw = expectKeyword(start, "switch");
	int success = 0;
	if (kw) {
		success = 1;
		start = llLexerItemNext(start);

		lP = expectOp(start, "(");
		if (!lP) {
			success = 0;
			whineExpected(start, "(");
		} else
			start = llLexerItemNext(start);

		exp = parseExpression(start, NULL, &start);
		if (!exp)
			success = 0;

		rP = expectOp(start, ")");
		if (!rP) {
			success = 0;
			whineExpected(start, ")");
		} else
			start = llLexerItemNext(start);

		struct parserNodeSwitch swit;
		swit.base.type = NODE_SWITCH;
		swit.body = body;
		swit.caseSubcases = NULL;
		swit.dft = NULL;
		swit.exp = exp;
		retVal = ALLOCATE(swit);

		// Push to stack
		long oldStackSize = strParserNodeSize(switchStack);
		switchStack = strParserNodeAppendItem(switchStack, retVal);

		body = parseStatement(start, &start);

		// Whine about unterminated sub switches
		//+1 ignores current switch entry
		for (long i = oldStackSize + 1; i != strParserNodeSize(switchStack); i++) {
			assert(switchStack[i]->type == NODE_SUBSWITCH);

			struct parserNodeSubSwitch *sub = (void *)switchStack[i];
			assert(sub->start->type == NODE_LABEL);
			struct parserNodeLabel *lab = (void *)sub->start;
			struct parserNodeName *name = (void *)lab->name;

			diagErrorStart(name->base.pos.start, name->base.pos.end);
			diagPushText("Unterminated sub-switch.");
			diagHighlight(name->base.pos.start, name->base.pos.end);
			diagEndMsg();

			struct parserNodeKeyword *kw2 = (void *)kw;
			diagNoteStart(kw2->base.pos.start, kw2->base.pos.end);
			diagPushText("From here:");
			diagHighlight(kw2->base.pos.start, kw2->base.pos.end);
			diagEndMsg();
		}

		switchStack = strParserNodeResize(switchStack, oldStackSize);
		((struct parserNodeSwitch *)retVal)->body = body;
		if (retVal) {
			if (end)
				assignPosByLexerItems(retVal, originalStart, *end);
			else
				assignPosByLexerItems(retVal, originalStart, NULL);
		}
		if (end)
			*end = start;
	}
	return retVal;
}
static long searchForNode(const strParserNode nodes, const struct parserNode *node) {
	long retVal = 0;
	for (long i = 0; i != strParserNodeSize(nodes); i++)
		if (nodes[i] == node)
			return i;

	return -1;
}
static void addLabel(struct parserNode *node, const char *name) {
	__auto_type find = mapParserNodeGet(labels, name);
	assert(!find);
	mapParserNodeInsert(labels, name, node);
}
static void __parserMapLabels2Refs(int failOnNotFound) {
	long count;
	mapParserNodesKeys(labelReferences, NULL, &count);
	const char *keys[count];
	mapParserNodesKeys(labelReferences, keys, NULL);
	for (long i = 0; i != count; i++) {
		__auto_type refs = *mapParserNodesGet(labelReferences, keys[i]);
		__auto_type lab = mapParserNodeGet(labels, keys[i]);
		for (long g = 0; g != strParserNodeSize(refs); g++) {
			long start = refs[g]->pos.start, end = refs[g]->pos.end;
			switch (refs[g]->type) {
			case NODE_GOTO: {
				if (!lab)
					goto undefinedRef;
				struct parserNodeGoto *gt = (void *)refs[g];
				gt->pointsTo = *lab;
				break;
			}
			default:;
			}
			continue;
		undefinedRef:
			if (!failOnNotFound)
				continue;
			diagErrorStart(start, end);
			diagPushText("Undefined reference to label ");
			diagPushQoutedText(start, end);
			diagPushText(".");
			diagEndMsg();
		}
		if (lab) {
			strParserNodeDestroy(&refs);
			mapParserNodesRemove(labelReferences, keys[i], NULL);
		}
	}
}
struct parserNode *parseLabel(llLexerItem start, llLexerItem *end) {
	struct parserNode *colon1 = NULL, *retVal = NULL;
	__auto_type originalStart = start;
	struct parserNode *atAt CLEANUP(parserNodeDestroy) = expectKeyword(start, "@@");
	if (atAt) {
		start = llLexerItemNext(start);
		struct parserNode *name = nameParse(start, NULL, &start);
		if (!name) {
			if (end)
				*end = start;
			whineExpected(start, "name");
			return NULL;
		}
		struct parserNode *colon CLEANUP(parserNodeDestroy) = expectKeyword(start, ":");
		if (!colon) {
			if (end)
				*end = start;
			whineExpected(start, ":");
		} else
			start = llLexerItemNext(start);
		struct parserNodeName *nameNode = (void *)name;
		if (end)
			*end = start;
		if (!isAsmMode) {
			diagErrorStart(atAt->pos.start, atAt->pos.end);
			diagPushText("Local labels must appear in asm statement.");
			diagEndMsg();
		}
		if (mapParserNodeGet(localLabels, nameNode->text)) {
			diagErrorStart(atAt->pos.start, atAt->pos.end);
			diagPushText("Redefinition of local symbol ");
			diagPushQoutedText(name->pos.start, name->pos.end);
			diagPushText(".");
			diagEndMsg();
			parserNodeDestroy(&name);
			return NULL;
		} else {
			struct parserNodeLabelLocal local;
			getStartEndPos(originalStart, start, &local.base.pos.start, &local.base.pos.end);
			local.base.type = NODE_ASM_LABEL_LOCAL;
			local.name = name;
			__auto_type node = ALLOCATE(local);
			mapParserNodeInsert(localLabels, nameNode->text, node);
			addLabel(node, nameNode->text);

			return node;
		}
	}
	__auto_type name = nameParse(start, NULL, &start);
	if (name == NULL)
		return NULL;
	colon1 = expectKeyword(start, ":");
	__auto_type colonPos = start;
	if (!colon1)
		goto end;
	start = llLexerItemNext(start);
	{
		struct parserNode *colon2 CLEANUP(parserNodeDestroy) = expectKeyword(start, ":");
		if (colon2) {
			// Clear all local labels (remove their pointer from locals,dont actually free them)
			__parserMapLabels2Refs(0);
			// Remove them from the current scope
			long count;
			mapParserNodeKeys(localLabels, NULL, &count);
			const char *keys[count];
			mapParserNodeKeys(localLabels, keys, NULL);
			for (long l = 0; l != count; l++) {
				mapParserNodeRemove(labels, keys[l], NULL);
			}
			// Clear the local labels
			mapParserNodeDestroy(localLabels, NULL);
			localLabels = mapParserNodeCreate();

			start = llLexerItemNext(start);
			// Ensure doesn't already exist
			struct parserNodeName *nameNode = (void *)name;
			{
				if (end)
					*end = start;
				struct parserNodeLabelGlbl glbl;
				glbl.base.type = NODE_ASM_LABEL_GLBL;
				glbl.base.pos.start = name->pos.start, glbl.base.pos.end = colon2->pos.end;
				glbl.name = name;
				__auto_type glblNode = ALLOCATE(glbl);
				parserAddGlobalSym(glblNode, (struct linkage){LINKAGE_LOCAL, NULL});
				addLabel(glblNode, nameNode->text);
				if (!isAsmMode) {
					diagErrorStart(atAt->pos.start, atAt->pos.end);
					diagPushText("Local labels must appear in asm statement.");
					diagEndMsg();
				}
				struct linkage link;
				link.fromSymbol=NULL;
				link.type=LINKAGE_EXTERN;
				parserAddGlobalSym(glblNode, link);
				return glblNode;
			}
		}
	}

	struct parserNodeLabel lab;
	lab.base.type = NODE_LABEL;
	lab.name = name;
	retVal = ALLOCATE(lab);

	struct parserNodeName *name2 = (void *)name;
	if (0 == strcmp(name2->text, "start") && 0 != strParserNodeSize(switchStack)) {
		// Create sub-switch
		struct parserNodeSubSwitch sub;
		sub.base.type = NODE_SUBSWITCH;
		sub.caseSubcases = NULL;
		sub.dft = NULL;
		sub.start = retVal;
		sub.body = NULL;
		struct parserNode *top = switchStack[strParserNodeSize(switchStack) - 1];
		struct parserNode *sub2 = ALLOCATE(sub);
		retVal = sub2;
		if (top->type == NODE_SWITCH) {
			struct parserNodeSwitch *swit = (void *)top;
			swit->caseSubcases = strParserNodeAppendItem(swit->caseSubcases, sub2);
		} else if (top->type == NODE_SUBSWITCH) {
			struct parserNodeSubSwitch *sub = (void *)top;
			sub->caseSubcases = strParserNodeAppendItem(sub->caseSubcases, sub2);
		}

		switchStack = strParserNodeAppendItem(switchStack, retVal);
		// Continue untill find end:
		for (;;) {
			llLexerItem colonPos;
			struct parserNodeName *name = (void *)nameParse(start, NULL, &colonPos);
			if (name) {
				if (name->base.type == NODE_NAME) {
					if (0 == strcmp("end", name->text)) {
						__auto_type colon = expectKeyword(colonPos, ":");
						if (colon) {
							start = llLexerItemNext(colonPos);
							break;
						}
					}
				}
			}

			__auto_type stmt = parseStatement(start, &start);
			if (!stmt) {
				diagErrorStart(llLexerItemValuePtr(originalStart)->start, llLexerItemValuePtr(colonPos)->end);
				diagPushText("Expected end label to complete sub-switch.");
				diagEndMsg();
				break;
			}
			((struct parserNodeSubSwitch *)sub2)->body = strParserNodeAppendItem(((struct parserNodeSubSwitch *)sub2)->body, stmt);
		}

		getSubswitchStartCode((struct parserNodeSubSwitch *)retVal);
		switchStack = strParserNodePop(switchStack, NULL);
	} else {
		addLabel(retVal, ((struct parserNodeName *)name)->text);
	};
end:
	// Check if success
	if (!retVal)
		start = originalStart;
	if (end != NULL)
		*end = start;

	if (retVal) {
		if (end)
			assignPosByLexerItems(retVal, originalStart, *end);
		else
			assignPosByLexerItems(retVal, originalStart, NULL);
	}

	return retVal;
}
static void ensureCaseDoesntExist(long valueLow, long valueHigh, long rangeStart, long rangeEnd) {
	long len = strParserNodeSize(switchStack);
	for (long i = len - 1; i >= 0; i--) {
		strParserNode cases = NULL;
		if (switchStack[i]->type == NODE_SWITCH) {
			cases = ((struct parserNodeSwitch *)switchStack[i])->caseSubcases;
		} else if (switchStack[i]->type == NODE_SUBSWITCH) {
			cases = ((struct parserNodeSubSwitch *)switchStack[i])->caseSubcases;
		} else
			assert(0);

		for (long i = 0; i != strParserNodeSize(cases); i++) {
			if (cases[i]->type == NODE_CASE) {
				struct parserNodeCase *cs = (void *)cases[i];

				int consumed = 0;
				// Check if high is in range
				consumed |= valueHigh <= cs->valueUpper && valueHigh >= cs->valueLower;
				// Check if low is in range
				consumed |= valueLow <= cs->valueUpper && valueLow >= cs->valueLower;
				// Check if range is consumed
				consumed |= valueHigh >= cs->valueUpper && valueLow <= cs->valueLower;
				if (consumed) {
					struct parserNodeKeyword *kw = (void *)cs->label;
					diagErrorStart(rangeStart, rangeEnd);
					diagPushText("Conflicting case statements.");
					diagHighlight(rangeStart, rangeEnd);
					diagEndMsg();

					diagNoteStart(kw->base.pos.start, kw->base.pos.end);
					diagPushText("Previous case here: ");
					diagHighlight(kw->base.pos.start, kw->base.pos.end);
					diagEndMsg();
					goto end;
				}
			}
		}
	}
end:;
}
static void whineCaseNoSwitch(const struct parserNode *kw, long start, long end) {
	diagErrorStart(start, end);
	const struct parserNodeKeyword *kw2 = (void *)kw;
	char buffer[128];
	sprintf(buffer, "Unexpected '%s'. Not in switch statement. ", kw2->text);
	diagPushText(buffer);
	diagHighlight(start, end);
	diagEndMsg();
}
static int64_t intLitValue(struct parserNode *lit);
struct parserNode *parseCase(llLexerItem start, llLexerItem *end) {
	__auto_type originalStart = start;

	struct parserNode *kwCase = NULL, *colon = NULL, *dotdotdot = NULL;
	struct parserNode *retVal = NULL;
	int failed = 0;
	kwCase = expectKeyword(start, "case");
	/**
	 * Get parent switch
	 */
	struct parserNode *parent = NULL;
	if (strParserNodeSize(switchStack) == 0) {
	} else {
		parent = switchStack[strParserNodeSize(switchStack) - 1];
	}
	if (kwCase) {
		start = llLexerItemNext(start);

		// Used later
		__auto_type valueStart = start;

		int gotInt = 0;
		long caseValue = -1, caseValueUpper = -1;
		if (start != NULL) {
				struct parserNode *lit CLEANUP(parserNodeDestroy)=literalRecur(start, NULL, &start);
				if (lit) {
						if(lit->type==NODE_LIT_INT)
								gotInt = 1;
						if(lit->type==NODE_LIT_STR)
								if(((struct parserNodeLitStr*)lit)->str.isChar)
										gotInt = 1;

						caseValue = intLitValue(lit);

						if(!gotInt)
								whineExpected(start, "integer");
			} else if (parent) {
				caseValue = getNextCaseValue(parent);
			}
		}

		dotdotdot = expectKeyword(start, "...");
		if (dotdotdot) {
				start = llLexerItemNext(start);
				struct parserNode *lit CLEANUP(parserNodeDestroy)=literalRecur(start, NULL, &start);
				if(lit->type==NODE_LIT_INT)
						gotInt = 1;
				if(lit->type==NODE_LIT_STR)
						if(((struct parserNodeLitStr*)lit)->str.isChar)
								gotInt = 1;
			
				caseValueUpper = intLitValue(lit);
			
				if(!gotInt)
						whineExpected(start, "integer");
		} else {
				failed = 1;
		}

		colon = expectKeyword(start, ":");
		if (colon) {
			start = llLexerItemNext(start);
		} else {
			failed = 1;
			whineExpected(start, ":");
		}

		long startP, endP;
		getStartEndPos(valueStart, start, &startP, &endP);
		if (parent == NULL)
			whineCaseNoSwitch(kwCase, startP, endP);

		caseValueUpper = (caseValueUpper == -1) ? caseValue  : caseValueUpper;

		ensureCaseDoesntExist(caseValue, caseValueUpper, startP, endP);

		struct parserNodeCase caseNode;
		caseNode.base.type = NODE_CASE;
		caseNode.parent = parent;
		caseNode.valueLower = caseValue;
		caseNode.valueUpper = caseValueUpper;
		caseNode.label=kwCase;
		retVal = ALLOCATE(caseNode);
		if (end)
			assignPosByLexerItems(retVal, originalStart, *end);
		else
			assignPosByLexerItems(retVal, originalStart, NULL);

		if (parent->type == NODE_SWITCH) {
			struct parserNodeSwitch *swit = (void *)parent;
			swit->caseSubcases = strParserNodeAppendItem(swit->caseSubcases, retVal);
		} else if (parent->type == NODE_SUBSWITCH) {
			struct parserNodeSubSwitch *sub = (void *)parent;
			sub->caseSubcases = strParserNodeAppendItem(sub->caseSubcases, retVal);
		}
	} else {
		__auto_type defStart = start;
		kwCase = expectKeyword(start, "default");
		if (kwCase) {
			start = llLexerItemNext(start);

			colon = expectKeyword(start, ":");
			if (colon) {
				start = llLexerItemNext(start);
			} else {
				whineExpected(start, ":");
			}
		} else
			goto end;

		struct parserNodeDefault dftNode;
		dftNode.base.type = NODE_DEFAULT;
		dftNode.parent = parent;
		retVal = ALLOCATE(dftNode);
		if (end)
			assignPosByLexerItems(retVal, originalStart, *end);
		else
			assignPosByLexerItems(retVal, originalStart, NULL);
		;

		long startP, endP;
		getStartEndPos(start, start, &startP, &endP);
		if (parent == NULL) {
			whineCaseNoSwitch(kwCase, startP, endP);
		} else {
			if (parent->type == NODE_SWITCH) {
				struct parserNodeSwitch *swit = (void *)parent;
				swit->dft = retVal;
			} else if (parent->type == NODE_SUBSWITCH) {
				struct parserNodeSubSwitch *sub = (void *)parent;
				sub->dft = retVal;
			}
		}
	}
end:
	if (end)
		*end = start;

	return retVal;
}
//
// Stack of current function info
//
struct currentFunctionInfo {
	struct object *retType;
	long retTypeBegin, retTypeEnd;
	strParserNode insideFunctions;
};
STR_TYPE_DEF(struct currentFunctionInfo, FuncInfoStack);
STR_TYPE_FUNCS(struct currentFunctionInfo, FuncInfoStack);

static __thread strFuncInfoStack currentFuncsStack = NULL;

struct parserNode *parseGoto(llLexerItem start, llLexerItem *end) {
	struct parserNode *gt = expectKeyword(start, "goto");
	struct parserNode *retVal = NULL;
	if (gt) {
		start = llLexerItemNext(start);
		__auto_type nm = nameParse(start, NULL, &start);
		if (!nm) {
			// Whine about expected name
			diagErrorStart(gt->pos.start, gt->pos.end);
			diagPushText("Expected name after goto.");
			diagHighlight(gt->pos.start, gt->pos.end);
			;
			diagEndMsg();
			goto end;
		}

		struct parserNodeGoto node;
		node.base.pos.start = gt->pos.start;
		node.base.pos.end = nm->pos.end;
		node.base.type = NODE_GOTO;
		node.labelName = nm;

		retVal = ALLOCATE(node);
		addLabelRef(retVal, ((struct parserNodeName *)nm)->text);
	}

end:
	if (end)
		*end = start;

	return retVal;
}
struct parserNode *parseReturn(llLexerItem start, llLexerItem *end) {
	__auto_type originalStart = start;
	struct parserNode *ret = expectKeyword(start, "return");
	if (ret) {
		start = llLexerItemNext(start);
		// Ensrue if in function
		if (strFuncInfoStackSize(currentFuncsStack) == 0) {
			diagErrorStart(ret->pos.start, ret->pos.end);
			diagPushText("Return appearing in non-function.");
			diagHighlight(ret->pos.start, ret->pos.end);
			diagEndMsg();

			// Expect expression then semi
			parseExpression(start, NULL, &start);
			__auto_type semi = expectKeyword(start, ";");
			if (!semi)
				whineExpected(start, ";");
			else
				start = llLexerItemNext(start);

			goto end;
		}
		__auto_type top = &currentFuncsStack[strFuncInfoStackSize(currentFuncsStack) - 1];

		struct parserNode *retVal = parseExpression(start, NULL, &start);
		// Check for semicolon
		if (!retVal) {
			start = llLexerItemNext(start);

			// Warn if current return type isn't void
			if (top->retType != &typeU0) {
				diagWarnStart(ret->pos.start, ret->pos.end);
				diagPushText("Empty return on non-U0 function.");
				diagHighlight(ret->pos.start, ret->pos.end);
				diagEndMsg();
			}

			retVal = NULL;
		} else {
			// Ensure if current function inst void
			if (top->retType == &typeU0) {
				diagErrorStart(ret->pos.start, ret->pos.end);
				diagPushText("Attempting to return a value when the return type is U0.");
				diagHighlight(ret->pos.start, ret->pos.end);
				diagEndMsg();

				// Goto end(because fail)
				goto end;
			} else if (!objectIsCompat(top->retType, assignTypeToOp(retVal))) {
				// Whine because types are compable
				// Error
				diagErrorStart(ret->pos.start, ret->pos.end);
				__auto_type res = object2Str(assignTypeToOp(retVal));
				diagPushText("Attempting to return incompatable type '%s'.");
				diagHighlight(retVal->pos.start, retVal->pos.end);
				diagEndMsg();

				// Note
				diagNoteStart(top->retTypeBegin, top->retTypeEnd);
				diagPushText("Return type defined here:");
				diagHighlight(top->retTypeBegin, top->retTypeEnd);
				diagEndMsg();

				// goto end(beacuse of fail)
				goto end;
			}
		}

		struct parserNodeReturn node;
		node.base.type = NODE_RETURN;
		assignPosByLexerItems((struct parserNode *)&node, originalStart, start);
		node.value = retVal;

		if (end)
			*end = start;
		return ALLOCATE(node);
	}

end:
	if (end)
		*end = start;
	return NULL;
}
struct linkage linkageClone(struct linkage from) {
	return (struct linkage){from.type, strClone(from.fromSymbol)};
}
struct parserNode *parseFunction(llLexerItem start, llLexerItem *end) {
	__auto_type originalStart = start;
	struct linkage link = getLinkage(start, &start);
	struct parserNode *name = nameParse(start, NULL, &start);
	if (!name)
		return NULL;

	struct parserNodeName *nm = (void *)name;
	__auto_type baseType = objectByName(nm->text);
	if (!baseType) {
		return NULL;
	}

	long ptrLevel = 0;
	for (;;) {
		__auto_type star = expectOp(start, "*");
		if (!star)
			break;

		ptrLevel++, start = llLexerItemNext(start);
	}

	name = nameParse(start, NULL, &start);
	if (!name)
		return NULL;

	struct parserNode *lP = NULL, *rP = NULL;
	struct parserNode *toKill[] = {lP, rP};
	strParserNode args = NULL;
	long count = sizeof(toKill) / sizeof(*toKill);
	lP = expectOp(start, "(");
	if (!lP)
		goto fail;
	start = llLexerItemNext(start);

	int foundDotDotDot=0;
	for (int firstRun = 1;; firstRun = 0) {
		rP = expectOp(start, ")");
		if (rP) {
			start = llLexerItemNext(start);
			break;
		}
		if (!firstRun) {
			__auto_type comma = expectOp(start, ",");
			if (comma == NULL) {
				whineExpected(start, ",");
				break;
			} else
				start = llLexerItemNext(start);
		}

		struct parserNode *dotdotdot CLEANUP(parserNodeDestroy)=expectKeyword(start, "...");
		if(dotdotdot) {
				foundDotDotDot=1;
				start=llLexerItemNext(start);
				rP = expectOp(start, ")");
				if(!rP)
						whineExpected(start, ")");
				else
						start=llLexerItemNext(start);
				break;
		}
		
		struct parserNode *decl = parseSingleVarDecl(start, &start);
		if (decl)
			args = strParserNodeAppendItem(args, decl);
	}

	struct object *retType = baseType;
	for (long i = 0; i != ptrLevel; i++)
		retType = objectPtrCreate(retType);

	strFuncArg fargs = NULL;
	for (long i = 0; i != strParserNodeSize(args); i++) {
		if (args[i]->type != NODE_VAR_DECL) {
			// TODO whine
			continue;
		}
		struct parserNodeVarDecl *decl = (void *)args[i];
		struct objectFuncArg arg;
		arg.dftVal = decl->dftVal;
		arg.name = decl->name;
		arg.type = decl->type;
		arg.var=NULL;
		
		// TODO whine on metadata

		fargs = strFuncArgAppendItem(fargs, arg);
	}

	struct object *funcType;
	funcType = objectFuncCreate(retType, fargs,foundDotDotDot);

	//
	// Enter the function
	//
	struct currentFunctionInfo info;
	info.retTypeBegin = nm->base.pos.start;
	info.retTypeEnd = name->pos.start;
	info.retType = retType;
	info.insideFunctions = NULL;
	currentFuncsStack = strFuncInfoStackAppendItem(currentFuncsStack, info);
	//!!! each function's regular labels are unique to that function
	__auto_type oldLabels = labels;
	__auto_type oldLabelRefs = labelReferences;
	labels = mapParserNodeCreate();
	labelReferences = mapParserNodesCreate();
	//

	struct parserNode *retVal = NULL;
	__auto_type scope = parseScope(start, &start, (struct objectFunction*)funcType);
	if (!scope) {
		// If no scope follows,is a forward declaration
		__auto_type semi = expectKeyword(start, ";");
		if (!semi)
			whineExpected(start, ";");
		else
			start = llLexerItemNext(start);

		struct parserNodeFuncForwardDec forward;
		forward.base.type = NODE_FUNC_FORWARD_DECL;
		forward.funcType = funcType;
		forward.name = name;

		retVal = ALLOCATE(forward);
	} else {
		// Has a function body
		struct parserNodeFuncDef func;
		func.base.type = NODE_FUNC_DEF;
		func.bodyScope = scope;
		func.funcType = funcType;
		func.name = name;
		func.args = args;

		retVal = ALLOCATE(func);
	}

	// Assign parent function to functions within function
	info = currentFuncsStack[strFuncInfoStackSize(currentFuncsStack) - 1];
	for (long i = 0; i != strParserNodeSize(info.insideFunctions); i++) {
		assert(info.insideFunctions[i]->type == NODE_FUNC_DEF);
		((struct parserNodeFuncDef *)info.insideFunctions[i])->func->parentFunction = retVal;
	}
	strParserNodeDestroy(&info.insideFunctions);
	//
	// Leave the function
	//
	currentFuncsStack = strFuncInfoStackPop(currentFuncsStack, NULL);
	//!!! restore the old labels too
	parserMapGotosToLabels();
	mapParserNodeDestroy(labels, NULL);
	mapParserNodesDestroy(labelReferences, NULL); // TODO
	labels = oldLabels;
	labelReferences = oldLabelRefs;
	//

	// Assign parent to functions inside function (if in a function)
	if (strFuncInfoStackSize(currentFuncsStack)) {
		__auto_type appendTo = &currentFuncsStack[strFuncInfoStackSize(currentFuncsStack) - 1].insideFunctions;
		*appendTo = strParserNodeAppendItem(*appendTo, retVal);
	}

	if (end)
		*end = start;

	if (end)
		assignPosByLexerItems(retVal, originalStart, *end);
	else
		assignPosByLexerItems(retVal, originalStart, NULL);

	parserAddFunc(name, funcType, retVal);

	if (retVal->type == NODE_FUNC_DEF)
		((struct parserNodeFuncDef *)retVal)->func = parserGetFunc(name);
	else if (retVal->type == NODE_FUNC_FORWARD_DECL)
		((struct parserNodeFuncForwardDec *)retVal)->func = parserGetFunc(name);
	return retVal;
fail:
	return NULL;
}
struct parserVar *parserVariableClone(struct parserVar *var) {
	struct parserVar *retVal = ALLOCATE(*var);
	if (var->name)
		retVal->name = strClone(var->name);
	retVal->refs = NULL;
	return retVal;
}
struct parserNode *parseAsmRegister(llLexerItem start, llLexerItem *end) {
	__auto_type item = llLexerItemValuePtr(start);
	if (item->template == &nameTemplate) {
		strRegP regs CLEANUP(strRegPDestroy) = regsForArch();
		for (long i = 0; i != strRegPSize(regs); i++) {
			if (0 == strcasecmp(regs[i]->name, lexerItemValuePtr(item))) {
				if (end)
					*end = llLexerItemNext(start);
				struct parserNodeAsmReg reg;
				reg.base.pos.start = item->start;
				reg.base.pos.end = item->end;
				reg.base.type = NODE_ASM_REG;
				reg.reg = regs[i];
				return ALLOCATE(reg);
			}
		}
	}
	return NULL;
}
static struct X86AddressingMode *addrModeFromParseTree(struct parserNode *node, struct object *valueType, struct X86AddressingMode *providedOffset, int *success);
struct parserNode *parseAsmAddrModeSIB(llLexerItem start, llLexerItem *end) {
	__auto_type originalStart = start;
	struct object *valueType = NULL;
	struct parserNode *colon = NULL, *segment = NULL, *rB = NULL, *lB = NULL, *offset = NULL;
	struct parserNode **toDestroy[] = {
	    &colon, &segment, &lB, &rB, &offset,
	};
	struct parserNode *typename CLEANUP(parserNodeDestroy) = nameParse(start, NULL, &start);
	if (typename) {
		struct parserNodeName *name = (void *)typename;
		if (objectByName(name->text)) {
			valueType = objectByName(name->text);
		}
	}
	segment = parseAsmRegister(start, &start);
	if (segment) {
		colon = expectKeyword(start, ":");
		if (colon)
			start = llLexerItemNext(start);
		if (!start)
			goto fail;
	}

	__auto_type disp = literalRecur(start, llLexerItemNext(start), &start);
	if (disp) {
		offset = disp;
	}
	lB = expectOp(start, "[");
	if (lB == NULL) {
		// If we were onto something(had a colon ) fail
		if (colon)
			whineExpected(start, "[");
	}
	if (!colon && !lB)
		goto fail;
	start = llLexerItemNext(start);
	__auto_type indirExp = parseExpression(start, NULL, &start);
	rB = expectOp(start, "]");
	if (rB) {
		start = llLexerItemNext(start);
	} else {
		if (lB)
			whineExpected(start, "]");
	}
	if (end)
		*end = start;

	struct parserNodeAsmAddrMode indir;
	indir.base.type=NODE_ASM_ADDRMODE;
	struct X86AddressingMode *offsetMode=NULL;
	if(offset) {
			if(offset->type==NODE_LIT_INT) {
					offsetMode=X86AddrModeSint(((struct parserNodeLitInt*)offset)->value.value.sLong);
			} else {
					//Implment me
					abort();
			}
	}
	indir.mode=addrModeFromParseTree(indirExp, valueType, offsetMode, NULL);
	indir.mode->value.m.segment=NULL;
	if(segment)
			indir.mode->value.m.segment=((struct parserNodeAsmReg*)segment)->reg;
	parserNodeDestroy(&indirExp);
	if (lB)
		parserNodeDestroy(&lB);
	if (rB)
		parserNodeDestroy(&rB);
	if (colon)
		parserNodeDestroy(&colon);
	return ALLOCATE(indir);
fail:;
	long count = sizeof(toDestroy) / sizeof(*toDestroy);
	for (long i = 0; i != count; i++)
		parserNodeDestroy(toDestroy[i]);
	return NULL;
}
static void strParserNodeDestroy2(strParserNode *str) {
	for (long s = 0; s != strParserNodeSize(*str); s++)
		parserNodeDestroy(&str[0][s]);
	strParserNodeDestroy(str);
}
void parserNodeDestroy(struct parserNode **node) {
	if (!node[0])
		return;
	switch (node[0]->type) {
	case NODE_TRY: {
			struct parserNodeTry *try=(void*)node[0];
			parserNodeDestroy(&try->body);
			parserNodeDestroy(&try->catch);
			break;
	}
	case NODE_ASM_ADDRMODE: {
			struct parserNodeAsmAddrMode *addrMode=(void*)node[0];
			X86AddrModeDestroy(&addrMode->mode);
			break;
	}
	case NODE_ARRAY_LITERAL: {
			struct parserNodeArrayLit *lit=(void*)node[0];
			strParserNodeDestroy2(&lit->items);
			break;
	}
	case NODE_SIZEOF_TYPE:
		break;
	case NODE_SIZEOF_EXP: {
		struct parserNodeSizeofExp *sz = (void *)node;
		parserNodeDestroy(&sz->exp);
		break;
	}
	case NODE_ASM_REG:
		break;
		case NODE_ASM_LABEL_GLBL: {
				struct parserNodeLabelGlbl *lab=(void*)node[0];
				parserNodeDestroy(&lab->name);
				break;
		}
	case NODE_ASM_LABEL:
	case NODE_ASM_LABEL_LOCAL: {
		struct parserNodeLabel *lab = (void *)node[0];
		parserNodeDestroy(&lab->name);
		break;
	}
	case NODE_ASM_IMPORT: {
		struct parserNodeAsmImport *imp = (void *)node[0];
		//Symbols point to global symbols so dont free them
		strParserNodeDestroy(&imp->symbols);
		break;
	}
	case NODE_ASM_DU8:
	case NODE_ASM_DU16:
	case NODE_ASM_DU32:
	case NODE_ASM_DU64: {
		struct parserNodeDUX *du = (void *)node[0];
		__vecDestroy(&du->bytes);
		break;
	}
	case NODE_ASM_USE16:
	case NODE_ASM_USE32:
	case NODE_ASM_USE64: {
		struct parserNodeUse16 *use = (void *)node[0];
		break;
	}
	case NODE_ASM_ORG: {
		struct parserNodeAsmOrg *org = (void *)node[0];
		break;
	}
	case NODE_ASM_ALIGN: {
		struct parserNodeAsmAlign *align = (void *)node[0];
		break;
	}
	case NODE_ASM_BINFILE: {
		struct parserNodeAsmBinfile *binfile = (void *)node[0];
		parserNodeDestroy(&binfile->fn);
		break;
	}
	case NODE_ASM: {
		struct parserNodeAsm *ASM = (void *)node[0];
		strParserNodeDestroy2(&ASM->body);
		break;
	}
	case NODE_BINOP: {
		struct parserNodeBinop *binop = (void *)node[0];
		parserNodeDestroy(&binop->a);
		parserNodeDestroy(&binop->b);
		parserNodeDestroy(&binop->op);
		break;
	}
	case NODE_LIT_FLT: {
		break;
	}
	case NODE_ASM_INST: {
		struct parserNodeAsmInstX86 *inst = (void *)node[0];
		strParserNodeDestroy2(&inst->args);
		parserNodeDestroy(&inst->name);
		break;
	}
	case NODE_UNOP: {
		struct parserNodeUnop *unop = (void *)node[0];
		parserNodeDestroy(&unop->op);
		parserNodeDestroy(&unop->a);
		break;
	}
	case NODE_NAME: {
		struct parserNodeName *name = (void *)node[0];
		free(name->text);
		break;
	}
	case NODE_OP: {
		struct parserNodeOpTerm *op = (void *)node[0];
		break;
	}
	case NODE_FUNC_CALL: {
		struct parserNodeFuncCall *call = (void *)node[0];
		strParserNodeDestroy2(&call->args);
		parserNodeDestroy(&call->func);
		break;
	}
	case NODE_COMMA_SEQ: {
		struct parserNodeCommaSeq *seq = (void *)node[0];
		strParserNodeDestroy2(&seq->items);
		break;
	}
	case NODE_LIT_INT: {
		struct parserNodeLitInt *i = (void *)node[0];
		break;
	}
	case NODE_LIT_STR: {
		struct parserNodeLitStr *str = (void *)node[0];
		__vecDestroy(&str->str.text);
		break;
	}
	case NODE_KW: {
		struct parserNodeKeyword *kw = (void *)node[0];
		break;
	}
	case NODE_VAR_DECL: {
		struct parserNodeVarDecl *decl = (void *)node[0];
		parserNodeDestroy(&decl->dftVal);
		strParserNodeDestroy2(&decl->metaData);
		parserNodeDestroy(&decl->name);
		break;
	}
	case NODE_VAR_DECLS: {
		struct parserNodeVarDecls *decls = (void *)node[0];
		strParserNodeDestroy2(&decls->decls);
		break;
	}
	case NODE_META_DATA: {
		struct parserNodeMetaData *meta = (void *)node[0];
		parserNodeDestroy(&meta->name);
		parserNodeDestroy(&meta->value);
		break;
	}
	case NODE_CLASS_DEF: {
		struct parserNodeClassDef *def = (void *)node[0];
		parserNodeDestroy(&def->name);
		break;
	}
	case NODE_CLASS_FORWARD_DECL: {
		struct parserNodeClassFwd *fwd = (void *)node[0];
		parserNodeDestroy(&fwd->name);
		break;
	}
	case NODE_UNION_DEF: {
		struct parserNodeUnionDef *def = (void *)node[0];
		parserNodeDestroy(&def->name);
		break;
	}
	case NODE_UNION_FORWARD_DECL: {
		struct parserNodeUnionFwd *fwd = (void *)node[0];
		parserNodeDestroy(&fwd->name);
		break;
	}
	case NODE_IF: {
		struct parserNodeIf *If = (void *)node[0];
		parserNodeDestroy(&If->cond);
		parserNodeDestroy(&If->body);
		parserNodeDestroy(&If->el);
		break;
	}
	case NODE_SCOPE: {
		struct parserNodeScope *scope = (void *)node[0];
		strParserNodeDestroy2(&scope->stmts);
		break;
	}
	case NODE_BREAK: {
		struct parserNodeBreak *brk = (void *)node[0];
		break;
	}
	case NODE_WHILE: {
		struct parserNodeWhile *whl = (void *)node[0];
		parserNodeDestroy(&whl->cond);
		parserNodeDestroy(&whl->body);
		break;
	}
	case NODE_DO: {
		struct parserNodeDo *Do = (void *)node[0];
		parserNodeDestroy(&Do->cond);
		parserNodeDestroy(&Do->body);
		break;
	}
	case NODE_FOR: {
		struct parserNodeFor *For = (void *)node[0];
		parserNodeDestroy(&For->body);
		parserNodeDestroy(&For->cond);
		parserNodeDestroy(&For->inc);
		parserNodeDestroy(&For->init);
		break;
	}
	case NODE_VAR: {
		struct parserNodeVar *var = (void *)node[0];
		break;
	}
	case NODE_CASE: {
		struct parserNodeCase *cs = (void *)node[0];
		parserNodeDestroy(&cs->label);
		break;
	}
	case NODE_DEFAULT: {
		struct parserNodeDefault *dft = (void *)node[0];
		break;
	}
	case NODE_SWITCH: {
		struct parserNodeSwitch *swit = (void *)node[0];
		parserNodeDestroy(&swit->body);
		parserNodeDestroy(&swit->exp);
		strParserNodeDestroy(&swit->caseSubcases);
		break;
	}
	case NODE_SUBSWITCH: {
		struct parserNodeSubSwitch *sub = (void *)node[0];
		strParserNodeDestroy(&sub->caseSubcases);
		strParserNodeDestroy2(&sub->body);
		parserNodeDestroy(&sub->start);
		break;
	}
	case NODE_LABEL: {
		struct parserNodeLabel *lab = (void *)node[0];
		parserNodeDestroy(&lab->name);
		break;
	}
	case NODE_TYPE_CAST: {
		struct parserNodeTypeCast *cast = (void *)node[0];
		parserNodeDestroy(&cast->exp);
		break;
	}
	case NODE_ARRAY_ACCESS: {
		struct parserNodeArrayAccess *arrAcc = (void *)node[0];
		parserNodeDestroy(&arrAcc->index);
		parserNodeDestroy(&arrAcc->exp);
		break;
	}
	case NODE_FUNC_DEF: {
		struct parserNodeFuncDef *def = (void *)node[0];
		parserNodeDestroy(&def->name);
		parserNodeDestroy(&def->bodyScope);
		break;
	}
	case NODE_FUNC_FORWARD_DECL: {
		struct parserNodeFuncForwardDec *fwd = (void *)node[0];
		parserNodeDestroy(&fwd->name);
		break;
	}
	case NODE_FUNC_REF: {
		struct parserNodeFuncRef *ref = (void *)node[0];
		parserNodeDestroy(&ref->name);
		break;
	}
	case NODE_MEMBER_ACCESS: {
		struct parserNodeMemberAccess *mem = (void *)node[0];
		parserNodeDestroy(&mem->exp);
		parserNodeDestroy(&mem->name);
		break;
	}
	case NODE_RETURN: {
		struct parserNodeReturn *ret = (void *)node[0];
		parserNodeDestroy(&ret->value);
		break;
	}
	case NODE_GOTO: {
		struct parserNodeGoto *gt = (void *)node[0];
		parserNodeDestroy(&gt->labelName);
		break;
	}
	case NODE_LINKAGE: {
		struct parserNodeLinkage *link = (void *)node[0];
		break;
	}
	}
	free(node[0]);
}
static int isExpectedBinop(struct parserNode *node, const char *op) {
	if (node->type != NODE_BINOP)
		return 0;
	struct parserNodeBinop *binop = (void *)node;
	struct parserNodeOpTerm *opTerm = (void *)binop->op;
	return 0 == strcmp(opTerm->text, op);
}
static uint64_t uintLitValue(struct parserNode *lit) {
	if (lit->type == NODE_LIT_INT) {
		__auto_type node = (struct parserNodeLitInt *)lit;
		uint64_t retVal;
		if (node->value.type == INT_SLONG)
			retVal = node->value.value.sLong;
		else if (node->value.type == INT_ULONG)
			retVal = node->value.value.uLong;
		return retVal;
	} else if (lit->type == NODE_LIT_STR) {
		struct parserNodeLitStr *str = (void *)lit;
		uint64_t retVal = 0;
		for (long i = 0; i != __vecSize(str->str.text); i++) {
			uint64_t c = ((unsigned char *)str->str.text)[i];
			retVal |= c << 8 * i;
		};
		return retVal;
	}
	return -1;
}
static struct X86AddressingMode *sibOffset2Addrmode(struct parserNode *node) {
	if (node->type == NODE_NAME) {
		struct parserNodeName *name = (void *)node;
		__auto_type find = mapParserNodeGet(asmImports, name->text);
		if (find) {
			if (find[0]->type == NODE_VAR)
				((struct parserNodeVar *)*find)->var->isNoreg = 1;
			return X86AddrModeItemAddrOf(*find, 0,NULL);
		}
		addLabelRef((struct parserNode *)node, name->text);
		return X86AddrModeLabel(name->text);
	} else if (node->type == NODE_LIT_INT) {
		return X86AddrModeSint(intLitValue(node));
	}
	return NULL;
}
static struct X86AddressingMode *addrModeFromParseTree(struct parserNode *node, struct object *valueType, struct X86AddressingMode *providedOffset, int *success) {
	int64_t scale = 0;
	int64_t scaleDefined = 0, offsetDefined = providedOffset != NULL;
	struct X86AddressingMode *offset = NULL;
	if (providedOffset)
		offset = providedOffset;
	struct reg *index = NULL, *base = NULL;
	strParserNode toProcess CLEANUP(strParserNodeDestroy) = NULL;
	strParserNode stack CLEANUP(strParserNodeDestroy) = strParserNodeAppendItem(NULL, node);
	// Points to operand of current binop/unop on stack
	strInt stackArg CLEANUP(strIntDestroy) = strIntAppendItem(NULL, 0);

	while (1) {
		if (!strParserNodeSize(stack))
			break;
	pop:;
		int argi;
		stackArg = strIntPop(stackArg, &argi);
		struct parserNode *top;
		stack = strParserNodePop(stack, &top);
		// If binop,re-insert on stack if passed first argument,but inc argi
		if (top->type == NODE_BINOP && argi < 2) {
			stack = strParserNodeAppendItem(stack, top);
			stackArg = strIntAppendItem(stackArg, argi + 1);
		}
		if (top->type == NODE_BINOP && argi < 2) {
			struct parserNodeBinop *binop = (void *)top;
			struct parserNodeOpTerm *op = (void *)binop->op;
			struct parserNode *arg = (argi) ? binop->b : binop->a;
			stack = strParserNodeAppendItem(stack, arg);
			stackArg = strIntAppendItem(stackArg, 0);
			// Should expect "+" or "*"
			if (!isExpectedBinop(top, "+") && !isExpectedBinop(top, "*"))
				goto fail;
		} else if (top->type == NODE_UNOP) {
			// Should expect "+" or "*"
			goto fail;
		} else if (top->type == NODE_LIT_INT) {
			// Can be index or scale
			// Is only scale if "*" is on top
			if (strParserNodeSize(stack)) {
				if (isExpectedBinop(stack[strParserNodeSize(stack) - 1], "*")) {
					if (scaleDefined)
						goto fail;
					scaleDefined = 1;
					scale = uintLitValue(top);
					continue;
				}
			}
			// Is an offset
			if (offsetDefined)
				goto fail;
			offsetDefined = 1;
			offset = X86AddrModeSint(intLitValue(top));
		} else if (top->type == NODE_ASM_REG) {
			// Is only scale if "*" is on top
			if (strParserNodeSize(stack)) {
				if (isExpectedBinop(stack[strParserNodeSize(stack) - 1], "*")) {
					if (index)
						goto fail;
					index = ((struct parserNodeAsmReg *)top)->reg;
					continue;
				}
			}
			// Is a base
			if (base)
				goto fail;
			base = ((struct parserNodeAsmReg *)top)->reg;
		} else if (top->type == NODE_NAME) {
			// Offset can be a name,
			if (strParserNodeSize(stack))
				if (isExpectedBinop(stack[strParserNodeSize(stack) - 1], "*"))
					goto fail;
			offset = sibOffset2Addrmode(top);
		} else if (top->type == NODE_VAR) {
			// Ensure item is only thing in expression
			if (strParserNodeSize(stack) != 0)
				goto fail;
			if (base || index || offset)
				goto fail;
			if (success)
					*success = 1;
			struct X86AddressingMode retVal;
			retVal.type = X86ADDRMODE_ITEM_ADDR;
			retVal.valueType = valueType;
			retVal.value.itemAddr.item = top;
			retVal.value.itemAddr.offset=0;
			return ALLOCATE(retVal);
		} else
			goto fail;
	}
	if (success)
		*success = 1;
	struct X86AddressingMode retVal;
	retVal.type = X86ADDRMODE_MEM;
	retVal.value.m.type = x86ADDR_INDIR_SIB;
	if(base)
			retVal.value.m.value.sib.base = X86AddrModeReg(base);
	else
			retVal.value.m.value.sib.base=NULL;
	if(index)
			retVal.value.m.value.sib.index = X86AddrModeReg(index);
	else
			retVal.value.m.value.sib.index=NULL;
	retVal.value.m.value.sib.offset = offset;
	retVal.value.m.value.sib.offset2=0;
	retVal.value.m.value.sib.scale = scale;
	retVal.value.m.segment=NULL;
	retVal.valueType = valueType;
	return ALLOCATE(retVal);
fail:
	if (success)
		*success = 0;
	return X86AddrModeSint(-1);
}
struct X86AddressingMode *parserNode2X86AddrMode(struct parserNode *node) {
		if(node->type==NODE_LIT_STR) {
				struct parserNodeLitStr *str=(void*)node;
				return X86EmitAsmStrLit((char*)str->str.text, __vecSize(str->str.text));
		} else
				if(node->type==NODE_ASM_ADDRMODE) {
				struct parserNodeAsmAddrMode *addrMode=(void*)node;
				return X86AddrModeClone(addrMode->mode);
		} else if(node->type==NODE_LIT_INT) {
				struct parserNodeLitInt *i=(void*)node;
				if(i->value.type==INT_SLONG) {
						return X86AddrModeSint(i->value.value.sLong);
				} else
			 			return X86AddrModeUint(i->value.value.uLong);
		} else if(node->type==NODE_ASM_REG) {
				struct parserNodeAsmReg *r=(void*)node;
				return X86AddrModeReg(r->reg);
		}
		abort();
}
static struct X86AddressingMode *x86ItemAddr(llLexerItem start, llLexerItem *end) {
		struct parserNode *rB CLEANUP(parserNodeDestroy)=NULL;
		struct parserNode *lB CLEANUP(parserNodeDestroy)=NULL;
		struct parserNode *segment CLEANUP(parserNodeDestroy) = parseAsmRegister(start, &start);;
		struct parserNode *typename CLEANUP(parserNodeDestroy) = nameParse(start, NULL, &start);
		struct object *valueType=NULL;
		if (typename) {
				struct parserNodeName *name = (void *)typename;
				if (objectByName(name->text)) {
						valueType = objectByName(name->text);
				}
		}

		rB=expectOp(start, "[");
		if(!rB)
				return NULL;
		start=llLexerItemNext(start);

		struct parserNode *expr CLEANUP(parserNodeDestroy)= parseExpression(start, NULL,&start);
		long offset=0;
		struct parserNode *currExprPart=expr;
		struct X86AddressingMode *retVal=NULL;
	loop:
		if(currExprPart->type==NODE_MEMBER_ACCESS) {
				struct parserNodeMemberAccess *access=(void*)currExprPart;
				struct parserNodeOpTerm *t=(void*)access->op;
				if(0!=strcmp(t->text,"."))
						goto fail;

				__auto_type aType=assignTypeToOp(access->exp);
				struct parserNodeName *nm=(void*)access->name;
				struct objectMember *member = objectMemberGet(aType,nm);
				if(!member) {
						diagErrorStart(nm->base.pos.start, nm->base.pos.end);
						char *str=object2Str(aType);
						diagPushText("Type ");
						diagPushText(str);
						diagPushText(" doesn't have member");
						diagPushQoutedText(nm->base.pos.start, nm->base.pos.end);
						diagPushText(".");
						free(str);
						diagEndMsg();
						goto fail;
				} else {
						offset+=member->offset;
				}
				
				currExprPart=access->exp;
				goto loop;
		} else if(currExprPart->type==NODE_VAR) {
				((struct parserNodeVar *)currExprPart)->var->isNoreg = 1;
				__auto_type clone=*(struct parserNodeVar *)currExprPart;
				retVal=X86AddrModeItemAddrOf(ALLOCATE(clone), offset , (valueType)?valueType:assignTypeToOp(expr));
				
				retVal->value.m.segment=NULL;
				if(segment)
						retVal->value.m.segment=((struct parserNodeAsmReg*)segment)->reg;
		} else 
				goto fail;

		lB=expectOp(start, "]");
		if(!lB)
				whineExpected(start, "]");
		else
				start=llLexerItemNext(start);

		if(end)
				*end=start;
		return retVal;
	fail:
		return NULL;
}
struct parserNode *parseAsmInstructionX86(llLexerItem start, llLexerItem *end) {
	if (llLexerItemValuePtr(start)->template == &nameTemplate) {
		__auto_type originalStart = start;
		struct parserNode *name CLEANUP(parserNodeDestroy) = nameParse(start, NULL, &start);
		__auto_type nameText = ((struct parserNodeName *)name)->text;
		strOpcodeTemplate dummy CLEANUP(strOpcodeTemplateDestroy) = X86OpcodesByName(nameText);
		if (strOpcodeTemplateSize(dummy)) {
			long argc = X86OpcodesArgCount(nameText);
			strX86AddrMode args CLEANUP(strX86AddrModeDestroy) = NULL;
			for (long a = 0; a != argc; a++) {
				if (a != 0) {
					struct parserNode *comma CLEANUP(parserNodeDestroy) = expectOp(start, ",");
					if (!comma) {
						whineExpected(start, ",");
					} else
						start = llLexerItemNext(start);
				}
				// Check for address of symbol
				__auto_type addrOf=x86ItemAddr(start, &start);
				if (addrOf) {
						args = strX86AddrModeAppendItem(args,addrOf);
						continue;
				}
				// Try parsing memory address first,then register,then number
				struct parserNode *addrMode = parseAsmAddrModeSIB(start, &start);
				if (addrMode) {
					args = strX86AddrModeAppendItem(args, parserNode2X86AddrMode(addrMode));
					continue;
				}
				struct parserNode *reg = parseAsmRegister(start, &start);
				if (reg) {
					args = strX86AddrModeAppendItem(args, parserNode2X86AddrMode(reg));
					continue;
				}
				struct parserNode *label = nameParse(start, NULL, NULL);
				if (label) {
					struct parserNodeName *name = (void *)label;
						args = strX86AddrModeAppendItem(args, X86AddrModeLabel(name->text));
						addLabelRef(label, name->text);
						start = llLexerItemNext(start);
					continue;
				invalidArg1:;
				}
				struct parserNode *literal = literalRecur(start, NULL, &start);
				if (literal) {
					args = strX86AddrModeAppendItem(args, parserNode2X86AddrMode(literal));
					continue;
				}
			invalidArg:
				// Couldn't find argument so quit
				diagErrorStart(llLexerItemValuePtr(start)->start, llLexerItemValuePtr(start)->end);
				diagPushText("Invalid argument for opcode.");
				diagEndMsg();
				break;
			}
			int ambiguous;
			strOpcodeTemplate valids CLEANUP(strOpcodeTemplateDestroy) = X86OpcodesByArgs(nameText, args, &ambiguous);
			if (end)
				*end = start;
			if (ambiguous) {
				diagErrorStart(name->pos.start, name->pos.end);
				diagPushText("Ambiuous operands for opcode ");
				diagPushQoutedText(name->pos.start, name->pos.end);
				diagPushText(".");
				diagEndMsg();
			} else if (!strOpcodeTemplateSize(valids)) {
				diagErrorStart(name->pos.start, name->pos.end);
				diagPushText("Invalid arguments for opcode ");
				diagPushQoutedText(name->pos.start, name->pos.end);
				diagPushText(".");
				diagEndMsg();
			} else {
				struct parserNodeAsmInstX86 inst;
				strParserNode addrModeArgs=strParserNodeResize(NULL, strX86AddrModeSize(args));
				for(long a=0;a!=strX86AddrModeSize(args);a++) {
						struct parserNodeAsmAddrMode mode;
						mode.base.pos.start=0;
						mode.base.pos.end=0;
						mode.base.type=NODE_ASM_ADDRMODE;
						mode.mode=args[a];
						addrModeArgs[a]=ALLOCATE(mode);
				}
				inst.args = addrModeArgs;
				inst.name = nameParse(originalStart, NULL, NULL);
				inst.base.type = NODE_ASM_INST;
				getStartEndPos(originalStart, start, &inst.base.pos.start, &inst.base.pos.end);
				return ALLOCATE(inst);
			}
		}
	}
	if (end)
		*end = start;
	return NULL;
}

void parserMapGotosToLabels() {
	__parserMapLabels2Refs(1);
}
struct parserNode *parseAsm(llLexerItem start, llLexerItem *end) {
	__auto_type originalStart = start;
	struct parserNode *asmKw CLEANUP(parserNodeDestroy) = expectKeyword(start, "asm");
	if (!asmKw)
		return NULL;
	start = llLexerItemNext(start);
	struct parserNode *lB CLEANUP(parserNodeDestroy) = expectKeyword(start, "{");
	if (!lB)
		whineExpected(start, "{");
	else
		start = llLexerItemNext(start);
	isAsmMode = 1;
	strParserNode body = NULL;
	mapParserNodeDestroy(asmImports, NULL);
	asmImports = mapParserNodeCreate();
	for (; start;) {
		struct parserNode *rB CLEANUP(parserNodeDestroy) = expectKeyword(start, "}");
		if (rB)
			break;
		struct parserNode *lab = parseLabel(start, &start);
		if (lab) {
			body = strParserNodeAppendItem(body, lab);
			continue;
		}
		struct parserNode *inst = parseAsmInstructionX86(start, &start);
		if (inst) {
			body = strParserNodeAppendItem(body, inst);
			continue;
		}
		// define
		long duSize = -1;
		struct parserNode *du8 CLEANUP(parserNodeDestroy) = expectKeyword(start, "DU8");
		if (du8)
			duSize = 1;
		struct parserNode *du16 CLEANUP(parserNodeDestroy) = expectKeyword(start, "DU16");
		if (du16)
			duSize = 2;
		struct parserNode *du32 CLEANUP(parserNodeDestroy) = expectKeyword(start, "DU32");
		if (du32)
			duSize = 4;
		struct parserNode *du64 CLEANUP(parserNodeDestroy) = expectKeyword(start, "DU64");
		if (du64)
			duSize = 8;
		if (duSize != -1) {
			__auto_type originalStart = start;
			struct parserNodeDUX define;
			define.bytes = NULL;
			start = llLexerItemNext(start);
			for (int firstRun = 1;; firstRun = 0) {
				struct parserNode *semi CLEANUP(parserNodeDestroy) = expectKeyword(start, ";");
				if (semi) {
					start = llLexerItemNext(start);
					break;
				}
				if (!firstRun) {
					struct parserNode *comma CLEANUP(parserNodeDestroy) = expectOp(start, ",");
					if (comma)
						start = llLexerItemNext(start);
					else
						whineExpected(start, ",");
				}
				__auto_type originalStart = start;
				struct parserNode *number CLEANUP(parserNodeDestroy) = literalRecur(start, NULL, &start);
				if (number) {
					// Ensure is an int
					if (number->type != NODE_LIT_INT) {
						whineExpected(originalStart, "integer");
						continue;
					}
					uint64_t mask = 0;
					for (long i = 0; i != duSize; i++) {
						mask <<= 8;
						mask |= 0xff;
					}
					struct parserNodeLitInt *lit = (void *)number;
					uint64_t value = mask & lit->value.value.uLong;
					if (archEndian() == ENDIAN_LITTLE) {
					} else if (archEndian() == ENDIAN_BIG) {
						value = __builtin_bswap64(value);
					}
					for (long b = 0; b != duSize; b++) {
						uint8_t byte = value & 0xff;
						value >>= 8;
						define.bytes = __vecAppendItem(define.bytes, &byte, 1);
					}
				} else {
					whineExpected(start, ";");
					break;
				}
			}
			getStartEndPos(originalStart, start, &define.base.pos.start, &define.base.pos.end);
			switch (duSize) {
			case 1:
				define.base.type = NODE_ASM_DU8;
				break;
			case 2:
				define.base.type = NODE_ASM_DU16;
				break;
			case 4:
				define.base.type = NODE_ASM_DU32;
				break;
			case 8:
				define.base.type = NODE_ASM_DU64;
				break;
			}
			__auto_type alloced = ALLOCATE(define);
			body = strParserNodeAppendItem(body, alloced);
			continue;
		}
		struct parserNode *use16 CLEANUP(parserNodeDestroy) = expectKeyword(start, "USE16");
		struct parserNode *use32 CLEANUP(parserNodeDestroy) = expectKeyword(start, "USE32");
		struct parserNode *use64 CLEANUP(parserNodeDestroy) = expectKeyword(start, "USE64");
		if (use16 || use32 || use64) {
			__auto_type originalStart = start;
			struct parserNodeAsmUseXX use;
			getStartEndPos(originalStart, start, &use.base.pos.start, &use.base.pos.end);
			start = llLexerItemNext(start);
			if (use16)
				use.base.type = NODE_ASM_USE16;
			else if (use32)
				use.base.type = NODE_ASM_USE32;
			else if (use64)
				use.base.type = NODE_ASM_USE64;
			body = strParserNodeAppendItem(body, ALLOCATE(use));
			continue;
		}
		struct parserNode *org CLEANUP(parserNodeDestroy) = expectKeyword(start, "ORG");
		if (org) {
			__auto_type originalStart = start;
			start = llLexerItemNext(start);
			struct parserNode *where CLEANUP(parserNodeDestroy) = literalRecur(start, NULL, &start);
			if (!where) {
				whineExpected(start, "int");
				continue;
			}
			if (where->type != NODE_LIT_INT) {
				whineExpected(start, "int");
				continue;
			}
			struct parserNodeAsmOrg org;
			org.base.type = NODE_ASM_ORG;
			getStartEndPos(originalStart, start, &org.base.pos.start, &org.base.pos.end);
			start = llLexerItemNext(start);
			org.org = uintLitValue(where);
			body = strParserNodeAppendItem(body, ALLOCATE(org));
			continue;
		}
		struct parserNode *binfile CLEANUP(parserNodeDestroy) = expectKeyword(start, "BINFILE");
		if (binfile) {
			__auto_type originalStart = start;
			start = llLexerItemNext(start);
			struct parserNode *fn = literalRecur(start, NULL, &start);
			if (!fn) {
				whineExpected(start, "filename");
				continue;
			}
			if (fn->type != NODE_LIT_STR) {
				parserNodeDestroy(&fn);
				whineExpected(start, "filename");
				continue;
			}
			struct parserNodeAsmBinfile binfile;
			binfile.base.type = NODE_ASM_BINFILE;
			binfile.fn = fn;
			getStartEndPos(originalStart, start, &binfile.base.pos.start, &binfile.base.pos.end);
			body = strParserNodeAppendItem(body, ALLOCATE(binfile));
			// Ensure file exists
			struct parserNodeLitStr *str = (void *)fn;
			FILE *file = fopen((char*)str->str.text, "rb");
			fclose(file);
			if (!file) {
				diagErrorStart(fn->pos.start, fn->pos.end);
				diagPushText("File ");
				diagPushQoutedText(fn->pos.start, fn->pos.end);
				diagPushText(" not found.");
				diagEndMsg();
			}
			continue;
		}
		struct parserNode *list CLEANUP(parserNodeDestroy) = expectKeyword(start, "LIST");
		struct parserNode *nolist CLEANUP(parserNodeDestroy) = expectKeyword(start, "NOLIST");
		if (list || nolist)
			continue;
		struct parserNode *import CLEANUP(parserNodeDestroy) = expectKeyword(start, "IMPORT");
		if (import) {
			__auto_type originalStart = start;
			start = llLexerItemNext(start);
			strParserNode symbols = NULL;
			for (int firstRun = 1; start; firstRun = 0) {
				struct parserNode *semi CLEANUP(parserNodeDestroy) = expectKeyword(start, ";");
				if (semi)
					break;
				if (!firstRun) {
					struct parserNode *comma CLEANUP(parserNodeDestroy) = expectOp(start, ",");
					if (!comma)
						whineExpected(start, ",");
					else
						start = llLexerItemNext(start);
				}
				struct parserNode *name CLEANUP(parserNodeDestroy) = nameParse(start, NULL, &start);
				if (!name) {
					start = llLexerItemNext(start);
					whineExpected(start, "symbol");
					continue;
				}
				struct parserNodeName *name2 = (void *)name;
				__auto_type find = parserGetGlobalSym(name2->text);
			importFindLoop:
				if (!find) {
					__auto_type findVar = parserGetVar((const struct parserNode *)name2);
					if (!findVar) {
						diagErrorStart(name->pos.start, name->pos.end);
						diagPushText("Global symbol ");
						diagPushQoutedText(name->pos.start, name->pos.end);
						diagPushText(" wasn't found.");
						diagEndMsg();
					} else {
						struct parserNodeVar var;
						getStartEndPos(start, start, &var.base.pos.start, &var.base.pos.end);
						var.base.type = NODE_VAR;
						var.var = findVar;
						find = ALLOCATE(var);
						var.var->isNoreg = 1;
						goto importFindLoop;
					}
				} else
					mapParserNodeInsert(asmImports, name2->text, find);
				// Mark as noreg if imported
				if (find->type == NODE_VAR) {
					struct parserNodeVar *var = (void *)find;
					var->var->isNoreg = 1;
				}
				symbols = strParserNodeAppendItem(symbols, find);
			}
			struct parserNodeAsmImport import;
			import.base.type = NODE_ASM_IMPORT;
			import.symbols = symbols;
			getStartEndPos(originalStart, start, &import.base.pos.start, &import.base.pos.end);
			start = llLexerItemNext(start);
			body = strParserNodeAppendItem(body, ALLOCATE(import));
			continue;
		}
		struct parserNode *align CLEANUP(parserNodeDestroy) = expectKeyword(start, "ALIGN");
		if (align) {
			__auto_type originalStart = start;
			start = llLexerItemNext(start);
			__auto_type numPos = start;
			struct parserNode *num CLEANUP(parserNodeDestroy) = literalRecur(start, NULL, &start);
			if (!num) {
				whineExpected(start, "int");
				continue;
			}
			struct parserNode *comma CLEANUP(parserNodeDestroy) = expectOp(start, ",");
			if (!comma)
				whineExpected(start, ",");
			else
				start = llLexerItemNext(start);
			__auto_type fillPos = start;
			struct parserNode *fill CLEANUP(parserNodeDestroy) = literalRecur(start, NULL, &start);
			if (!fill) {
				whineExpected(fillPos, "int");
				continue;
			}
			if (num->type != NODE_LIT_INT) {
				whineExpected(numPos, "int");
				continue;
			}
			if (fill->type != NODE_LIT_INT) {
				whineExpected(fillPos, "int");
				continue;
			}
			struct parserNodeAsmAlign align;
			align.base.type = NODE_ASM_ALIGN;
			align.count = uintLitValue(num);
			align.fill = intLitValue(fill);
			getStartEndPos(originalStart, start, &align.base.pos.start, &align.base.pos.end);
			body = strParserNodeAppendItem(body, ALLOCATE(align));
			continue;
		}
		whineExpectedExpr(start);
		start = llLexerItemNext(start);
	}
	isAsmMode = 0;
	struct parserNodeAsm asmBlock;
	asmBlock.body = body;
	asmBlock.base.type = NODE_ASM;
	getStartEndPos(originalStart, start, &asmBlock.base.pos.start, &asmBlock.base.pos.end);
	// Move past "}"
	start = llLexerItemNext(start);
	if (end)
		*end = start;
	return ALLOCATE(asmBlock);
}
struct parserNode *parseArrayLiteral(llLexerItem start, llLexerItem *end) {
		__auto_type originalStart=start;
		struct parserNode *lC CLEANUP(parserNodeDestroy)=expectKeyword(start, "{");
		if(!lC)
				return NULL;
		start=llLexerItemNext(start);
		strParserNode items=NULL;
		for(int firstRun=1;;firstRun=0) {
				struct parserNode *rC CLEANUP(parserNodeDestroy)=expectKeyword(start, "}");
				int foundComma=0;
				if(!firstRun) {
						struct parserNode *comma CLEANUP(parserNodeDestroy)=expectOp(start, ",");
						if(comma) {
								start=llLexerItemNext(start);
								foundComma=1;
						}
				}
				if(rC) {
						start=llLexerItemNext(start);
						break;
				}
				if(!firstRun&&!foundComma)
						whineExpected(start, ",");
				
				__auto_type exp=parseExpression(start, findEndOfExpression(start, 1), &start);
				if(exp) {
						items=strParserNodeAppendItem(items, exp);
				} else {
						whineExpectedExpr(start);
						break;
				}
		}
		struct parserNodeArrayLit lit;
		lit.base.type=NODE_ARRAY_LITERAL;
		lit.items=items;
		__auto_type retVal=ALLOCATE(lit);
		assignPosByLexerItems(retVal, originalStart, start);
		if(end)
				*end=start;
		return retVal;
}
struct parserNode *parseTry(llLexerItem start, llLexerItem *end) {
		__auto_type originalStart=start;
		struct parserNode *try CLEANUP(parserNodeDestroy)=expectKeyword(start, "try");
		if(!try)
				return NULL;
		start=llLexerItemNext(start);

		struct parserNode *body=parseStatement(start, &start);
		if(!body)
				whineExpected(start, "body");
		struct parserNode *catch CLEANUP(parserNodeDestroy)=expectKeyword(start, "catch");

		if(!catch)
				whineExpected(start, "catch");
		start=llLexerItemNext(start);

		struct parserNode *catchBlock=parseStatement(start, &start);
		if(!catchBlock)
				whineExpected(start, "body");
		if(end)
				*end=start;
		
		struct parserNodeTry node;
		assignPosByLexerItems((void*)&node, originalStart, start);
		node.base.type=NODE_TRY;
		node.body=body;
		node.catch=catchBlock;
		return ALLOCATE(node);
}
