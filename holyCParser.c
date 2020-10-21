#include <assert.h>
#include <holyCParser.h>
#include <parserBase.h>
#include <stdlib.h>
#define ALLOCATE(x)                                                            \
	({                                                                           \
		void *r = malloc(sizeof(x));                                               \
		r;                                                                         \
	})
enum assoc {
	DIR_LEFT,
	DIR_RIGHT,
};
static void parserNodeDestroy(struct parserNode **node) { free(*node); }
static int isExpression(const struct parserNode *node) {
	switch (node->type) {
	case NODE_LIT_CHAR:
	case NODE_LIT_FLOATING:
	case NODE_LIT_INT:
	case NODE_LIT_STRING:
	case NODE_EXPR_BINOP:
	case NODE_EXPR_UNOP:
		return 1;
	default:
		return 0;
	}
}
STR_TYPE_DEF(struct parserNode *, Node);
STR_TYPE_FUNCS(struct parserNode *, Node);
static void *binopRuleBase(const char **ops, long opsCount, const void **items,
                           long itemsCount, enum assoc dir) {
	if (itemsCount < 3) {
		if (itemsCount == 1)
			return (void *)items[0];
		else
			return NULL;
	}

	strNode result = strNodeReserve(NULL, itemsCount / 2);
	for (long offset = 0; offset < itemsCount; offset += 2) {
		const struct parserNode **items2 = (void *)items;
		if (!isExpression(items2[offset + 0]) || !isExpression(items2[offset + 2]))
			return NULL;
		if (items2[offset + 1]->type != NODE_OP_TERM)
			return NULL;

		const struct parserNodeOpTerm *term = (void *)items2[offset + 1];
		for (long i = 0; i != opsCount; i++) {
			if (0 == strcmp(ops[i], term->text)) {
				goto success;
			}
		}
		//
		for (long i = 0; i != strNodeSize(result); i++)
			parserNodeDestroy(&result[i]);
		strNodeDestroy(&result);
		return NULL;
	success : {
		int lastRun = offset + 2 >= itemsCount - 1;
		struct parserNodeBinop retVal;
		retVal.base.type = NODE_EXPR_BINOP;
		retVal.a = (offset == 0) ? items[0] : NULL;
		retVal.b = (lastRun) ? items2[offset + 2] : NULL;
		retVal.op = (void *)term;

		result = strNodeAppendItem(result, ALLOCATE(retVal));
	}
	}

	__auto_type len = strNodeSize(result);
	struct parserNodeBinop *current;
	if (dir == DIR_LEFT) {
		for (long i = 0; i < len - 1; i++) {
			current = (void *)result[i];
			current->b = result[i + 1];
		}
	} else if (dir == DIR_RIGHT) {
		for (long i = len - 1; i >= 1; i--) {
			current = (void *)result[i];
			current->a = result[i - 1];
		}
	} else {
		assert(0);
	}
	return current;
}
static void *unopRuleBase(const char **ops, long opsCount, const void **items,
                          long itemsCount, int leftSide) {
	if (itemsCount != 2) {
		if (itemsCount == 1)
			return (void *)items[0];

		return NULL;
	}

	__auto_type items2 = (struct parserNode **)items;

	if (!isExpression(items2[1]))
		return NULL;
	if (items2[0]->type != NODE_OP_TERM)
		return NULL;

	int opIndex = (!leftSide) ? 1 : 0;
	int argIndex = (!leftSide) ? 0 : 1;

	__auto_type op = (struct parserNodeOpTerm *)items[opIndex];
	for (long i = 0; i != opsCount; i++) {
		if (0 == strcmp(ops[i], op->text)) {
			struct parserNodeUnop retVal;
			retVal.base.type = NODE_EXPR_UNOP;
			retVal.a = items[argIndex];
			retVal.op = (void *)op;
		}
	}

	return NULL;
}
static struct __lexerItemTemplate intTemplate;
static struct __lexerItemTemplate stringTemplate;
static struct __lexerItemTemplate floatingTemplate;
static struct __lexerItemTemplate operatorTemplate;
static strLexerItemTemplate templates;
const strLexerItemTemplate holyCLexerTemplates() { return templates; }
#define OP_TERMINAL(name, ...)                                                 \
	void *name##Treminal(const struct __lexerItem *item) {                       \
		if (item->template == &operatorTemplate) {                                 \
			const char *items[] = {__VA_ARGS__};                                     \
			long count = sizeof(items) / sizeof(*items);                             \
			__auto_type text = *(const char **)lexerItemValuePtr(item);              \
			for (long i = 0; i != count; i++) {                                      \
				if (0 == strcmp(items[i], text)) {                                     \
					struct parserNodeOpTerm term;                                        \
					term.pos.start = item->start;                                        \
					term.pos.end = item->end;                                            \
					term.base.type = NODE_OP_TERM;                                       \
					term.text = *(const char **)lexerItemValuePtr(item);                 \
					return ALLOCATE(term);                                               \
				}                                                                      \
			}                                                                        \
		}                                                                          \
		return NULL;                                                               \
	}
