#pragma once
#include <str.h>
#include <cacheingLexerItems.h>
enum parserNodeType {
 NODE_BINOP,
 NODE_UNOP,
 NODE_INT,
 NODE_STR,
 NODE_NAME,
 NODE_OP,
 NODE_FUNC_CALL,
 NODE_COMMA_SEQ,
 NODE_LIT_INT,
 NODE_LIT_STR,
};
STR_TYPE_DEF(struct parserNode *,ParserNode);
STR_TYPE_FUNCS(struct parserNode *,ParserNode);
struct parserNode {
 enum parserNodeType type;
};
struct sourcePos {
 long start;
 long end;
};
struct parserNodeOpTerm {
 struct parserNode base;
 struct sourcePos pos;
 const char *text;
};
struct parserNodeUnop {
 struct parserNode base;
 struct parserNode *a;
 struct parserNode *op;
 long isSuffix;
};
struct parserNodeBinop {
 struct parserNode base;
 struct parserNode *a;
 struct parserNode *op;
 struct parserNode *b;
};
struct parserNodeName {
 struct parserNode base;
 struct sourcePos pos;
 char *text;
};
struct parserNodeLitInt {
 struct parserNode base;
 struct lexerInt value;
};
struct parserNodeLitStr {
 struct parserNode base;
 char *text;
 int isChar;
};
struct parserNodeFuncCall {
 struct parserNode base;
 struct parserNode *func;
 strParserNode args;
};
struct parserNodeCommaSeq {
 struct parserNode base;
 strParserNode items;
};
struct parserNode *parseExpression(llLexerItem start,llLexerItem end,llLexerItem *result);
strLexerItemTemplate holyCLexerTemplates();
