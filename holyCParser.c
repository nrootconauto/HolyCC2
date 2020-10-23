#include <assert.h>
#include <holyCParser.h>
#include <parserBase.h>
#include <stdio.h>
#include <stdlib.h>
#define ALLOCATE(x)                                                            \
	({                                                                           \
		void *r = malloc(sizeof(x));                                               \
		memcpy(r, &x, sizeof(x));                                                  \
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
	case NODE_NAME_TOKEN:
		return 1;
	default:
		return 0;
	}
}
STR_TYPE_DEF(struct parserNode *, Node);
STR_TYPE_FUNCS(struct parserNode *, Node);
static void *binopRuleBase(const char **ops, long opsCount,
                           const void **__items, long __itemsCount,
                           enum assoc dir) {
	/**
	 * NOTE:
	 * items[0] is a vector of nodes,so __itemsCount will always be 1
	 */
	assert(__itemsCount == 1);

	const strNode items = (const strNode)__items[0];
	long itemsCount = strNodeSize(items);
	if (itemsCount < 3) {
		if (itemsCount == 1)
			return (void *)items[0];
		else
			return NULL;
	}

	strNode result = strNodeReserve(NULL, itemsCount / 2);
	for (long offset = 0; offset + 2 < itemsCount; offset += 2) {
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
		retVal.b = (lastRun) ? (struct parserNode *)items2[offset + 2] : NULL;
		retVal.op = (void *)term;

		result = strNodeAppendItem(result, ALLOCATE(retVal));
	}
	}

	__auto_type len = strNodeSize(result);
	struct parserNodeBinop *current;
	if (dir == DIR_RIGHT) {
		for (long i = 0; i < len; i++) {
			current = (void *)result[i];
			current->a = items[i * 2];

			if (i + 1 < len)
				current->b = result[i + 1];
		}
		current = (void *)result[0];
	} else if (dir == DIR_LEFT) {
		for (long i = len - 1; i >= 1; i--) {
			current = (void *)result[i];
			current->a = result[i - 1];

			struct parserNodeBinop *prev = (void *)result[i - 1];
			prev->b = items[i * 2];
		}
		current = (void *)result[len - 1];
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
			retVal.a = (struct parserNode *)items[argIndex];
			retVal.op = (void *)op;
		}
	}

	return NULL;
}
static struct __lexerItemTemplate intTemplate;
static struct __lexerItemTemplate stringTemplate;
static struct __lexerItemTemplate floatingTemplate;
static struct __lexerItemTemplate operatorTemplate;
static struct __lexerItemTemplate nameTemplate;
static strLexerItemTemplate templates;
const strLexerItemTemplate holyCLexerTemplates() { return templates; }
#define OP_TERMINAL(name, ...)                                                 \
	static void *name##Treminal(const struct __lexerItem *item) {                \
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
OP_TERMINAL(leftParen, "(");
OP_TERMINAL(rightParen, ")");
OP_TERMINAL(comma, ",");
#define PREC_UNOP(name, leftSide, ...)                                         \
	static void *name(const void **items, long count) {                          \
		static const char *operators[] = {__VA_ARGS__};                            \
		long opsCount = sizeof(operators) / sizeof(*operators);                    \
		return unopRuleBase(operators, opsCount, items, count, leftSide);          \
	}                                                                            \
	OP_TERMINAL(name, __VA_ARGS__);
