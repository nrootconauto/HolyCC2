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
	NODE_NAME_TOKEN,
	NODE_COMMA_SEQ,
	NODE_ARRAY_DIM,
	NODE_REPEAT,
};
struct parserNode;
STR_TYPE_DEF(struct parserNode*,ParserNode);
STR_TYPE_FUNCS(struct parserNode*,ParserNode);
struct parserNode {
	enum parserNodeType type;
};
struct parserNodeRepeat {
 struct parserNode base;
 strParserNode nodes;
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
	struct parserNode *a;
	struct parserNode *b;
	struct parserNode *op;
};
struct parserNodeUnop {
	struct parserNode base;
	struct parserNode *a;
	struct parserNode *op;
};
struct parserNodeNameToken {
 struct parserNode base;
 struct nodePosition pos;
 const char *text;
};
struct parserNodeArrayDim {
 struct parserNode base;
 struct parserNode *dim;
};
struct parserNodeCommaSequence {
 struct parserNode base;
 strParserNode nodes;
 strParserNode commas;
};
enum holyCTypeModifier {
 TYPE_MOD_PTR,
 TYPE_MOD_VOLATILE,
 TYPE_MOD_ARRAY,
};
struct grammar *holyCGrammarCreate();
const strLexerItemTemplate holyCLexerTemplates();
