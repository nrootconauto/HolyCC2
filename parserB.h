#pragma once
#include <hashTable.h>
#include <linkedList.h>
#include <parserA.h>
struct scope;
MAP_TYPE_DEF(struct variable, Var);
MAP_TYPE_FUNCS(struct variable, Var);
MAP_TYPE_DEF(struct function, Func);
MAP_TYPE_FUNCS(struct function, Func);
LL_TYPE_DEF(struct scope, Scope);
struct scope {
	mapVar vars;
	mapFunc funcs;
	llScope subScopes;
	llScope parent;
};
LL_TYPE_FUNCS(struct scope, Scope);
void variableDestroy(struct variable *var);
void enterScope();
void leaveScope();
void addVar(const struct parserNode *name, struct object *type);
struct variable *getVar(const struct parserNode *name);
struct function *getFunc(const struct parserNode *name);
void addFunc(const struct parserNode *name, const struct object *type,
             struct parserNode *func);

void initParserData();
void killParserData();
struct parserNode *getGlobalSymbol(const char *name);
void addGlobalSymbol(struct parserNode *node,struct  linkage link);
strParserNode parserSymbolTableSyms();
const struct linkage *getGlobalSymbolLink(const char *name);
void addSymbolByDiffName(struct parserNode *sym,const char *useName,struct  linkage link);
