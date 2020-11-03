#include <assert.h>
#include <hashTable.h>
#include <parserB.h>
MAP_TYPE_DEF(struct variable, Var);
MAP_TYPE_FUNCS(struct variable, Var);
LL_TYPE_DEF(struct scope, Scope);
LL_TYPE_FUNCS(struct scope, Scope);
void variableDestroy(struct variable *var) {
	free(var->name);
	objectDestroy(&var->type);
	strParserNodeDestroy(&var->refs);
}
void scopeDestroy(void *s) {
	struct scope *scope = s;
	mapVarDestroy(scope->vars, (void (*)(void *))variableDestroy);
	llScopeDestroy(&scope->subScopes, (void (*)(void *))scopeDestroy);
}
static llScope masterScope = NULL;
static llScope currentScope = NULL;
void enterScope() {
	struct scope new;
	new.parent = currentScope;
	new.subScopes = NULL;
	new.vars = mapVarCreate();

	__auto_type newNode = llScopeCreate(new);
	if (currentScope == NULL) {
		currentScope = newNode;
	} else {
		llInsertListAfter(llScopeValuePtr(currentScope)->subScopes, newNode);
		llScopeValuePtr(currentScope)->subScopes = newNode;
	}
}
void leaveScope() {
	__auto_type par = llScopeValuePtr(currentScope)->parent;
	assert(par);
	currentScope = par;
}
void addVar(const struct parserNode *name, struct object *type) {
	struct variable var;
	var.type = type;
	var.refs = strParserNodeAppendItem(NULL, (struct parserNode *)name);

	assert(name->type == NODE_NAME);
	struct parserNodeName *name2 = (void *)name;
	var.name = malloc(strlen(name2->text) + 1);
	strcpy(var.name, name2->text);

	__auto_type scope = llScopeValuePtr(currentScope);
	__auto_type find = mapVarGet(scope->vars, var.name);
	if (!find) {
		// TODO whine about re-declaration
	} else {
		mapVarInsert(scope->vars, var.name, var);
	}
}
struct variable *getVar(const struct parserNode *name) {
	assert(name->type == NODE_NAME);
	const struct parserNodeName *name2 = (void *)name;

	for (__auto_type scope = currentScope; scope != NULL;
	     scope = llScopeValuePtr(scope)->parent) {
		struct variable *find =
		    mapVarGet(llScopeValuePtr(scope)->vars, name2->text);
		if (find) {
			find->refs =
			    strParserNodeAppendItem(find->refs, (struct parserNode*)name);
			return find;
		}
	}
	// TODO whine about not found.

	return NULL;
}
static void killParserData() __attribute__((destructor));
static void killParserData() { scopeDestroy(masterScope); masterScope=NULL;}
static void initParserData() __attribute__((constructor));
static void initParserData() {enterScope();}