#define PREC_UNOP(name, leftSide, ...)                                         \
	static void *name(const void **items, long count) {                          \
		static const char *operators[] = {__VA_ARGS__};                            \
		long opsCount = sizeof(operators) / sizeof(*operators);                    \
		return unopRuleBase(operators, opsCount, items, count, leftSide);       \
	}                                                                            \
	OP_TERMINAL(name, __VA_ARGS__);
#define PREC_BINOP(name, dir, ...)                                             \
	static void *name(const void **items, long count) {                          \
		static const char *operators[] = {__VA_ARGS__};                            \
		long opsCount = sizeof(operators) / sizeof(*operators);                    \
		return binopRuleBase(operators, opsCount, items, count, dir);           \
	}                                                                            \
	OP_TERMINAL(name, __VA_ARGS__);

PREC_BINOP(prec0_Binop, DIR_LEFT, ".", "->");
PREC_UNOP(prec0_Unop, 0, "++", "--");
PREC_UNOP(prec1, 0, "++", "--", "+", "-", "!", "~", "*", "&");
PREC_BINOP(prec2, DIR_LEFT, "`", "<<", ">>");
PREC_BINOP(prec3, DIR_LEFT, "*", "/", "%");
PREC_BINOP(prec4, DIR_LEFT, "&");
PREC_BINOP(prec5, DIR_LEFT, "^");
PREC_BINOP(prec6, DIR_LEFT, "|");
PREC_BINOP(prec7, DIR_LEFT, "+", "-");
PREC_BINOP(prec8, DIR_LEFT, ">=", "<=", ">", "<");
PREC_BINOP(prec9, DIR_LEFT, "==", "!=");
PREC_BINOP(prec10, DIR_LEFT, "&&");
PREC_BINOP(prec11, DIR_LEFT, "^^");
PREC_BINOP(prec12, DIR_LEFT, "||");
PREC_BINOP(prec13, DIR_RIGHT, "=",
           "&=", "|=", "^=", "<<=", ">>=", "+=", "-=", "*=", "%=", "/=");

