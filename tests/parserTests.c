#include <assert.h>
#include <cacheingLexerItems.h>
#include <holyCParser.h>
#include <parserBase.h>
STR_TYPE_DEF(char, Char);
STR_TYPE_FUNCS(char, Char);
static int charCmp(const void *a, const void *b) {
	return *(char *)a == *(char *)b;
}
void parserTests() {
	const char *text = "1 + 2 + 3";
	__auto_type textStr = strCharAppendData(NULL, text, strlen(text));

	__auto_type g = holyCGrammarCreate();

	__auto_type lex = lexerCreate((struct __vec *)textStr, holyCLexerTemplates(),
	                              charCmp, skipWhitespace);

	int success;
	struct parserNode *node = parse(g, lexerGetItems(lex), &success, NULL);
	assert(success);
	{
		/**
		 *     +
		 *    / \
		 *   +   3
		 *  / \
		 * 1   2
		 */
		assert(node->type == NODE_EXPR_BINOP);
		struct parserNodeBinop *binop = (void *)node;

		assert(binop->b->type == NODE_LIT_INT);
		struct parserNodeIntLiteral *intLit = (void *)binop->b;
		assert(intLit->value.value.sInt == 3);

		assert(binop->a->type == NODE_EXPR_BINOP);
		binop = (void *)binop->a;

		intLit = (void *)binop->a;
		assert(intLit->value.value.sInt == 1);
		intLit = (void *)binop->b;
		assert(intLit->value.value.sInt == 2);
	}
}
