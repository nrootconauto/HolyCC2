#pragma once
#include <holyCParser.h>
struct scope {
 mapVar vars;
 llScope subScopes;
 llScope parent;
};
void variableDestroy(struct variable *var);
void scopeDestroy(void *s);
void enterScope();
void leaveScope();
void addVar(const struct parserNode *name, struct object *type) ;