#define PREC_BINOP(name, dir, ...)                                             \
	static void *name(const void **items, long count) {                          \
		static const char *operators[] = {__VA_ARGS__};                            \
		long opsCount = sizeof(operators) / sizeof(*operators);                    \
		return binopRuleBase(operators, opsCount, items, count, dir);              \
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

	const char *keywords[] = {
	    "if",
	    "else",
	    //
	    "while",
	    "do",
	    "for",
	    "break",
	    //
	    "class",
	    "union",
	    //
	    "switch",
	    "case",
	    "default",
	    //
	    "reg",
	    "noreg",
	    //
	    "asm",
	    //
	    "static",
	    "extern",
	    "_extern",
	    "import",
	    "_import",
	    "public",
	};
	__auto_type keywordCount = sizeof(keywords) / sizeof(*keywords);

	nameTemplate = nameTemplateCreate(keywords, keywordCount);

	const struct __lexerItemTemplate *templates2[] = {
	    &intTemplate,
	    &operatorTemplate,
	    //&floatingTemplate,
	    &stringTemplate,
	    &nameTemplate,
	};
	long templateCount = sizeof(templates2) / sizeof(*templates2);
	templates = strLexerItemTemplateAppendData(NULL, templates2, templateCount);
}
static void *floatingLiteral(const struct __lexerItem *item) {
	if (item->template == &floatingTemplate) {
		struct parserNodeFloatingLiteral lit;
		lit.pos.start = item->start;
		lit.pos.end = item->end;
		lit.base.type = NODE_LIT_FLOATING;
		lit.value = *(struct lexerFloating *)lexerItemValuePtr(item);

		return ALLOCATE(lit);
	}
	return NULL;
}
static void *nameToken(const struct __lexerItem *item) {
	if (item->template == &nameTemplate) {
		const char *text = lexerItemValuePtr(item);
		struct parserNodeNameToken retVal;
		retVal.base.type = NODE_NAME_TOKEN;
		retVal.pos.start = item->start;
		retVal.pos.end = item->end;
		retVal.text = text;

		return ALLOCATE(retVal);
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

		struct parserNode *node = ALLOCATE(lit);
		return node;
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

	if (count != 0)
		printf("count:%li\n", count); // TODO
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
static void *__prependVec(const void **items, long count) {
	assert(count == 2);
	strNode retVal = strNodeAppendData(NULL, (const struct parserNode **)items[0],
	                                   strNodeSize((strNode)items[0]));
	return strNodeAppendItem(retVal, (struct parserNode *)items[1]);
}
/**
 * 1+[2,3,4]->[1,2,3,4]
 */
static void *__appendVec(const void **items, long count) {
	assert(count == 2);
	strNode retVal = strNodeAppendItem(
	    NULL, (struct parserNode *)items[0]); // TODO dont destroy items[1]
	return strNodeConcat(retVal, (strNode)items[1]);
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
	__auto_type combine = grammarRuleSequenceCreate(combinedName, 1, __appendVec,
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
	__auto_type combined = grammarRuleSequenceCreate(combinedName, 1, __appendVec,
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
static void *parenFunc(const void **data, long count) {
	assert(count == 3);
	return (void *)data[1];
}
/**
 * [1,2,3]+3->[1,2,3,4]
 */
static void *__appendItem(const void **items, long count) {
	assert(count == 2);
	return __vecAppendItem(*(void **)items, items[1],
	                       sizeof(struct parserNode *));
}
/**
 * One item which is a vec
 * Takes [[exp? ","] [exp? ","] ...exp?]
 */
void *commaSequenceFunc(const void **items, long count) {
	assert(count == 1);

	long count2 = strNodeSize((const strNode)items[0]);
	struct parserNode **nodes = (void *)items[0];

	long commaCount = 0;
	for (long i = 1; i < count2; i += 2) {
		if (nodes[i]->type == NODE_OP_TERM)
			commaCount++;
	}
	if (commaCount == 0)
		return nodes[0];

	struct parserNodeCommaSequence seq;
	seq.base.type = NODE_COMMA_SEQ;
	seq.commas = NULL;
	seq.nodes = NULL;

	for (long i = 0; i < count2; i += 2) {
		seq.nodes = strNodeAppendItem(seq.nodes, nodes[i]);
		if (i + 1 < count2)
			seq.commas = strNodeAppendItem(seq.commas, nodes[i + 1]);
	}

	return ALLOCATE(seq);
}
static void *forward1(const void *value) { return (void *)value; }
/**
 * ["(" , typename , ")"]
 */
static void *typecastFunc(const void **data ,long count) {
 assert(count==3);
 
 if(NULL) //TODO resume work
	;
}
static strRule createPrecedenceRules(strRule prev, struct grammarRule **top) {

	const struct grammarRule *precRules[] = {
	    grammarRuleTerminalCreate("LITERAL", 1, intLiteral),
	    grammarRuleTerminalCreate("LITERAL", 1, floatingLiteral),
	    grammarRuleTerminalCreate("LITERAL", 1, stringLiteral),
	    grammarRuleTerminalCreate("COMMA", 1, commaTreminal),
	    grammarRuleTerminalCreate("NAME", 1, nameToken),
	    /**
			 * Type-name
			 */
			grammarRuleSequenceCreate("TYPENAME",1,yes1,"NAME",NULL),//TODO
	    /**
	     * Base expression,used by comma
	     */
	    grammarRuleOrCreate("PREC0", 1, "PREC0_BINOP", "PREC0_UNOP", NULL),
	    /**
			 * Typecast
			 */
			grammarRuleSequenceCreate("__TYPECAST_HEAD",1,yes1,"PREC0",NULL) ,
			grammarRuleSequenceCreate("__TYPECAST_TAIL",1,yes2,"LEFT_PAREN","TYPE","TYPENAME","RIGHT_PAREN",NULL) ,
			grammarRuleSequenceCreate("TYPECAST",1,NULL,"PREC0",NULL), //TODO
	    /**
			 * (explicit) Function call
			 */
			grammarRuleSequenceCreate("__FUNC_CALL",1,yes1,"TYPECAST",NULL), //PROVIDE FUNCTION
	    /**
	     * Comma operator(creates sequences for function args,or returns single
	     * __exp)
	     */
	    grammarRuleOptCreate("EXP_OPT", 1, "FUNC_CALL", forward1),
	    grammarRuleSequenceCreate("COMMA_HEAD", 1, yes1, "EXP_OPT", NULL),
	    grammarRuleSequenceCreate("COMMA_TAIL", 1, yes2, "COMMA", "EXP_OPT",
	                              NULL),
	    grammarRuleRepeatCreate("COMMA_TAIL*", 1, mergeYes2, "COMMA_TAIL"),
	    grammarRuleSequenceCreate("__COMMA_COMPLETE", 1, __appendVec,
	                              "COMMA_HEAD", "COMMA_TAIL*", NULL),
	    grammarRuleSequenceCreate("COMMA_EXP", 1, commaSequenceFunc,
	                              "__COMMA_COMPLETE", NULL),
	    /**
	     * Parenethesis
	     */
	    grammarRuleTerminalCreate("LEFT_PAREN", 1, leftParenTreminal),
	    grammarRuleTerminalCreate("RIGHT_PAREN", 1, rightParenTreminal),
	    grammarRuleSequenceCreate("PAREN", 1, parenFunc, "LEFT_PAREN", "EXP",
	                              "RIGHT_PAREN", NULL),
	    grammarRuleSequenceCreate("PAREN", 2, yes1, "NAME_OR_LITERAL", NULL),
	    grammarRuleOrCreate("NAME_OR_LITERAL", 1, "NAME", "LITERAL", NULL),
	    /**
	     * Standard operators
	     */
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
	prev = strRuleAppendData(prev, precRules, count);
	//
	DEFINE_PREC_LEFT_ASSOC_BINOP(prev, PREC0_BINOP, "OP0_BINOP", "PREC1",
	                             prec0_Binop);
	DEFINE_PREC_RIGHT_ASSOC_BINOP(prev, PREC0_UNOP, "OP0_UNOP", "PREC1",
	                              prec0_Unop);
	DEFINE_PREC_LEFT_SIDE_UNOP(prev, PREC1, "OP1", "PREC2", prec1); // Unops
	DEFINE_PREC_LEFT_ASSOC_BINOP(prev, PREC2, "OP2", "PREC3", prec2);
	DEFINE_PREC_LEFT_ASSOC_BINOP(prev, PREC3, "OP3", "PREC4", prec3);
	DEFINE_PREC_LEFT_ASSOC_BINOP(prev, PREC4, "OP4", "PREC5", prec4);
	DEFINE_PREC_LEFT_ASSOC_BINOP(prev, PREC5, "OP5", "PREC6", prec5);
	DEFINE_PREC_LEFT_ASSOC_BINOP(prev, PREC6, "OP6", "PREC7", prec6);
	DEFINE_PREC_LEFT_ASSOC_BINOP(prev, PREC7, "OP7", "PREC8", prec7);
	DEFINE_PREC_LEFT_ASSOC_BINOP(prev, PREC8, "OP8", "PREC9", prec8);
	DEFINE_PREC_LEFT_ASSOC_BINOP(prev, PREC9, "OP9", "PREC10", prec9);
	DEFINE_PREC_LEFT_ASSOC_BINOP(prev, PREC10, "OP10", "PREC11", prec10);
	DEFINE_PREC_LEFT_ASSOC_BINOP(prev, PREC11, "OP11", "PREC12", prec11);
	DEFINE_PREC_LEFT_ASSOC_BINOP(prev, PREC12, "OP12", "PREC13", prec12);
	//
	DEFINE_PREC_RIGHT_ASSOC_BINOP(prev, PREC13, "OP13", "PAREN", prec13);
	//

	// Top
	__auto_type top2 = grammarRuleOrCreate("EXP", 1, "COMMA_EXP", NULL);
	prev = strRuleAppendItem(prev, top2);
	if (top != NULL)
		*top = top2;

	return prev;
}
struct grammar *holyCGrammarCreate() {
	struct grammarRule *top;
	strRule rules = createPrecedenceRules(NULL, &top);
	return grammarCreate(top, rules, strRuleSize(rules));
}
