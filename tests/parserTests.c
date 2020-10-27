#include <assert.h>
#include <cacheingLexerItems.h>
#include <holyCParser.h>
STR_TYPE_DEF(char, Char);
STR_TYPE_FUNCS(char, Char);
static int charCmp(const void *a, const void *b) {
	return *(char *)a == *(char *)b;
}
void parserTests() {
	const char *text = "0 + 1 + 2 + 3";
	__auto_type textStr = strCharAppendData(NULL, text, strlen(text));

	__auto_type lex = lexerCreate((struct __vec *)textStr, holyCLexerTemplates(),
	                              charCmp, skipWhitespace);

	int success;
	struct parserNode *node =
	    parseExpression(llLexerItemFirst(lexerGetItems(lex)), NULL, 0, &success);
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
		if (i == 0)
			text = "a=b=c=d";
		else if (i == 1)
			text = "a=(b)=(c)=(d)";

		textStr = strCharAppendData(NULL, text, strlen(text));
		int err;
		lexerUpdate(lex, (struct __vec *)textStr, &err);
		assert(!err);

		struct parserNode *node = parseExpression(
		    llLexerItemFirst(lexerGetItems(lex)), NULL, 0, &success);
		assert(success);
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
	strCharDestroy(&textStr);
	text = "a,b,c";
	textStr = strCharAppendData(NULL, text, strlen(text));
	int err;
	lexerUpdate(lex, (struct __vec *)textStr, &err);
	assert(!err);

	node =
	    parseExpression(llLexerItemFirst(lexerGetItems(lex)), NULL, 1, &success);
	assert(success);
	{
		assert(node->type == NODE_COMMA_SEQ);
		struct parserNodeCommaSequence *seq = (void *)node;
		assert(strParserNodeSize(seq->commas) == 2);
		assert(strParserNodeSize(seq->nodes) == 3);

		const char *names[] = {"a", "b", "c"};
		for (long i = 0; i != 3; i++) {
			assert(seq->nodes[i]->type == NODE_NAME_TOKEN);
			struct parserNodeNameToken *name = (void *)seq->nodes[i];
			assert(0 == strcmp(name->text, names[i]));
		}
	}

	strCharDestroy(&textStr);
	text = "c=!(a+1*3)+ ++b";
	textStr = strCharAppendData(NULL, text, strlen(text));
	err = 0;
	lexerUpdate(lex, (struct __vec *)textStr, &err);
	assert(!err);
	node =
	    parseExpression(llLexerItemFirst(lexerGetItems(lex)), NULL, 1, &success);
	assert(success);
	{
		// c =
		assert(node->type == NODE_EXPR_BINOP);
		struct parserNodeBinop *binop = (void *)node;
		struct parserNodeOpTerm *op = (void *)binop->op;
		assert(op->base.type == NODE_OP_TERM);
		assert(0 == strcmp(op->text, "="));
		struct parserNodeNameToken *name = (void *)binop->a;
		assert(name->base.type == NODE_NAME_TOKEN);
		assert(0 == strcmp(name->text, "c"));

		//+
		binop = (void *)binop->b;
		assert(binop->base.type == NODE_EXPR_BINOP);
		op = (void *)binop->op;
		assert(op->base.type == NODE_OP_TERM);
		assert(0 == strcmp(op->text, "+"));
		__auto_type plus1 = binop;

		//++
		struct parserNodeUnop *unop1 = (void *)plus1->b;
		assert(unop1->base.type == NODE_EXPR_UNOP);
		name = (void *)unop1->a;
		assert(name->base.type == NODE_NAME_TOKEN);
		assert(0 == strcmp(name->text, "b"));
		op = (void *)unop1->op;
		assert(op->base.type == NODE_OP_TERM);
		assert(0 == strcmp(op->text, "++"));

		//!
		struct parserNodeUnop *unop2 = (void *)plus1->a;
		assert(unop2->base.type == NODE_EXPR_UNOP);
		op = (void *)unop2->op;
		assert(op->base.type == NODE_OP_TERM);
		assert(0 == strcmp(op->text, "!"));

		struct parserNodeBinop *plus2 = (void *)unop2->a;
		name = (void *)plus2->a;
		assert(name->base.type == NODE_NAME_TOKEN);
		assert(0 == strcmp(name->text, "a"));
		op = (void *)plus2->op;
		assert(op->base.type == NODE_OP_TERM);
		assert(0 == strcmp(op->text, "+"));

		struct parserNodeBinop *times = (void *)plus2->b;
		struct parserNodeIntLiteral *intLit = (void *)times->a;
		assert(intLit->base.type == NODE_LIT_INT);
		assert(intLit->value.value.sInt == 1);
		intLit = (void *)times->b;
		assert(intLit->base.type == NODE_LIT_INT);
		assert(intLit->value.value.sInt == 3);
	}
}
