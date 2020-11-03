#include <assert.h>
#include <lexer.h>
#include <holyCParser.h>
#include <holyCType.h>
#define DEBUG_PRINT_ENABLE 1
#include <debugPrint.h>
static char *strCopy(const char *text) {
	char *retVal = malloc(strlen(text) + 1);
	strcpy(retVal, text);

	return retVal;
}
#define ALLOCATE(x)                                                            \
	({                                                                           \
		__auto_type len = sizeof(x);                                               \
		void *$retVal = malloc(len);                                               \
		memcpy($retVal, &x, len);                                                  \
		$retVal;                                                                   \
	})
static char *strClone(const char *str) {
	__auto_type len = strlen(str);
	char *retVal = malloc(len + 1);
	strcpy(retVal, str);
	return retVal;
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
			term.pos.start = item->start;
			term.pos.end = item->end;
			term.text = text;

			return ALLOCATE(term);
		}
	}
	return NULL;
}
void parserNodeDestroy(struct parserNode **node) {
	// TODO implement
}
static struct parserNode *nameParse(llLexerItem start, llLexerItem end,
                                    llLexerItem *result);
static void strParserNodeDestroy2(strParserNode *nodes) {
	for (long i = 0; i != strParserNodeSize(*nodes); i++)
		parserNodeDestroy(&nodes[0][i]);
	strParserNodeDestroy(nodes);
}
static struct parserNode *parenRecur(llLexerItem start, llLexerItem end,
                                     llLexerItem *result);
static struct parserNode *literalRecur(llLexerItem start, llLexerItem end,
                                       llLexerItem *result) {
	if (result != NULL)
		*result = start;

	__auto_type item = llLexerItemValuePtr(start);
	if (item->template == &intTemplate) {
		if (result != NULL)
			*result = llLexerItemNext(start);

		struct parserNodeLitInt lit;
		lit.base.type = NODE_LIT_INT;
		lit.value = *(struct lexerInt *)lexerItemValuePtr(item);

		return ALLOCATE(lit);
	} else if (item->template == &strTemplate) {
		if (result != NULL)
			*result = llLexerItemNext(start);

		__auto_type str = *(struct parsedString *)lexerItemValuePtr(item);
		struct parserNodeLitStr lit;
		lit.base.type = NODE_LIT_STR;
		lit.text = strClone((char *)str.text);
		lit.isChar = str.isChar;

		return ALLOCATE(lit);
	} else if (item->template == &nameTemplate) {
		return nameParse(start, end, result);
	} else {
		DEBUG_PRINT("op:%s\n", *(const char **)lexerItemValuePtr(item))
		assert(0);
	}

	// TODO add float template.
}
static struct parserNode *prec0Binop(llLexerItem start, llLexerItem end,
                                     llLexerItem *result);
static struct parserNode *prec1Recur(llLexerItem start, llLexerItem end,
                                     llLexerItem *result);
static struct parserNode *prec2Recur(llLexerItem start, llLexerItem end,
                                     llLexerItem *result);
static struct parserNode *prec3Recur(llLexerItem start, llLexerItem end,
                                     llLexerItem *result);
static struct parserNode *prec4Recur(llLexerItem start, llLexerItem end,
                                     llLexerItem *result);
static struct parserNode *prec5Recur(llLexerItem start, llLexerItem end,
                                     llLexerItem *result);
static struct parserNode *prec6Recur(llLexerItem start, llLexerItem end,
                                     llLexerItem *result);
static struct parserNode *prec7Recur(llLexerItem start, llLexerItem end,
                                     llLexerItem *result);
static struct parserNode *prec8Recur(llLexerItem start, llLexerItem end,
                                     llLexerItem *result);
static struct parserNode *prec9Recur(llLexerItem start, llLexerItem end,
                                     llLexerItem *result);
static struct parserNode *prec10Recur(llLexerItem start, llLexerItem end,
                                      llLexerItem *result);
static struct parserNode *prec11Recur(llLexerItem start, llLexerItem end,
                                      llLexerItem *result);
static struct parserNode *prec12Recur(llLexerItem start, llLexerItem end,
                                      llLexerItem *result);
static struct parserNode *prec13Recur(llLexerItem start, llLexerItem end,
                                      llLexerItem *result);
