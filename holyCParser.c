#include <assert.h>
#include <cacheingLexer.h>
#include <cacheingLexerItems.h>
#include <holyCParser.h>
#define DEBUG_PRINT_ENABLE 1
#include <debugPrint.h>
#define ALLOCATTE(x)                                                           \
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
static struct __lexerItemTemplate kwTemplate;
static struct __lexerItemTemplate opTemplate;
static struct __lexerItemTemplate intTemplate;
static struct __lexerItemTemplate strTemplate;
static struct __lexerItemTemplate nameTemplate;
static strLexerItemTemplate templates;
static void initTemplates() __attribute__((constructor));
static void initTemplates() {
	const char *keywords[] = {
	    "if",
	    "else",
	    //
	    "for",
	    "while",
	    "do",
	    "break",
	    //
	    "goto",
	};
	__auto_type kwCount = sizeof(keywords) / sizeof(*keywords);

	const char *operators[] = {
	    "++",
	    "--",
	    //
	    "(",
	    ")",
	    "[",
	    "]",
	    //
	    ".",
	    "->",
	    //
	    "!",
	    "~",
	    //
	    "*",
	    "&",
	    //
	    "/",
	    "%",
	    //
	    "+",
	    "-",
	    //
	    "<<",
	    ">>",
	    //
	    "<",
	    ">",
	    //
	    ">=",
	    "<=",
	    //
	    "==",
	    "!=",
	    //
	    "^",
	    "|",
	    //
	    "&&",
	    "^^",
	    "||",
	    //
	    "=",
	    "+=",
	    "-=",
	    "*=",
	    "/=",
	    "%=",
	    "<<=",
	    ">>=",
	    "&=",
	    "^=",
	    "|=",
	    //
	    ",",
	};
	__auto_type opCount = sizeof(operators) / sizeof(*operators);
	kwTemplate = keywordTemplateCreate(keywords, kwCount);
	opTemplate = keywordTemplateCreate(operators, opCount);
	intTemplate = intTemplateCreate();
	nameTemplate = nameTemplateCreate(keywords, kwCount);
	struct __lexerItemTemplate *templates2[] = {&kwTemplate, &opTemplate,
	                                            &intTemplate, &nameTemplate};
	__auto_type templateCount = sizeof(templates2) / sizeof(*templates2);
	templates =
	    strLexerItemTemplateAppendData(NULL, (void *)templates2, templateCount);
}
strLexerItemTemplate holyCLexerTemplates() { return templates; }
static struct parserNode *expectOp(const struct __lexerItem *item,
                                   const char *text) {
	if (item == NULL)
		return NULL;

	if (item->template == &opTemplate) {
		if (0 == strcmp(*(const char **)lexerItemValuePtr(item), text)) {
			struct parserNodeOpTerm term;
			term.base.type = NODE_OP;
			term.pos.start = item->start;
			term.pos.end = item->end;
			term.text = text;

			return ALLOCATTE(term);
		}
	}
	return NULL;
}
static void parserNodeDestroy(struct parserNode **node) {
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

		return ALLOCATTE(lit);
	} else if (item->template == &strTemplate) {
		if (result != NULL)
			*result = llLexerItemNext(start);

		__auto_type str = *(struct parsedString *)lexerItemValuePtr(item);
		struct parserNodeLitStr lit;
		lit.base.type = NODE_LIT_STR;
		lit.text = strClone((char *)str.text);
		lit.isChar = str.isChar;

		return ALLOCATTE(lit);
	} else if (item->template == &nameTemplate) {
		return nameParse(start, end, result);
	} else {
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
		__auto_type comma = expectOp(llLexerItemValuePtr(start), ",");
		if (comma) {
			parserNodeDestroy(&comma);
			start = llLexerItemNext(start);
			seq.items = strParserNodeAppendItem(seq.items, node);
			node = NULL;
		} else if (node == NULL) {
			node = prec0Binop(start, end, &start);
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
		//Append last item
		seq.items = strParserNodeAppendItem(seq.items, node);
		return ALLOCATTE(seq);
	}
}
static struct parserNode *parenRecur(llLexerItem start, llLexerItem end,
                                     llLexerItem *result) {
	if (start == NULL)
		return NULL;

	struct parserNode *left = expectOp(llLexerItemValuePtr(start), "(");

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
		right = expectOp(llLexerItemValuePtr(res), ")");
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
			op = expectOp(llLexerItemValuePtr(start), ops[i]);
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

		return ALLOCATTE(retVal);
	}
	return NULL;
}
static struct parserNode *pairOperator(const char *left, const char *right,
                                       llLexerItem start, llLexerItem end,
                                       llLexerItem *result) {
	if (start == NULL)
		return NULL;

	if (result != NULL)
		*result = start;

	llLexerItem result2;
	struct parserNode *l = NULL, *r = NULL, *exp = NULL;

	l = expectOp(llLexerItemValuePtr(start), left);
	result2 = llLexerItemNext(start);
	if (l == NULL)
		goto end;
	__auto_type pairEnd = findOtherSide(start, end);

	exp = precCommaRecur(result2, pairEnd, &result2);

	r = expectOp(llLexerItemValuePtr(result2), right);
	if (r == NULL)
		goto end;

	if (result != NULL)
		*result = start;

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
	struct parserNode *head = prec1Recur(start, end, &result2);
	if (head == NULL)
		return NULL;
	const char *binops[] = {".", "->"};
	const char *unops[] = {"--", "++"};
	strPNPair tails = NULL;
	for (; result2 != NULL && result2 != end;) {
		for (long i = 0; i != 2; i++) {
			__auto_type ptr = expectOp(llLexerItemValuePtr(result2), unops[i]);
			if (ptr != NULL) {
				result2 = llLexerItemNext(result2);

				struct parserNodeUnop unop;
				unop.a = head;
				unop.base.type = NODE_UNOP;
				unop.isSuffix = 1;
				unop.op = ptr;

				head = ALLOCATTE(unop);
				goto loop1;
			}
		}
		for (long i = 0; i != 2; i++) {
			__auto_type ptr = expectOp(llLexerItemValuePtr(result2), binops[i]);
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

				head = ALLOCATTE(binop);
				goto loop1;
			}
		}
		__auto_type funcCallArgs = pairOperator("(", ")", result2, end, &result2);
		if (funcCallArgs != NULL) {
			struct parserNodeFuncCall newNode;
			newNode.base.type = NODE_FUNC_CALL;
			newNode.func = head;
			newNode.args = NULL;

			if (funcCallArgs->type == NODE_COMMA_SEQ) {
				struct parserNodeCommaSeq *seq = (void *)funcCallArgs;
				for (long i = 0; i != strParserNodeSize(seq->items); i++)
					newNode.args = strParserNodeAppendItem(newNode.args, seq->items[i]);
			} else if (funcCallArgs != NULL) {
				newNode.args = strParserNodeAppendItem(newNode.args, funcCallArgs);
			}

			head = ALLOCATTE(newNode);
			goto loop1;
		}
		__auto_type oldResult2 = result2;
		__auto_type array = pairOperator("[", "]", result2, end, &result2);
		if (array != NULL) {
			struct parserNodeBinop binop;
			binop.a = head;
			binop.base.type = NODE_BINOP;
			binop.op = expectOp(llLexerItemValuePtr(oldResult2), "[");
			binop.b = array;

			head = ALLOCATTE(binop);

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
			__auto_type op = expectOp(llLexerItemValuePtr(result2), unops[i]);
			if (op != NULL) {
				result2 = llLexerItemNext(result2);
				opStack = strParserNodeAppendItem(opStack, op);
				goto loop1;
			}
		}
		break;
	}
	struct parserNode *tail = prec2Recur(result2, end, &result2);
	if (opStack != NULL) {
		if (tail == NULL) {
			strParserNodeDestroy2(&opStack);
			goto fail;
		}

		for (long i = strParserNodeSize(opStack)-1; i >= 0; i--) {
			struct parserNodeUnop unop;
			unop.base.type = NODE_BINOP;
			unop.a = tail;
			unop.isSuffix = 0;
			unop.op = opStack[i];

			tail = ALLOCATTE(unop);
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

		head = ALLOCATTE(binop);
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

		right = ALLOCATTE(binop);
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
RIGHT_ASSOC_OP(prec13, parenRecur, "=",
               "-=", "+=", "*=", "/=", "%=", "<<=", ">>=", "&=", "|=", "^=");
LEFT_ASSOC_OP(prec12, prec13Recur, "||");
LEFT_ASSOC_OP(prec11, prec12Recur, "^^");
LEFT_ASSOC_OP(prec10, prec11Recur, "&&");
LEFT_ASSOC_OP(prec9, prec10Recur, "==", "!=");
LEFT_ASSOC_OP(prec8, prec9Recur, ">", "<", ">=", "<=");
LEFT_ASSOC_OP(prec7, prec8Recur, "+", "-");
LEFT_ASSOC_OP(prec6, prec7Recur, "|");
LEFT_ASSOC_OP(prec5, prec6Recur, "^");
LEFT_ASSOC_OP(prec4, prec5Recur, "&");
LEFT_ASSOC_OP(prec3, prec4Recur, "*", "%", "/");
LEFT_ASSOC_OP(prec2, prec3Recur, "`", "<<", ">>");