STR_TYPE_DEF(struct grammarRule *, Rule);
STR_TYPE_FUNCS(struct grammarRule *, Rule);
static void initTemplates() __attribute__((constructor));
static void initTemplates() {
	intTemplate = intTemplateCreate();
	stringTemplate = stringTemplateCreate();
	// floatingTemplate = floatingTemplateCreate();

	const char *operators[] = {
	    "~",
	    "!"
	    //
	    ".",
	    "->"
	    //
	    "`",
	    "<<", ">>",
	    //
	    "*", "/", "%",
	    //
	    "+", "-",
	    //
	    "++", "--",
	    //
	    "&&", "||", "^^",
	    //
	    "&", "|", "^",
	    //
	    "(", ")", "[", "]", ",",
	    //
	    "=", "+=", "-=", "*=", "/=", "%=", "|=", "&=", "^=", ">>=", "<<=="};
	__auto_type operCount = sizeof(operators) / sizeof(*operators);
	operatorTemplate = keywordTemplateCreate(operators, operCount);

	const struct __lexerItemTemplate *templates2[] = {
	    &intTemplate,
	    &operatorTemplate,
	    //&floatingTemplate,
	    &stringTemplate,
	};
	long templateCount = sizeof(templates2) / sizeof(*templates2);
	templates = strLexerItemTemplateAppendData(NULL, templates2, templateCount);
}
static void *floatingLiteral(const struct __lexerItem *item) {
	if (item->template != &floatingTemplate) {
		struct parserNodeFloatingLiteral lit;
		lit.pos.start = item->start;
		lit.pos.end = item->end;
		lit.base.type = NODE_LIT_FLOATING;
		lit.value = *(struct lexerFloating *)lexerItemValuePtr(item);

		return ALLOCATE(lit);
	}
	return NULL;
}
static void *intLiteral(const struct __lexerItem *item) {
	if (item->template == &intTemplate) {
		struct parserNodeIntLiteral lit;
		lit.pos.start = item->start;
		lit.pos.end = item->end;
		lit.base.type = NODE_LIT_INT;
		lit.value = *(struct lexerInt *)lexerItemValuePtr(item);

		return ALLOCATE(lit);
	}
	return NULL;
};
static void *stringLiteral(const struct __lexerItem *item) {
	if (item->template == &stringTemplate) {
		struct parserNodeStringLiteral lit;
		lit.pos.start = item->start;
		lit.pos.end = item->end;
		lit.base.type = NODE_LIT_STRING;
		lit.value = *(struct parsedString *)lexerItemValuePtr(item);

		return ALLOCATE(lit);
	}
	return NULL;
};
struct pair {
	const struct parserNode *a, *b;
};
static void *yes2(const void **nodes, long count) {
	if (count != 2)
		return NULL;
	struct pair p = {nodes[0], nodes[1]};
	return ALLOCATE(p);
}
static void *yes1(const void **data, long length) {
	if (length == 1)
		return (void *)data[0];

	return NULL;
}
/**
 * [[1,2],[3,4]]->[1,2,3,4]
 */
static void *mergeYes2(const void **nodes, long count) {
	strNode retVal = strNodeResize(NULL, 2 * count);

	for (long i = 0; i != count; i++) {
		const struct pair *node = nodes[i];
		retVal[2 * i] = (struct parserNode *)node->a;
		retVal[2 * i + 1] = (struct parserNode *)node->b;
	}

	return retVal;
}
/**
 * [1,2,3]+4->[1,2,3,4]
 */
static void *mergeLeft(const void **items, long count) {
	assert(count == 2);
	return strNodeAppendItem((strNode)items[0], (struct parserNode *)items[1]);
}
/**
 * 1+[2,3,4]->[1,2,3,4]
 */
