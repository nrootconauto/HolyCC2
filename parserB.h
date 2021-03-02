#pragma once
#include <hashTable.h>
#include <linkedList.h>
#include <parserA.h>
struct scope;
MAP_TYPE_DEF(struct variable, Var);
MAP_TYPE_FUNCS(struct parserVar, Var);
MAP_TYPE_DEF(struct parserFunction*, Func);
MAP_TYPE_FUNCS(struct parserFunction*, Func);
LL_TYPE_DEF(struct scope, Scope);
struct scope {
	mapVar vars;
	mapFunc funcs;
	llScope subScopes;
	llScope parent;
};
LL_TYPE_FUNCS(struct scope, Scope);
void variableDestroy(struct parserVar *var);
void enterScope();
void leaveScope();
void parserAddVar(const struct parserNode *name, struct object *type);
struct parserVar *parserGetVar(const struct parserNode *name);
struct parserFunction *parserGetFunc(const struct parserNode *name);
void parserAddFunc(const struct parserNode *name, const struct object *type, struct parserNode *func);

void initParserData();
void killParserData();
struct parserNode *parserGetGlobalSym(const char *name);
void parserAddGlobalSym(struct parserNode *node, struct linkage link);
strParserNode parserSymbolTableSyms();
const struct linkage *parserGlobalSymLinkage(const char *name);
void parserSymTableNames(const char **keys, long *count);
void parserAddVarLenArgsVars2Func(struct parserVar **Argc,struct parserVar **Argv);
struct parserVar *parserGetVarByText(const char *name);
const char  *parserGetGlobalSymLinkageName(const char *name);
