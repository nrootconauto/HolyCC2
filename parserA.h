#pragma once
#include "lexer.h"
struct object;
#include "str.h"
enum parserNodeType {
	NODE_ASM_REG,
	NODE_ASM_ADDRMODE,
	NODE_ASM_LABEL,
	NODE_ASM_LABEL_GLBL,
	NODE_ASM_LABEL_LOCAL,
	NODE_ASM_IMPORT,
	NODE_ASM_DU8,
	NODE_ASM_DU16,
	NODE_ASM_DU32,
	NODE_ASM_DU64,
	NODE_ASM_USE16,
	NODE_ASM_USE32,
	NODE_ASM_USE64,
	NODE_ASM_ORG,
	NODE_ASM_ALIGN,
	NODE_ASM_BINFILE,
	NODE_ASM,
	NODE_BINOP,
	NODE_LIT_FLT,
	NODE_ASM_INST,
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
	NODE_CLASS_FORWARD_DECL,
	NODE_UNION_DEF,
	NODE_UNION_FORWARD_DECL,
	NODE_IF,
	NODE_SCOPE,
	NODE_BREAK,
	NODE_DO,
	NODE_WHILE,
	NODE_FOR,
	NODE_TRY,
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
	NODE_SIZEOF_TYPE,
	NODE_SIZEOF_EXP,
	NODE_RANGES,
	/**
	 * extern,import,_extern,_import,public
	 */
	NODE_LINKAGE,
	NODE_ARRAY_LITERAL,
};
struct linkage {
	enum {
		LINKAGE_LOCAL = 0,
		LINKAGE_STATIC = 1,
		LINKAGE_PUBLIC = 2,
		LINKAGE_EXTERN = 4,
		LINKAGE__EXTERN = 8,
		LINKAGE_IMPORT = 16,
		LINKAGE__IMPORT = 32,
	} type;
	char *fromSymbol;
};
struct linkage linkageClone(struct linkage from);
STR_TYPE_DEF(struct parserNode *, ParserNode);
STR_TYPE_FUNCS(struct parserNode *, ParserNode);
struct parserNode;
struct parserVar *parserVariableClone(struct parserVar *var);
struct parserVar {
		char *name;
		struct object *type;
		strParserNode refs;
		struct reg *inReg;
		long refCount;
		unsigned int isGlobal : 1;
		unsigned int isNoreg : 1;
		unsigned int isRefedByPtr:1;
		unsigned int isTmp:1;
};
struct parserFunction {
	char *name;
	struct object *type;
	strParserNode refs;
	int isForwardDecl;
	struct parserNode *node;
	struct parserNode *parentFunction;
};
void variableDestroy(struct parserVar *var);
struct parserSourcePos {
	long start;
	long end;
};
struct parserNode {
	enum parserNodeType type;
	struct parserSourcePos pos;
		long refCount;
};
struct parserNodeSizeofType {
		struct parserNode base;
		struct object *type;
};
struct parserNodeSizeofExp {
		struct parserNode base;
		struct parserNode *exp;
};
struct parserNodeGoto {
	struct parserNode base;
	struct parserNode *labelName;
	struct parserNode *pointsTo;
};
struct parserNodeUnionFwd {
	struct parserNode base;
	struct parserNode *name;
	struct object *type;
};
struct parserNodeClassFwd {
	struct parserNode base;
	struct parserNode *name;
	struct object *type;
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
struct parserNodeLitFlt {
	struct parserNode base;
	double value;
};
struct parserNodeLinkage {
	struct parserNode base;
	struct linkage link;
};
struct parserNodeLitStr {
	struct parserNode base;
		struct parsedString str;
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
		struct reg *inReg;
		unsigned int isNoReg:1;
		struct parserNode *var;
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
	struct parserVar *var;
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
struct parserNodeLabelGlbl {
	struct parserNode base;
	struct parserNode *name;
};
struct parserNodeLabelLocal {
	struct parserNode base;
	struct parserNode *name;
};
struct parserNodeLabel {
	struct parserNode base;
	struct parserNode *scope;
	struct parserNode *name;
};
struct parserNodeSubSwitch {
	struct parserNode base;
	struct parserNode *parent;
	struct parserNode *start;
	strParserNode body;
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
		strParserNode args;
	struct object *funcType;
	struct parserNode *name;
	struct parserFunction *func;
	struct parserNode *bodyScope;
		llLexerItem __cacheStartToken;
		llLexerItem __cacheEndToken;
};
struct parserNodeFuncRef {
	struct parserNode base;
	struct parserFunction *func;
	struct parserNode *name;
};
struct parserNodeMemberAccess {
	struct parserNode base;
	struct parserNode *exp;
		struct parserNode *op;
	struct parserNode *name;
};
struct parserNodeFuncForwardDec {
	struct parserNode base;
	struct parserNode *name;
	struct object *funcType;
	struct parserFunction *func;
};
struct parserNodeBreak {
	struct parserNode base;
};
struct parserNodeAsmReg {
	struct parserNode base;
	struct reg *reg;
};
struct parserNodeAsmAddrMode {
	struct parserNode base;
		struct X86AddressingMode *mode;
};
struct parserNodeUse16 {
	struct parserNode base;
};
struct parserNodeUse32 {
	struct parserNode base;
};
struct parserNodeAsmBinfile {
	struct parserNode base;
	struct parserNode *fn;
};
struct parserNodeAsmOrg {
	struct parserNode base;
	uint64_t org;
};
struct parserSymbol;
STR_TYPE_DEF(struct parserSymbol*,ParserSymbol);
struct parserNodeAsmImport {
	struct parserNode base;
};
/* USE16/32/64
 */
struct parserNodeAsmAlign {
	struct parserNode base;
	uint64_t count;
	int fill;
};
struct parserNodeAsmUseXX {
	struct parserNode base;
};
/* DU8/16/32/64
 */
struct parserNodeDUX {
	struct parserNode base;
	struct __vec *bytes;
};
struct parserNodeAsm {
	struct parserNode base;
	strParserNode body;
};
struct parserNodeAsmInstX86 {
	struct parserNode base;
	struct parserNode *name;
	strParserNode args;
};
struct parserNodeArrayLit {
		struct parserNode base;
		strParserNode items;
};
struct parserNodeTry {
		struct parserNode base;
		struct parserNode *body;
		struct parserNode *catch;
};
struct parserNodeRanges {
		struct parserNode base;
		strParserNode exprs;
		strParserNode ops;
};
struct parserNode *parseTry(llLexerItem start, llLexerItem *end);
struct parserNode *parseExpression(llLexerItem start, llLexerItem end, llLexerItem *result);
struct parserNode *parseVarDecls(llLexerItem start, llLexerItem *end);
struct parserNode *parseClass(llLexerItem start, llLexerItem *end, int allowForwardDecl);
struct parserNode *parseArrayLiteral(llLexerItem start, llLexerItem *end);
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
struct parserNode *parseBreak(llLexerItem item, llLexerItem *end);
struct parserNode *parseAsmRegister(llLexerItem start, llLexerItem *end);
void parserNodeDestroy(struct parserNode **node);
struct parserNode *parseAsmInstructionX86(llLexerItem start, llLexerItem *end);
void __initParserA();
struct X86AddressingMode *parserNode2X86AddrMode(struct parserNode *node);
void parserMapGotosToLabels();
struct parserNode *parseAsm(llLexerItem start, llLexerItem *end);
