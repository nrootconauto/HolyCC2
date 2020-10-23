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
	const char *text = "0 + 1 + 2 + 3";
	__auto_type textStr = strCharAppendData(NULL, text, strlen(text));

	__auto_type g = holyCGrammarCreate();

	__auto_type lex = lexerCreate((struct __vec *)textStr, holyCLexerTemplates(),
	                              charCmp, skipWhitespace);

	int success;
	struct parserNode *node = parse(g, lexerGetItems(lex), &success, NULL);
	assert(success);
	{
		/**
		 *       +
		 *      / \
		 *     +   3
		 *    / \
		 *   +   2
		 *  / \
		 * 0   1
		 */
		assert(node->type == NODE_EXPR_BINOP);
		struct parserNodeBinop *binop = (void *)node;

		assert(binop->b->type == NODE_LIT_INT);
		struct parserNodeIntLiteral *intLit = (void *)binop->b;
		assert(intLit->value.value.sInt == 3);

		assert(binop->a->type == NODE_EXPR_BINOP);
		binop = (void *)binop->a;

		intLit = (void *)binop->b;
		assert(intLit->value.value.sInt == 2);

		binop = (void *)binop->a;

		intLit = (void *)binop->a;
		assert(intLit->value.value.sInt == 0);
		intLit = (void *)binop->b;
		assert(intLit->value.value.sInt == 1);
	}
	for (int i = 0; i != 2; i++) {
		strCharDestroy(&textStr);
		text = (i == 0) ? "a=b=c=d" : "a=(b)=(c)=(d)";
		textStr = strCharAppendData(NULL, text, strlen(text));
		int err;
		lexerUpdate(lex, (struct __vec *)textStr, &err);
		assert(!err);

		struct parserNode *node = parse(g, lexerGetItems(lex), &success, NULL);
		{
			assert(node->type == NODE_EXPR_BINOP);
			struct parserNodeBinop *binop = (void *)node;

			assert(binop->a->type == NODE_NAME_TOKEN);
			struct parserNodeNameToken *token = (void *)binop->a;
			assert(0 == strcmp(token->text, "a"));

			assert(binop->b->type == NODE_EXPR_BINOP);
			binop = (void *)binop->b;

			assert(binop->a->type == NODE_NAME_TOKEN);
			token = (void *)binop->a;
			assert(0 == strcmp(token->text, "b"));

			assert(binop->b->type == NODE_EXPR_BINOP);
			binop = (void *)binop->b;

			assert(binop->a->type == NODE_NAME_TOKEN);
			token = (void *)binop->a;
			assert(0 == strcmp(token->text, "c"));
			assert(binop->b->type == NODE_NAME_TOKEN);
			token = (void *)binop->b;
			assert(0 == strcmp(token->text, "d"));
		}
	}
}