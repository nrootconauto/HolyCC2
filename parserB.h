#pragma once
#include <parserA.h>
#include <hashTable.h>
#include <linkedList.h>
struct scope;
MAP_TYPE_DEF(struct variable, Var);
MAP_TYPE_FUNCS(struct variable, Var);
LL_TYPE_DEF(struct scope, Scope);
struct scope {
 mapVar vars;
 llScope subScopes;
 llScope parent;
};
LL_TYPE_FUNCS(struct scope, Scope);

void variableDestroy(struct variable *var);
void scopeDestroy(struct scope *s);
void enterScope();
void leaveScope();
void addVar(const struct parserNode *name, struct object *type) ;
struct variable *getVar(const struct parserNode *name);