STR_TYPE_DEF(int, Int);
STR_TYPE_FUNCS(int, Int);
static llLexerItem findOtherSide(llLexerItem start, llLexerItem end) {
	const char *lefts[] = {"(", "[", "{"};
	const char *rights[] = {")", "]", "}"};
	__auto_type count = sizeof(lefts) / sizeof(*lefts);

	strInt stack = NULL;
	int dir = 0;
	do {
		if (start == NULL)
			return NULL;
		if (start == end)
			return NULL;

		if (llLexerItemValuePtr(start)->template == &opTemplate) {
			__auto_type text =
			    *(const char **)lexerItemValuePtr(llLexerItemValuePtr(start));
			int i;
			for (i = 0; i != count; i++) {
				if (0 == strcmp(lefts[i], text))
					goto foundLeft;
				if (0 == strcmp(rights[i], text))
					goto foundRight;
			}
			goto next;
		foundLeft : {
			if (dir == 0)
				dir = 1;

			if (dir == 1)
				stack = strIntAppendItem(stack, i);
			else
				stack = strIntResize(stack, strIntSize(stack) - 1);

			goto next;
		}
		foundRight : {
			if (dir == 0)
				dir = -1;

			if (dir == -1)
				stack = strIntAppendItem(stack, i);
			else
				stack = strIntResize(stack, strIntSize(stack) - 1);
			goto next;
		}
		}
	next:
		if (dir == 0)
			return NULL;

		if (strIntSize(stack) == 0) {
			strIntDestroy(&stack);
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
	strIntDestroy(&stack);
	return NULL;
}
static struct parserNode *precCommaRecur(llLexerItem start, llLexerItem end,
                                         llLexerItem *result) {
	if (start == NULL)
		return NULL;

	struct parserNodeCommaSeq seq;
	seq.base.type = NODE_COMMA_SEQ;
	seq.items = NULL;

	__auto_type node = NULL;
	for (; start != NULL && start != end;) {
		__auto_type comma = expectOp(start, ",");
		if (comma) {
			parserNodeDestroy(&comma);
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
		return ALLOCATE(seq);
	}
}
static struct parserNode *parenRecur(llLexerItem start, llLexerItem end,
                                     llLexerItem *result) {
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

		if (result != NULL)
			*result = start;

		goto success;
	} else {
		retVal = literalRecur(start, end, result);
		goto success;
	}
fail:
	parserNodeDestroy(&retVal);
	retVal = NULL;
success:
	parserNodeDestroy(&right);
	parserNodeDestroy(&left);

	return retVal;
}
struct parserNode *parseExpression(llLexerItem start, llLexerItem end,
                                   llLexerItem *result) {
	return precCommaRecur(start, end, result);
}
struct pnPair {
	struct parserNode *a, *b;
};
STR_TYPE_DEF(struct pnPair, PNPair);
STR_TYPE_FUNCS(struct pnPair, PNPair);
static void strPNPairDestroy2(strPNPair *list) {
	for (long i = 0; i != strPNPairSize(*list); i++) {
		parserNodeDestroy(&list[0][i].a);
		parserNodeDestroy(&list[0][i].b);
	}
	strPNPairDestroy(list);
}
static strPNPair tailBinop(llLexerItem start, llLexerItem end,
                           llLexerItem *result, const char **ops, long opCount,
                           struct parserNode *(*func)(llLexerItem, llLexerItem,
                                                      llLexerItem *)) {
	strPNPair retVal = NULL;
	for (; start != NULL;) {
		struct parserNode *op __attribute__((cleanup(parserNodeDestroy)));
		for (long i = 0; i != opCount; i++) {
			op = expectOp(start, ops[i]);
			if (op != NULL)
				break;
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
	strPNPairDestroy2(&retVal);
	return NULL;
}
static struct parserNode *nameParse(llLexerItem start, llLexerItem end,
                                    llLexerItem *result) {
	if (start == NULL)
		return NULL;

	__auto_type item = llLexerItemValuePtr(start);
	if (item->template == &nameTemplate) {
		if (result != NULL)
			*result = llLexerItemNext(start);

		__auto_type ptr = strClone(lexerItemValuePtr(item));
		struct parserNodeName retVal;
		retVal.base.type = NODE_NAME;
		retVal.pos.start = item->start;
		retVal.pos.end = item->end;
		retVal.text = ptr;

		return ALLOCATE(retVal);
	}
	return NULL;
}
static struct parserNode *pairOperator(const char *left, const char *right,
                                       llLexerItem start, llLexerItem end,
                                       llLexerItem *result, int *success) {
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
		parserNodeDestroy(&exp), exp = NULL;
	parserNodeDestroy(&l);
	parserNodeDestroy(&r);

	return exp;
}
static struct parserNode *prec0Binop(llLexerItem start, llLexerItem end,
                                     llLexerItem *result) {
	if (start == NULL)
		return NULL;

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
				struct parserNodeBinop binop;
				binop.a = head;
				binop.base.type = NODE_BINOP;
				binop.op = ptr;
				binop.b = next;

				head = ALLOCATE(binop);
				goto loop1;
			}
		}
		int success;
		__auto_type funcCallArgs =
		    pairOperator("(", ")", result2, end, &result2, &success);
		if (success) {
			struct parserNodeFuncCall newNode;
			newNode.base.type = NODE_FUNC_CALL;
			newNode.func = head;
			newNode.args = NULL;

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
		__auto_type array =
		    pairOperator("[", "]", result2, end, &result2, &success);
		if (success) {
			struct parserNodeBinop binop;
			binop.a = head;
			binop.base.type = NODE_BINOP;
			binop.op = expectOp(oldResult2, "[");
			binop.b = array;

			head = ALLOCATE(binop);

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
	parserNodeDestroy(&head);
	return NULL;
}
static struct parserNode *prec1Recur(llLexerItem start, llLexerItem end,
                                     llLexerItem *result) {
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
			strParserNodeDestroy2(&opStack);
			goto fail;
		}

		for (long i = strParserNodeSize(opStack) - 1; i >= 0; i--) {
			struct parserNodeUnop unop;
			unop.base.type = NODE_UNOP;
			unop.a = tail;
			unop.isSuffix = 0;
			unop.op = opStack[i];

			tail = ALLOCATE(unop);
		}
	}

	if (result != NULL)
		*result = result2;
	strParserNodeDestroy(&opStack);
	return tail;

fail:
	return tail;
}
static struct parserNode *binopLeftAssoc(
    const char **ops, long count, llLexerItem start, llLexerItem end,
    llLexerItem *result,
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

		head = ALLOCATE(binop);
	}
end:
	if (result != NULL)
		*result = result2;

	return head;
}
static struct parserNode *binopRightAssoc(
    const char **ops, long count, llLexerItem start, llLexerItem end,
    llLexerItem *result,
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

		right = ALLOCATE(binop);
	}

	retVal = right;
end:;
	if (result != NULL)
		*result = result2;

	return retVal;
}
#define LEFT_ASSOC_OP(name, next, ...)                                         \
	static const char *name##Ops[] = {__VA_ARGS__};                              \
	static struct parserNode *name##Recur(llLexerItem start, llLexerItem end,    \
	                                      llLexerItem *result) {                 \
		__auto_type count = sizeof(name##Ops) / sizeof(*name##Ops);                \
		return binopLeftAssoc(name##Ops, count, start, end, result, next);         \
	}
#define RIGHT_ASSOC_OP(name, next, ...)                                        \
	static const char *name##Ops[] = {__VA_ARGS__};                              \
	static struct parserNode *name##Recur(llLexerItem start, llLexerItem end,    \
	                                      llLexerItem *result) {                 \
		__auto_type count = sizeof(name##Ops) / sizeof(*name##Ops);                \
		return binopRightAssoc(name##Ops, count, start, end, result, next);        \
	}
RIGHT_ASSOC_OP(prec13, prec12Recur, "=",
               "-=", "+=", "*=", "/=", "%=", "<<=", ">>=", "&=", "|=", "^=");
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

static struct parserNode *expectKeyword(llLexerItem __item, const char *text) {
	__auto_type item = llLexerItemValuePtr(__item);

	if (item->template == &kwTemplate) {
		__auto_type itemText = *(const char **)lexerItemValuePtr(item);
		if (0 == strcmp(itemText, text)) {
			struct parserNodeKeyword kw;
			kw.base.type = NODE_KW;
			kw.pos.start = item->start;
			kw.pos.end = item->end;
			kw.text = text;

			return ALLOCATE(kw);
		}
	}
	return NULL;
}
struct linkagePair {
	enum linkage link;
	const char *text;
};
static enum linkage getLinkage(llLexerItem start, llLexerItem *result) {
	if (result != NULL)
		*result = start;
	if (start == NULL)
		return 0;

	if (llLexerItemValuePtr(start)->template != &kwTemplate)
		return 0;

	struct linkagePair pairs[] = {
	    {LINKAGE_PUBLIC, "public"}, {LINKAGE_STATIC, "static"},
	    {LINKAGE_EXTERN, "extern"}, {LINKAGE__EXTERN, "_extern"},
	    {LINKAGE_IMPORT, "import"}, {LINKAGE__IMPORT, "_import"},
	};
	__auto_type count = sizeof(pairs) / sizeof(*pairs);

	for (long i = 0; i != count; i++) {
		if (expectKeyword(start, pairs[i].text)) {
			if (result != NULL)
				*result = llLexerItemNext(start);

			return pairs[i].link;
		}
	}

	return 0;
}
static void getPtrsAndDims(llLexerItem start, llLexerItem *end,
                           struct parserNode **name, long *ptrLevel,
                           strParserNode *dims) {
	long ptrLevel2 = 0;
	for (; start != NULL; start = llLexerItemNext(start)) {
		__auto_type op = expectOp(start, "*");
		if (op) {
			parserNodeDestroy(&op);
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

		__auto_type dim = pairOperator("[", "]", start, NULL, &start, NULL);
		dims2 = strParserNodeAppendItem(dims2, dim);
	}

	if (dims != NULL)
		*dims = dims2;
	else
		strParserNodeDestroy(&dims2);

	if (ptrLevel != NULL)
		*ptrLevel = ptrLevel2;

	if (name != NULL)
		*name = name2;
	else
		parserNodeDestroy(&name2);

	if (end != NULL)
		*end = start;
}
static struct object *parseVarDeclTail(llLexerItem start, llLexerItem *end,
                                       struct object *baseType,
                                       struct parserNode **name,
                                       struct parserNode **dftVal,
                                       strParserNode *metaDatas);
struct parserNode *parseSingleVarDecl(llLexerItem start, llLexerItem *end) {
	struct parserNode *base __attribute__((cleanup(parserNodeDestroy)));
	base = nameParse(start, NULL, &start);
	if (base != NULL) {
		struct parserNodeName *baseName = (void *)base;
		__auto_type baseType = objectByName(baseName->text);
		if (baseType == NULL)
			return NULL;

		struct parserNodeVarDecl decl;
		decl.base.type = NODE_VAR_DECL;
		decl.type = parseVarDeclTail(start, end, baseType, &decl.name, &decl.dftVal,
		                             &decl.metaData);
		return ALLOCATE(decl);
	}
	return NULL;
}
struct parserNode *parseVarDecls(llLexerItem start, llLexerItem *end) {
	struct parserNode *base __attribute__((cleanup(parserNodeDestroy)));
	base = nameParse(start, NULL, &start);
	if (base != NULL) {
		strParserNode decls __attribute__((cleanup(strParserNodeDestroy2)));
		decls = NULL;

		struct parserNodeName *baseName = (void *)base;
		__auto_type baseType = objectByName(baseName->text);
		if (baseType == NULL)
			goto fail;

		for (int firstRun = 1;; firstRun = 0) {
			if (!firstRun) {
				struct parserNode *op __attribute__((cleanup(parserNodeDestroy)));
				op = expectOp(start, ",");
				if (!op)
					goto end;

				start = llLexerItemNext(start);
			}

			struct parserNodeVarDecl decl;
			decl.base.type = NODE_VAR_DECL;
			decl.type = parseVarDeclTail(start, &start, baseType, &decl.name,
			                             &decl.dftVal, &decl.metaData);

			decls = strParserNodeAppendItem(decls, ALLOCATE(decl));
		}
	end:;
		if (end != NULL)
			*end = start;

		if (strParserNodeSize(decls) == 1) {
			return decls[0];
		} else if (strParserNodeSize(decls) > 1) {
			struct parserNodeVarDecls retVal;
			retVal.base.type = NODE_VAR_DECLS;
			retVal.decls = strParserNodeAppendData(
			    NULL, (const struct parserNode **)decls, strParserNodeSize(decls));

			return ALLOCATE(retVal);
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
		return start;
	}

	return NULL;
}
static struct object *parseVarDeclTail(llLexerItem start, llLexerItem *end,
                                       struct object *baseType,
                                       struct parserNode **name,
                                       struct parserNode **dftVal,
                                       strParserNode *metaDatas) {
	if (metaDatas != NULL)
		*metaDatas = NULL;

	__auto_type funcPtrLeft = expectOp(start, "(");
	long ptrLevel = 0;
	struct parserNode *eq __attribute__((cleanup(parserNodeDestroy)));
	eq = NULL;
	strParserNode dims __attribute__((cleanup(strParserNodeDestroy)));
	dims = NULL;
	struct object *retValType = NULL;

	if (funcPtrLeft != NULL) {
		start = llLexerItemNext(start);
		parserNodeDestroy(&funcPtrLeft);

		getPtrsAndDims(start, &start, name, &ptrLevel, &dims);

		__auto_type r = expectOp(start, ")");
		start = llLexerItemNext(start);
		if (r == NULL) {
			parserNodeDestroy(&r);
			goto fail;
		}

		struct parserNode *l2 __attribute__((cleanup(parserNodeDestroy)));
		l2 = expectOp(start, "(");
		start = llLexerItemNext(start);

		strFuncArg args __attribute__((cleanup(strFuncArgDestroy2)));
		args = NULL;
		for (int firstRun = 1;; firstRun = 0) {
			struct parserNode *r2 __attribute__((cleanup(parserNodeDestroy)));
			r2 = expectOp(start, ")");
			if (r2 != NULL) {
				start = llLexerItemNext(start);
				break;
			}

			// Check for comma if not firt run
			if (!firstRun) {
				struct parserNode *comma __attribute__((cleanup(parserNodeDestroy)));
				comma = expectOp(start, ",");
				if (comma == NULL)
					goto fail;

				start = llLexerItemNext(start);
			}

			struct parserNodeVarDecl *argDecl =
			    (void *)parseSingleVarDecl(start, &start);
			if (argDecl == NULL)
				goto fail;

			struct objectFuncArg arg;
			arg.dftVal = argDecl->dftVal;
			arg.name = argDecl->name;
			arg.type = argDecl->type;
			free(argDecl);

			args = strFuncArgAppendItem(args, arg);
		}
		retValType = objectFuncCreate(baseType, args);
	} else {
		getPtrsAndDims(start, &start, name, &ptrLevel, &dims);
		retValType = baseType;
	}

	// Make type has pointers and dims
	for (long i = 0; i != ptrLevel; i++)
		retValType = objectPtrCreate(retValType);
	for (long i = 0; i != strParserNodeSize(dims); i++)
		retValType = objectArrayCreate(retValType, dims[i]);

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
		__auto_type dftVal2 = parseExpression(start, expEnd, &start);

		if (dftVal != NULL)
			*dftVal = dftVal2;
	} else {
		if (dftVal != NULL)
			*dftVal = NULL;
	}

	if (metaDatas != NULL)
		*metaDatas = metaDatas2;
	else
		strParserNodeDestroy2(&metaDatas2);

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
			decls2 =
			    strParserNodeAppendData(NULL, (const struct parserNode **)d->decls,
			                            strParserNodeSize(d->decls));
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
		strParserNodeDestroy2(&decls2);

		if (end != NULL)
			*end = start;
		return members;
	}
	if (end != NULL)
		*end = start;
	return NULL;
}
struct parserNode *parseClass(llLexerItem start, llLexerItem *end) {
	__auto_type originalStart = start;
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

		struct parserNode *l __attribute__((cleanup(parserNodeDestroy))) =
		    expectKeyword(start, "{");
		start = llLexerItemNext(start);
		if (l == NULL)
			goto end;

		strObjectMember members __attribute__((cleanup(strObjectMemberDestroy))) =
		    NULL;
		for (;;) {
			struct parserNode *r __attribute__((cleanup(parserNodeDestroy))) =
			    expectKeyword(start, "}");
			if (r != NULL) {
				start = llLexerItemNext(start);
				break;
			}

			__auto_type newMembers = parseMembers(start, &start);
			members = strObjectMemberConcat(members, newMembers);

			__auto_type kw = expectKeyword(start, ";");
			if (kw != NULL) {
				start = llLexerItemNext(start);
			} else {
				// TODO whine if no semicolon
			}
			parserNodeDestroy(&kw);
		}

		char *className = NULL;
		if (name2 != NULL) {
			struct parserNodeName *name = (void *)name2;
			className = strClone(name->text);
		}

		if (cls) {
			retValObj =
			    objectClassCreate(className, members, strObjectMemberSize(members));
			free(className);
		} else if (un) {
			retValObj =
			    objectUnionCreate(className, members, strObjectMemberSize(members));
			free(className);
		}
	}
	if (cls) {
		struct parserNodeClassDef def;
		def.base.type = NODE_CLASS_DEF;
		def.name = name2;
		def.type = retValObj;

		retVal = ALLOCATE(def);
	} else if (un) {
		struct parserNodeUnionDef def;
		def.base.type = NODE_UNION_DEF;
		def.name = name2;
		def.type = retValObj;

		retVal = ALLOCATE(def);
	}
end:
	parserNodeDestroy(&cls);
	parserNodeDestroy(&un);
	parserNodeDestroy(&baseName);

	return retVal;
}
