#include <assert.h>
#include <cacheingLexerItems.h>
#include <parserBase.h>
STR_TYPE_DEF(char, Char);
STR_TYPE_FUNCS(char, Char);
struct node {
	enum {
		TYPE_OP,
		TYPE_OP_TERM,
		TYPE_NUM,
		TYPE_PAREN,
	} type;
	union {
		struct {
			const struct node *left;
			const struct node *right;
			const char *op;
		} op;
		int num;
		const char *opTerm;
		struct {
			const struct node *exp;
		} paren;
	} value;
};
static int charCmp(const void *a, const void *b) {
	return *(char *)a == *(char *)b;
}
static struct __lexerItemTemplate intTemplate;
static struct __lexerItemTemplate opsTemplate;

static void *terminalInt(const struct __lexerItem *item) {
	if (item->template == &intTemplate) {
		struct lexerInt *i = lexerItemValuePtr(item);

		struct node retVal;
		retVal.type = TYPE_NUM;
		retVal.value.num = i->value.sInt;

		void *r = malloc(sizeof(retVal));
		memcpy(r, &retVal, sizeof(retVal));
		return r;
	}
	return NULL;
}
static void *terminalOp(const struct __lexerItem *item) {
	if (item->template == &opsTemplate) {
		const char *k = *(char**)lexerItemValuePtr(item);
		const char *valids[] = {"+", "-", "*", "/", "(", ")"};
		long count = sizeof(valids) / sizeof(*valids);
		for (long i = 0; i != count; i++) {
			if (0 == strcmp(k, valids[i])) {
				struct node retVal;

				retVal.type = TYPE_OP_TERM;
				retVal.value.opTerm = valids[i];

				void *r = malloc(sizeof(retVal));
				memcpy(r, &retVal, sizeof(retVal));
				return r;
			}
		}
	}
	return NULL;
}
static void *opSeq(const void **items, const char **valids, long validsCount,
                   long len) {
	if (len != 3)
		return NULL;

	__auto_type items2 = (const struct node **)items;
	if (items2[1]->type != TYPE_OP_TERM)
		return NULL;
	for (long i = 0; i != validsCount; i++) {
		if (0 == strcmp(valids[i], items2[1]->value.opTerm))
			goto success;
	}
	return NULL;
success:;

	struct node retVal;

	retVal.type = TYPE_OP;
	retVal.value.op.left = items2[0];
	retVal.value.op.op = items2[1]->value.opTerm;
	retVal.value.op.right = items2[2];

	void *r = malloc(sizeof(retVal));
	memcpy(r, &retVal, sizeof(retVal));
	return r;
}
static void *paren(const void **items, long len) {
	if (len != 3)
		return NULL;

	__auto_type items2 = (const struct node **)items;
	if (items2[0]->type != TYPE_OP_TERM || items2[1]->type != TYPE_OP_TERM)
		return NULL;
	if (0 != strcmp(items2[0]->value.opTerm, "("))
		return NULL;
	if (0 != strcmp(items2[2]->value.opTerm, ")"))
		return NULL;
	if (items2[1]->type != TYPE_OP && items2[1]->type != TYPE_NUM)
		return NULL;

	struct node retVal;
	retVal.type = TYPE_PAREN;
	retVal.value.paren.exp = items2[1];

	return __vecAppendItem(NULL, &retVal, sizeof(retVal));
}
static void *opPrec1(const void **items, long len) {
	const char *valids[] = {"+", "-"};
	return opSeq(items, valids, 2, len);
}
static void *opPrec2(const void **items, long len) {
	const char *valids[] = {"*", "/"};
	return opSeq(items, valids, 2, len);
}
static void killNode(void *node) { free(node); }
static void *yes(const void **items, long len) { return (void *)items[0]; }
static struct grammar *createGrammar() {
	__auto_type iT = grammarRuleTerminalCreate("EXP0", 1, terminalInt);
	__auto_type opT = grammarRuleTerminalCreate("OP", 1, terminalOp);

	__auto_type prec1_1 = grammarRuleSequenceCreate("PREC_1", 1, opPrec1, "EXP0",
	                                                "OP", "EXP0", NULL);
	__auto_type prec1_2 =
	    grammarRuleSequenceCreate("PREC_1", 2, yes, "EXP0", NULL);

	__auto_type prec2_1 = grammarRuleSequenceCreate(
	    "PREC_2", 1, opPrec2, "PREC_1", "OP", "PREC_1", NULL);
	__auto_type prec2_2 =
	    grammarRuleSequenceCreate("PREC_2", 2, yes, "PREC_1", NULL);

	__auto_type paren_ =
	    grammarRuleSequenceCreate("EXP0", 1, paren, "OP", "EXP0", "OP", NULL);
	struct grammarRule *rules[] = {
	    iT, opT, prec1_1, prec1_2, prec2_1, prec2_2, paren_,
	};
	long count = sizeof(rules) / sizeof(*rules);
	return grammarCreate(prec2_2, rules, count);
}
void parserTests() {
	const char *ops[] = {"+", "-", "*", "/", "(", ")"};
	long opCount = sizeof(ops) / sizeof(*ops);

	intTemplate = intTemplateCreate();
	opsTemplate = keywordTemplateCreate(ops, opCount);
	const struct __lexerItemTemplate *templates[] = {
	    &intTemplate,
	    &opsTemplate,
	};
	long templateCount = sizeof(templates) / sizeof(*templates);
	__auto_type templatesVec =
	    strLexerItemTemplateAppendData(NULL, templates, templateCount);

	const char *text = "1 + 2 + 3";
	__auto_type textStr = strCharAppendData(NULL, text, strlen(text));

	__auto_type g = createGrammar();

	__auto_type lex = lexerCreate((struct __vec *)textStr, templatesVec, charCmp,
	                              skipWhitespace);

	int success;
	parse(g, lexerGetItems(lex), &success, killNode);
	assert(success);
}