static void *mergeRight(const void **items, long count) {
	assert(count == 2);
	strNode retVal = strNodeAppendItem(NULL, (struct parserNode *)items[1]);
	return strNodeConcat(retVal, (strNode)items[0]);
}
#define DEFINE_PREC_LEFT_SIDE_UNOP(appendRulesTo, name, opName, nextPrecName,  \
                                   func)                                       \
	({                                                                           \
		__auto_type name =                                                         \
		    grammarRuleSequenceCreate(#name, 1, func, opName, nextPrecName, NULL); \
		__auto_type name2 =                                                        \
		    grammarRuleSequenceCreate(#name, 2, yes1, nextPrecName, NULL);         \
		appendRulesTo = strRuleAppendItem(appendRulesTo, name);                    \
		appendRulesTo = strRuleAppendItem(appendRulesTo, name2);                   \
		name;                                                                      \
	})
#define DEFINE_PREC_RIGHT_SIDE_UNOP(appendRulesTo, name, opName, nextPrecName, \
                                    func)                                      \
	({                                                                           \
		__auto_type name =                                                         \
		    grammarRuleSequenceCreate(#name, 1, func, nextPrecName, opName, NULL); \
		__auto_type name2 =                                                        \
		    grammarRuleSequenceCreate(#name, 2, yes1, nextPrecName, NULL);         \
		appendRulesTo = strRuleAppendItem(appendRulesTo, name2);                   \
		appendRulesTo = strRuleAppendItem(appendRulesTo, name);                    \
		name;                                                                      \
	})
static struct grammarRule *
__leftAssocBinop(strRule *appendRulesTo, const char *finalName,
                 const char *opName, const char *headName, const char *tailName,
                 const char *repName, const char *combinedName,
                 const char *nextPrecName, void *(*func)(const void **, long)) {
	__auto_type head =
	    grammarRuleSequenceCreate(headName, 1, yes1, nextPrecName, NULL);
	__auto_type tail =
	    grammarRuleSequenceCreate(tailName, 1, yes2, opName, nextPrecName, NULL);
	__auto_type rep = grammarRuleRepeatCreate(repName, 1, mergeYes2, tailName);
	__auto_type combine = grammarRuleSequenceCreate(combinedName, 1, mergeLeft,
	                                                headName, repName, NULL);
	__auto_type final =
	    grammarRuleSequenceCreate(finalName, 1, func, combinedName, NULL);
	*appendRulesTo = strRuleAppendItem(*appendRulesTo, tail);
	*appendRulesTo = strRuleAppendItem(*appendRulesTo, rep);
	*appendRulesTo = strRuleAppendItem(*appendRulesTo, combine);
	*appendRulesTo = strRuleAppendItem(*appendRulesTo, final);
	*appendRulesTo = strRuleAppendItem(*appendRulesTo, head);
	return final;
}
#define DEFINE_PREC_LEFT_ASSOC_BINOP(appendRulesTo, name, opName,              \
                                     nextPrecName, func)                       \
	__leftAssocBinop(&appendRulesTo, #name, opName, #name "_HEAD",               \
	                 #name "_TAIL", #name "_TAIL*", "__" #name, nextPrecName,    \
	                 func)
static struct grammarRule *
__rightAssocBinop(strRule *appendRulesTo, const char *finalName,
                  const char *opName, const char *headName,
                  const char *tailName, const char *repName,
                  const char *combinedName, const char *nextPrecName,
                  void *(*func)(const void **, long)) {
	__auto_type head =
	    grammarRuleSequenceCreate(headName, 1, yes1, nextPrecName, NULL);
	__auto_type tail =
	    grammarRuleSequenceCreate(tailName, 1, yes2, opName, nextPrecName, NULL);
	__auto_type rep = grammarRuleRepeatCreate(repName, 1, mergeYes2, tailName);
	__auto_type combined = grammarRuleSequenceCreate(combinedName, 1, mergeRight,
	                                                 headName, repName, NULL);
	__auto_type final =
	    grammarRuleSequenceCreate(finalName, 1, func, combinedName, NULL);
	*appendRulesTo = strRuleAppendItem(*appendRulesTo, head);
	*appendRulesTo = strRuleAppendItem(*appendRulesTo, tail);
	*appendRulesTo = strRuleAppendItem(*appendRulesTo, rep);
	*appendRulesTo = strRuleAppendItem(*appendRulesTo, combined);
	*appendRulesTo = strRuleAppendItem(*appendRulesTo, final);
	return final;
}
#define DEFINE_PREC_RIGHT_ASSOC_BINOP(appendRulesTo, name, opName,             \
                                      nextPrecName, func)                      \
	__rightAssocBinop(&appendRulesTo, #name, opName, #name "_HEAD",              \
	                  #name "_TAIL", #name "_TAIL*", "__" #name, nextPrecName,   \
	                  func)
static strRule createPrecedenceRules(struct grammarRule **top) {
	strRule retVal = NULL;

	const struct grammarRule *precRules[] = {
	    grammarRuleTerminalCreate("LITERAL", 1, intLiteral),
	    grammarRuleTerminalCreate("LITERAL", 1, floatingLiteral),
	    grammarRuleTerminalCreate("LITERAL", 1, stringLiteral),

	    grammarRuleTerminalCreate("OP0_BINOP", 1, prec0_BinopTreminal),
	    grammarRuleTerminalCreate("OP0_UNOP", 1, prec0_UnopTreminal),
	    grammarRuleTerminalCreate("OP1", 1, prec1Treminal),
	    grammarRuleTerminalCreate("OP2", 1, prec2Treminal),
	    grammarRuleTerminalCreate("OP3", 1, prec3Treminal),
	    grammarRuleTerminalCreate("OP4", 1, prec4Treminal),
	    grammarRuleTerminalCreate("OP5", 1, prec5Treminal),
	    grammarRuleTerminalCreate("OP6", 1, prec6Treminal),
	    grammarRuleTerminalCreate("OP7", 1, prec7Treminal),
	    grammarRuleTerminalCreate("OP8", 1, prec8Treminal),
	    grammarRuleTerminalCreate("OP9", 1, prec9Treminal),
	    grammarRuleTerminalCreate("OP10", 1, prec10Treminal),
	    grammarRuleTerminalCreate("OP11", 1, prec11Treminal),
	    grammarRuleTerminalCreate("OP12", 1, prec12Treminal),
	    grammarRuleTerminalCreate("OP13", 1, prec13Treminal),
	};
	long count = sizeof(precRules) / sizeof(*precRules);
	retVal = strRuleAppendData(retVal, precRules, count);
	//
	DEFINE_PREC_LEFT_ASSOC_BINOP(retVal, PREC0_BINOP, "OP0_BINOP", "PREC1",
	                             prec0_Binop);
	DEFINE_PREC_RIGHT_ASSOC_BINOP(retVal, PREC0_UNOP, "OP0_UNOP", "PREC1",
	                              prec0_Unop);
	DEFINE_PREC_LEFT_SIDE_UNOP(retVal, PREC1, "OP1", "PREC2", prec1); // Unops
	DEFINE_PREC_LEFT_ASSOC_BINOP(retVal, PREC2, "OP2", "PREC3", prec2);
	DEFINE_PREC_LEFT_ASSOC_BINOP(retVal, PREC3, "OP3", "PREC4", prec3);
	DEFINE_PREC_LEFT_ASSOC_BINOP(retVal, PREC4, "OP4", "PREC5", prec4);
	DEFINE_PREC_LEFT_ASSOC_BINOP(retVal, PREC5, "OP5", "PREC6", prec5);
	DEFINE_PREC_LEFT_ASSOC_BINOP(retVal, PREC6, "OP6", "PREC7", prec6);
	DEFINE_PREC_LEFT_ASSOC_BINOP(retVal, PREC7, "OP7", "PREC8", prec7);
	DEFINE_PREC_LEFT_ASSOC_BINOP(retVal, PREC8, "OP8", "PREC9", prec8);
	DEFINE_PREC_LEFT_ASSOC_BINOP(retVal, PREC9, "OP9", "PREC10", prec9);
	DEFINE_PREC_LEFT_ASSOC_BINOP(retVal, PREC10, "OP10", "PREC11", prec10);
	DEFINE_PREC_LEFT_ASSOC_BINOP(retVal, PREC11, "OP11", "PREC12", prec11);
	DEFINE_PREC_LEFT_ASSOC_BINOP(retVal, PREC12, "OP12", "PREC13", prec12);
	//
	DEFINE_PREC_RIGHT_ASSOC_BINOP(retVal, PREC13, "OP13", "LITERAL", prec13);
	//

	// Top
	__auto_type top2 =
	    grammarRuleOrCreate("EXP", 1, "PREC0_BINOP", "PREC0_UNOP", NULL);
	retVal = strRuleAppendItem(retVal, top2);
	if (top != NULL)
		*top = top2;

	return retVal;
}
struct grammar *holyCGrammarCreate() {
	struct grammarRule *top;
	strRule rules = createPrecedenceRules(&top);
	return grammarCreate(top, rules, strRuleSize(rules));
}
