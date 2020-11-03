#pragma once
#include <str.h>
#include <lexer.h>
#include <holyCType.h>
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
 NODE_KW,
 NODE_VAR_DECL,
 NODE_VAR_DECLS,
 NODE_META_DATA,
 NODE_CLASS_DEF,
 NODE_UNION_DEF,
 NODE_IF,
 NODE_SCOPE,
 NODE_DO,
 NODE_WHILE,
 NODE_FOR,
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
struct parserNodeKeyword {
 struct parserNode base;
 struct sourcePos pos;
 const char *text;
};
struct parserNodeMetaData {
 struct parserNode base;
 struct parserNode *name;
 struct parserNode *value;
};
struct parserNodeVarDecl {
 struct parserNode base;
 struct parserNode *name;
 struct object *type;
 struct parserNode *dftVal;
 strParserNode metaData;
};
struct parserNodeVarDecls {
 struct parserNode base;
 strParserNode decls;
};
struct parserNodeClassDef {
 struct parserNode base;
 struct parserNode *name;
 struct object *type;
};
struct parserNodeUnionDef {
 struct parserNode base;
 struct parserNode *name;
 struct object *type;
};
struct parserNodeIf {
 struct parserNode base;
 struct parserNode *cond;
 struct parserNode *body;
 /**
	* else if's are if statements present in el
	* if(cond) body else [if-statement]
	*/
 struct parserNode *el;
};
struct parserNodeScope {
 struct parserNode base;
 strParserNode smts;
};
struct parserNodeDo {
 struct parserNode base;
 struct parserNode *cond,*body;
};
struct parserNodeFor {
 struct parserNode base;
 struct parserNode *init,*cond,*inc,*body;
};
struct parserNodeWhile {
 struct parserNode base;
 struct parserNode *cond,*body;
}
struct parserNode *parseExpression(llLexerItem start,llLexerItem end,llLexerItem *result);
void parserNodeDestroy(struct parserNode **node);
struct parserNode *parseVarDecls(llLexerItem start, llLexerItem *end);
struct parserNode *parseClass(llLexerItem start, llLexerItem *end);
struct parserNode *parseIf(llLexerItem start, llLexerItem *end);
struct parserNode *parseStatement(llLexerItem start, llLexerItem *end);
