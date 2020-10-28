#include <cacheingLexerItems.h>
#include <cacheingLexer.h>
#include <holyCType.h>
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
	NODE_FUNC_CALL,
	NODE_TYPENAME,
	NODE_TYPECAST,
	NODE_ARRAY_ACCESS,
	NODE_VAR_DECL,
	NODE_VAR_DECLS,
	NODE_FUNC_TYPE_ARGS,
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
 char *text;
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
struct parserNodeFunctionCall {
 struct parserNode base;
 struct parserNode *func;
 strParserNode args;
};
struct parserNodeTypeCast {
 struct parserNode base;
 struct parserNode *exp;
 struct parserNode *toType;
};
struct parserNodeArrayAccess {
 struct parserNode base;
 struct parserNode *exp;
 struct parserNode *index;
};
struct parserNodeObject {
 struct parserNode base;
 struct object *type;
};
struct parserNodeVarDecl {
 struct parserNode base;
 struct nodePosition pos;
 char *name;
 struct object *type;
 struct parserNode *dftVal;
};
STR_TYPE_DEF(struct parserNodeVarDecl *,VarDecl);
STR_TYPE_FUNCS(struct parserNodeVarDecl *,VarDecl);
struct parserNodeVarDecls {
 struct parserNode base;
 strVarDecl decls;
};
struct parserNodeFuncArgs {
 struct parserNode base;
 strVarDecl decls;
};
const strLexerItemTemplate holyCLexerTemplates();
struct parserNode * parserNodeUnopCreate(struct parserNode *exp,struct parserNode *op);
struct parserNode *parserNodeCommaSequenceAppend(struct parserNode *start,
                                                 struct parserNode *comma,
                                                 struct parserNode *next);
struct parserNode *parseExpression(llLexerItem start, llLexerItem end,
                                   int includeCommas, int *success);
struct parserNode *parseTypename(struct parserNode **start,
                                 struct parserNode **end, long *count);
struct parserNode *parseVarDecls(struct parserNode **start,
                                   struct parserNode **end, char **itemName,
                                   long *count);
