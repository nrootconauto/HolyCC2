#pragma once
#include <lexer.h>
#include <object.h>
#include <str.h>
enum parserNodeType {
	NODE_BINOP,
	NODE_UNOP,
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
	NODE_VAR,
	NODE_CASE,
	NODE_DEFAULT,
	NODE_SWITCH,
	NODE_SUBSWITCH,
	NODE_LABEL,
	NODE_TYPE_CAST,
	NODE_ARRAY_ACCESS,
	NODE_FUNC_DEF,
	NODE_FUNC_FORWARD_DECL,
	NODE_FUNC_REF,
	NODE_MEMBER_ACCESS,
	NODE_RETURN,
	NODE_GOTO,
};
STR_TYPE_DEF(struct parserNode *, ParserNode);
STR_TYPE_FUNCS(struct parserNode *, ParserNode);
struct parserNode;
struct variable {
	char *name;
	struct object *type;
	strParserNode refs;
	int isGlobal;
};
struct function {
	char *name;
	struct object *type;
	strParserNode refs;
	int isForwardDecl;
	struct parserNode *node;
};
void variableDestroy(struct variable *var);
struct sourcePos {
	long start;
	long end;
};
struct parserNode {
	enum parserNodeType type;
	struct sourcePos pos;
};
struct parserNodeGoto {
	struct parserNode base;
	struct parserNode *labelName;
};
struct parserNodeReturn {
	struct parserNode base;
	struct parserNode *value;
};
struct parserNodeOpTerm {
	struct parserNode base;
	const char *text;
};
struct parserNodeUnop {
	struct parserNode base;
	struct parserNode *a;
	struct parserNode *op;
	long isSuffix;
	struct object *type;
};
struct parserNodeBinop {
	struct parserNode base;
	struct parserNode *a;
	struct parserNode *op;
	struct parserNode *b;
	struct object *type;
};
struct parserNodeName {
	struct parserNode base;
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
	struct object *type;
};
struct parserNodeCommaSeq {
	struct parserNode base;
	strParserNode items;
	struct object *type;
};
struct parserNodeKeyword {
	struct parserNode base;
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
	strParserNode stmts;
};
struct parserNodeDo {
	struct parserNode base;
	struct parserNode *cond, *body;
};
struct parserNodeFor {
	struct parserNode base;
	struct parserNode *init, *cond, *inc, *body;
};
struct parserNodeWhile {
	struct parserNode base;
	struct parserNode *cond, *body;
};
struct parserNodeVar {
	struct parserNode base;
	struct variable *var;
};
struct parserNodeSwitch {
	struct parserNode base;
	strParserNode caseSubcases;
	struct parserNode *dft;
	struct parserNode *body;
	struct parserNode *exp;
};
struct parserNodeCase {
	struct parserNode base;
	struct parserNode *label;
	struct parserNode *parent;
	long valueLower;
	long valueUpper;
};
/**
 * start:/end:
 */
struct parserNodeLabel {
	struct parserNode base;
		struct parserNode *scope;
	struct parserNode *name;
};
struct parserNodeSubSwitch {
	struct parserNode base;
	struct parserNode *parent;
	struct parserNode *start;
	struct parserNode *end;
		struct parserNode *__startCodeScope;
		strParserNode startCodeStatements;
	strParserNode caseSubcases;
	struct parserNode *dft;
};
struct parserNodeDefault {
	struct parserNode base;
	struct parserNode *parent;
};
struct parserNodeArrayAccess {
	struct parserNode base;
	struct parserNode *exp;
	struct parserNode *index;
	struct object *type;
};
struct parserNodeTypeCast {
	struct parserNode base;
	struct parserNode *exp;
	struct object *type;
};
struct parserNodeFuncDef {
	struct parserNode base;
	struct object *funcType;
	struct parserNode *name;
	struct parserNode *bodyScope;
};
struct parserNodeFuncRef {
	struct parserNode base;
	struct function *func;
	struct parserNode *name;
};
struct parserNodeMemberAccess {
	struct parserNode base;
	struct parserNode *exp;
	struct parserNode *name;
};
struct parserNodeFuncForwardDec {
	struct parserNode base;
	struct parserNode *name;
	struct object *funcType;
};
struct parserNode *parseExpression(llLexerItem start, llLexerItem end,
                                   llLexerItem *result);
struct parserNode *parseVarDecls(llLexerItem start, llLexerItem *end);
struct parserNode *parseClass(llLexerItem start, llLexerItem *end);
struct parserNode *parseIf(llLexerItem start, llLexerItem *end);
struct parserNode *parseStatement(llLexerItem start, llLexerItem *end);
struct parserNode *parseFor(llLexerItem start, llLexerItem *end);
struct parserNode *parseWhile(llLexerItem start, llLexerItem *end);
struct parserNode *parseDo(llLexerItem start, llLexerItem *end);
struct parserNode *parseSwitch(llLexerItem start, llLexerItem *end);
struct parserNode *parseCase(llLexerItem start, llLexerItem *end);
struct parserNode *parseLabel(llLexerItem start, llLexerItem *end);
struct parserNode *parseFunction(llLexerItem start, llLexerItem *end);
struct parserNode *parseReturn(llLexerItem start, llLexerItem *end);
struct parserNode *parseGoto(llLexerItem start, llLexerItem *end);
