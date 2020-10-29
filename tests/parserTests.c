#include <assert.h>
#include <cacheingLexer.h>
#include <cacheingLexerItems.h>
#include <holyCParser.h>
STR_TYPE_DEF(char, Char);
STR_TYPE_FUNCS(char, Char);
static int charCmp(const void *a, const void *b) {
	return *(char *)a == *(char *)b;
}
void precParserTests() {
	const char *text = "0 + 1 + 2 + 3";
	__auto_type textStr = strCharAppendData(NULL, text, strlen(text));

	__auto_type lex = lexerCreate((struct __vec *)textStr, holyCLexerTemplates(),
	                              charCmp, skipWhitespace);

	int success;
	struct parserNode *node =
	    parseExpression(llLexerItemFirst(lexerGetItems(lex)), NULL, NULL);
	assert(node);
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
		assert(node->type == NODE_BINOP);
		struct parserNodeBinop *binop = (void *)node;

		assert(binop->b->type == NODE_LIT_INT);
		struct parserNodeLitInt *intLit = (void *)binop->b;
		assert(intLit->value.value.sInt == 3);

		assert(binop->a->type == NODE_BINOP);
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

		struct parserNode *node =
		    parseExpression(llLexerItemFirst(lexerGetItems(lex)), NULL, NULL);
		assert(node);
		{
			assert(node->type == NODE_BINOP);
			struct parserNodeBinop *binop = (void *)node;

			assert(binop->a->type == NODE_NAME);
			struct parserNodeName *token = (void *)binop->a;
			assert(0 == strcmp(token->text, "a"));

			assert(binop->b->type == NODE_BINOP);
			binop = (void *)binop->b;

			assert(binop->a->type == NODE_NAME);
			token = (void *)binop->a;
			assert(0 == strcmp(token->text, "b"));

			assert(binop->b->type == NODE_BINOP);
			binop = (void *)binop->b;

			assert(binop->a->type == NODE_NAME);
			token = (void *)binop->a;
			assert(0 == strcmp(token->text, "c"));
			assert(binop->b->type == NODE_NAME);
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

	node = parseExpression(llLexerItemFirst(lexerGetItems(lex)), NULL, NULL);
	assert(node);
	{
		assert(node->type == NODE_COMMA_SEQ);
		struct parserNodeCommaSeq *seq = (void *)node;
		assert(strParserNodeSize(seq->items) == 3);

		const char *names[] = {"a", "b", "c"};
		for (long i = 0; i != 3; i++) {
			assert(seq->items[i]->type == NODE_NAME);
			struct parserNodeName *name = (void *)seq->items[i];
			assert(0 == strcmp(name->text, names[i]));
		}
	}
	strCharDestroy(&textStr);
	text = "*++a,a++.b.c++";
	textStr = strCharAppendData(NULL, text, strlen(text));
	err = 0;
	lexerUpdate(lex, (struct __vec *)textStr, &err);
	assert(!err);

	node = parseExpression(llLexerItemFirst(lexerGetItems(lex)), NULL, NULL);
	assert(node);
	{
		assert(node->type = NODE_COMMA_SEQ);
		struct parserNodeCommaSeq *seq = (void *)node;
		assert(strParserNodeSize(seq->items) == 2);

		struct parserNodeUnop *unop = (void *)seq->items[0];
		struct parserNodeOpTerm *op = (void *)unop->op;
		assert(op->base.type = NODE_OP);
		assert(0 == strcmp(op->text, "*"));
		assert(unop->isSuffix == 0);

		unop = (void *)unop->a;
		op = (void *)unop->op;
		assert(op->base.type = NODE_OP);
		assert(0 == strcmp(op->text, "++"));
		assert(unop->isSuffix == 0);

		struct parserNodeName *name = (void *)unop->a;
		assert(name->base.type = NODE_NAME);
		assert(0 == strcmp(name->text, "a"));

		unop = (void *)seq->items[1];
		op = (void *)unop->op;
		assert(op->base.type = NODE_OP);
		assert(0 == strcmp(op->text, "++"));
		assert(unop->isSuffix == 1);

		struct parserNodeBinop *binop = (void *)unop->a;
		name = (void *)binop->b;
		assert(name->base.type = NODE_NAME);
		assert(0 == strcmp(name->text, "c"));

		binop = (void *)binop->a;
		name = (void *)binop->b;
		assert(name->base.type = NODE_NAME);
		assert(0 == strcmp(name->text, "b"));

		unop = (void *)binop->a;
		op = (void *)unop->op;
		assert(op->base.type = NODE_OP);
		assert(0 == strcmp(op->text, "++"));
		assert(unop->isSuffix == 1);

		name = (void *)unop->a;
		assert(name->base.type = NODE_NAME);
		assert(0 == strcmp(name->text, "a"));
	}

	text = "c=!(a+1*3)+ ++b";
	textStr = strCharAppendData(NULL, text, strlen(text));
	err = 0;
	lexerUpdate(lex, (struct __vec *)textStr, &err);
	assert(!err);
	node = parseExpression(llLexerItemFirst(lexerGetItems(lex)), NULL, NULL);
	assert(node);
	{
		// c =
		assert(node->type == NODE_BINOP);
		struct parserNodeBinop *binop = (void *)node;
		struct parserNodeOpTerm *op = (void *)binop->op;
		assert(op->base.type == NODE_OP);
		assert(0 == strcmp(op->text, "="));
		struct parserNodeName *name = (void *)binop->a;
		assert(name->base.type == NODE_NAME);
		assert(0 == strcmp(name->text, "c"));

		//+
		binop = (void *)binop->b;
		assert(binop->base.type == NODE_BINOP);
		op = (void *)binop->op;
		assert(op->base.type == NODE_OP);
		assert(0 == strcmp(op->text, "+"));
		__auto_type plus1 = binop;

		//++
		struct parserNodeUnop *unop1 = (void *)plus1->b;
		assert(unop1->base.type == NODE_UNOP);
		name = (void *)unop1->a;
		assert(name->base.type == NODE_NAME);
		assert(0 == strcmp(name->text, "b"));
		op = (void *)unop1->op;
		assert(op->base.type == NODE_OP);
		assert(0 == strcmp(op->text, "++"));

		//!
		struct parserNodeUnop *unop2 = (void *)plus1->a;
		assert(unop2->base.type == NODE_UNOP);
		op = (void *)unop2->op;
		assert(op->base.type == NODE_OP);
		assert(0 == strcmp(op->text, "!"));

		struct parserNodeBinop *plus2 = (void *)unop2->a;
		name = (void *)plus2->a;
		assert(name->base.type == NODE_NAME);
		assert(0 == strcmp(name->text, "a"));
		op = (void *)plus2->op;
		assert(op->base.type == NODE_OP);
		assert(0 == strcmp(op->text, "+"));

		struct parserNodeBinop *times = (void *)plus2->b;
		struct parserNodeLitInt *intLit = (void *)times->a;
		assert(intLit->base.type == NODE_LIT_INT);
		assert(intLit->value.value.sInt == 1);
		intLit = (void *)times->b;
		assert(intLit->base.type == NODE_LIT_INT);
		assert(intLit->value.value.sInt == 3);
	}
}
/*


strCharDestroy(&textStr);

strCharDestroy(&textStr);
text = "a(b(),,c)";
textStr = strCharAppendData(NULL, text, strlen(text));
err = 0;
lexerUpdate(lex, (struct __vec *)textStr, &err);
assert(!err);
node =
    parseExpression(llLexerItemFirst(lexerGetItems(lex)), NULL, 1, &success);
assert(success);
{
  assert(node->type == NODE_FUNC_CALL);
  struct parserNodeFunctionCall *call = (void *)node;
  struct parserNodeNameToken *name = (void *)call->func;
  assert(name->base.type == NODE_NAME_TOKEN);
  assert(0 == strcmp(name->text, "a"));

  assert(strParserNodeSize(call->args) == 3);
  struct parserNodeFunctionCall *call2 = (void *)call->args[0];
  assert(call2->base.type == NODE_FUNC_CALL);
  name = (void *)call2->func;
  assert(name->base.type == NODE_NAME_TOKEN);
  assert(0 == strcmp(name->text, "b"));
  assert(strParserNodeSize(call2->args) == 0);

  assert(call->args[1] == NULL);

  name = (void *)call->args[2];
  assert(name->base.type == NODE_NAME_TOKEN);
  assert(0 == strcmp(name->text, "c"));
}
}
void typeParserTests() {
const char *text = "I64i x=1";
__auto_type textStr = strCharAppendData(NULL, text, strlen(text));
__auto_type lex = lexerCreate((struct __vec *)textStr, holyCLexerTemplates(),
                              charCmp, skipWhitespace);
__auto_type items = lexerGetItems(lex);
__auto_type str =
    parserLexerItems2Str(llLexerItemFirst(items), llLexerItemLast(items));
char *name;
long count;
__auto_type node =
    parseVarDecls(str, str + strParserNodeSize(str), &name, &count);
assert(node != NULL);
assert(count == 4);
assert(node->type == NODE_VAR_DECL);
struct parserNodeVarDecl *decl = (void *)node;
assert(0 == strcmp(decl->name, "x"));
assert(decl->type == &typeI64i);
assert(decl->dftVal->type == NODE_LIT_INT);

return;
strCharDestroy(&textStr);
text = "I64i (*x[])(I64i x)";
textStr = strCharAppendData(NULL, text, strlen(text));
lexerUpdate(lex, ^textStr);
}
*/
