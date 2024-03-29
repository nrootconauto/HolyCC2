#pragma once
#include "hashTable.h"
#include "linkedList.h"
#include "parserA.h"
struct scope;
MAP_TYPE_DEF(struct parserVar *, Var);
MAP_TYPE_FUNCS(struct parserVar *, Var);
MAP_TYPE_DEF(struct parserFunction*, Func);
MAP_TYPE_FUNCS(struct parserFunction*, Func);
LL_TYPE_DEF(struct scope, Scope);
struct scope {
	mapVar vars;
	llScope subScopes;
	llScope parent;
};
struct parserSymbol {
		char *fn;
		long ln,col;
		struct object *type;
		char *name;
		struct linkage link;
		long version;
		double shadowPrec;
		struct parserVar *var;
		struct parserFunction *function;
};
STR_TYPE_DEF(struct parserSymbol *,ParserSymbol);
STR_TYPE_FUNCS(struct parserSymbol *,ParserSymbol);
LL_TYPE_FUNCS(struct scope, Scope);
void variableDestroy(struct parserVar *var);
void enterScope();
void leaveScope();
void parserAddVar(const struct parserNode *name, struct object *type,struct reg *inReg,int isNoReg,struct linkage *glblLinkage);
struct parserVar *parserGetVar(const struct parserNode *name);
struct parserFunction *parserGetFunc(const struct parserNode *name);
void parserAddFunc(const struct parserNode *name, const struct object *type, struct parserNode *func,llLexerItem start,llLexerItem end,struct linkage link);
void initParserData();
void killParserData();
struct parserSymbol *parserGetGlobalSym(const char *name);
void parserAddGlobalSym(struct parserNode *node, struct linkage link);
strParserSymbol parserSymbolTableSyms();
const struct linkage *parserGlobalSymLinkage(const char *name);
void parserSymTableNames(const char **keys, long *count);
void parserAddVarLenArgsVars2Func(struct parserVar **Argc,struct parserVar **Argv);
struct parserVar *parserGetVarByText(const char *name);
const char  *parserGetGlobalSymLinkageName(const char *name);
struct object *parserGlobalSymType(const char *name);
struct parserFunction *parserGetFuncByName(const char *name);
void parserMoveGlobals2Extern();
int isGlobalScope();
