#include <cacheingLexerItems.h>
#include <cacheingLexer.h>
#pragma once
enum parserNodeType {
	NODE_OP_TERM,
	NODE_LIT_INT,
	NODE_LIT_CHAR,
	NODE_LIT_STRING,
	NODE_LIT_FLOATING,
	NODE_EXPR_UNOP,
	NODE_EXPR_BINOP,
};
struct parserNode {
	enum parserNodeType type;
};
struct nodePosition {
	long start, end;
};
struct parserNodeIntLiteral {
	struct parserNode base;
	struct nodePosition pos;
	struct lexerInt value;
};
struct parserNodeStringLiteral {
	struct parserNode base;
	struct nodePosition pos;
	struct parsedString value;
};
struct parserNodeFloatingLiteral {
	struct parserNode base;
	struct nodePosition pos;
	struct lexerFloating value;
};

struct parserNodeOpTerm {
	struct parserNode base;
	struct nodePosition pos;
	const char *text;
};
struct parserNodeBinop {
	struct parserNode base;
	const struct parserNode *a;
	const struct parserNode *b;
	const struct parserNode *op;
};
struct parserNodeUnop {
	struct parserNode base;
	const struct parserNode *a;
	const struct parserNode *op;
};
struct grammar *holyCGrammarCreate();
const strLexerItemTemplate holyCLexerTemplates();
