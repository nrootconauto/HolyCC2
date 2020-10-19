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

static struct __vec *terminalInt(const struct __lexerItem *item) {
	if (item->template == &intTemplate) {
		struct lexerInt *i = lexerItemValuePtr(item);

		struct node retVal;
		retVal.type = TYPE_NUM;
		retVal.value.num = i->value.sInt;

		return __vecAppendItem(NULL, &retVal, sizeof(retVal));
	}
	return NULL;
}
static struct __vec *terminalOp(const struct __lexerItem *item) {
	if (item->template == &opsTemplate) {
		__auto_type k = lexerItemValuePtr(item);
		const char *valids[] = {"+", "-", "*", "/","(",")"};
		long count = sizeof(valids) / sizeof(*valids);
		for (long i = 0; i != count; i++) {
			if (0 == strcmp(k, valids[i])) {
				struct node retVal;

				retVal.type = TYPE_NUM;
				retVal.value.opTerm = valids[i];

				return __vecAppendItem(NULL, &retVal, sizeof(retVal));
			}
		}
	}
	return NULL;
}
static struct __vec *opSeq(const void **items, const char **valids,
                           long validsCount, long len) {
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

	return __vecAppendItem(NULL, &retVal, sizeof(retVal));
}
static struct __vec *paren(const void **items, long len) {
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
static struct __vec *opPrec1(const void **items, long len) {
	const char *valids[] = {"+", "-"};
	return opSeq(items, valids, 2, len);
}
static struct __vec *opPrec2(const void **items, long len) {
	const char *valids[] = {"*", "/"};
	return opSeq(items, valids, 2, len);
}
static void killNode(void *node) {
 free(node);
}
static struct rule *createGrammar(strRuleP *allRules) {
 __auto_type iT=ruleTerminalCreate(1,terminalInt,killNode);
 __auto_type opT=ruleTerminalCreate(1,terminalOp,killNode);
 __auto_type op=ruleSequenceCreate(1,opPrec1,killNode,iT,opT,NULL);
 
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
	long templateCount = sizeof(ops) / sizeof(*ops);
	__auto_type templatesVec =
	    strLexerItemTemplateAppendData(NULL, templates, templateCount);

	const char *text = "1 + 2 + 3";
	__auto_type textStr = strCharAppendData(NULL, text, strlen(text) + 1);

	lexerCreate((struct __vec *)textStr, templatesVec, charCmp, skipWhitespace);
	parse();
}
