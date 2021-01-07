#include <assert.h>
#include <diagMsg.h>
#include <hashTable.h>
#include <parserB.h>
#include <string.h>
MAP_TYPE_DEF(struct parserNode *,ParserNode);
MAP_TYPE_FUNCS(struct parserNode *,ParserNode);
static __thread mapParserNode symbolTable=NULL;
static llScope currentScope = NULL;
struct parserNode *getGlobalSymbol(const char *name) {
		__auto_type find=mapParserNodeGet(symbolTable, name);
		if(!find)
				return NULL;
		return *find;
}
void addGlobalSymbol(struct parserNode *node) {
		switch(node->type) {
		case NODE_ASM_LABEL_GLBL: {
				struct parserNodeLabelGlbl *lab=(void*)node;
				struct parserNodeName *name=(void*)lab->name;
				mapParserNodeInsert(symbolTable, name->text,node);
				break;
		}
		case NODE_VAR: {
				struct parserNodeVar *var=(void*)node;
				mapParserNodeInsert(symbolTable, var->var->name,node);
				break;
		}
		case NODE_FUNC_DEF: {
				struct parserNodeFuncDef *def=(void*)node;
				struct parserNodeName *name=(void*)def->name;
				mapParserNodeInsert(symbolTable, name->text,node);
				break;
		}
		case NODE_CLASS_DEF: {
				struct parserNodeClassDef *class=(void*)node;
				struct parserNodeName *name=(void*)class->name;
				mapParserNodeInsert(symbolTable, name->text,node);
				break;
		}
		case NODE_UNION_DEF: {
				struct parserNodeUnionDef *class=(void*)node;
				struct parserNodeName *name=(void*)class->name;
				mapParserNodeInsert(symbolTable, name->text,node);
				break;
		}
		default:
				assert(0);
				break;
		}
}
void enterScope() {
	struct scope new;
	new.parent = currentScope;
	new.subScopes = NULL;
	new.vars = mapVarCreate();
	new.funcs = mapFuncCreate();

	__auto_type newNode = llScopeCreate(new);
	if (currentScope == NULL) {
		currentScope = newNode;
	} else {
		llInsertListAfter(llScopeValuePtr(currentScope)->subScopes, newNode);
		llScopeValuePtr(currentScope)->subScopes = newNode;

		currentScope = newNode;
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
	var.isGlobal = llScopeValuePtr(currentScope)->parent == NULL;

	assert(name->type == NODE_NAME);
	struct parserNodeName *name2 = (void *)name;
	var.name = malloc(strlen(name2->text) + 1);
	strcpy(var.name, name2->text);

	__auto_type scope = llScopeValuePtr(currentScope);
	__auto_type find = mapVarGet(scope->vars, var.name);
	if (find) {
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
			    strParserNodeAppendItem(find->refs, (struct parserNode *)name);
			return find;
		}
	}
	// TODO whine about not found.

	return NULL;
}
void killParserData() __attribute__((destructor));
void killParserData() {
	llScope top = currentScope;
	for (; currentScope != NULL;
	     currentScope = llScopeValuePtr(currentScope)->parent)
		top = currentScope;

	currentScope = NULL;
}
void initParserData();
void initParserData() {
		mapParserNodeDestroy(symbolTable, NULL);
		__initParserA();
		enterScope();
		symbolTable=mapParserNodeCreate();
}

struct function *getFunc(const struct parserNode *name) {
	struct parserNodeName *name2 = (void *)name;
	assert(name2->base.type == NODE_NAME);

	for (llScope scope = currentScope; scope != NULL;
	     scope = llScopeValuePtr(scope)->parent) {
		__auto_type find = mapFuncGet(llScopeValuePtr(scope)->funcs, name2->text);
		if (!find)
			continue;

		find->refs = strParserNodeAppendItem(find->refs, (struct parserNode *)name);
		return find;
	}
	return NULL;
}
void addFunc(const struct parserNode *name, const struct object *type,
             struct parserNode *func) {
	struct parserNodeName *name2 = (void *)name;
	__auto_type currentScopeFuncs = llScopeValuePtr(currentScope)->funcs;

	__auto_type conflict = mapFuncGet(currentScopeFuncs, name2->text);
	if (conflict) {
		if (!conflict->isForwardDecl) {
			// Whine about redeclaration
			diagErrorStart(name2->base.pos.start, name2->base.pos.end);
			char buffer[1024];
			sprintf(buffer, "Redeclaration of function '%s'.", name2->text);
			diagPushText(buffer);
			diagHighlight(name2->base.pos.start, name2->base.pos.end);
			diagEndMsg();

			__auto_type firstRef = conflict->refs[0];
			diagNoteStart(firstRef->pos.start, firstRef->pos.end);
			diagPushText("Declared here:");
			diagHighlight(firstRef->pos.start, firstRef->pos.end);
			diagEndMsg();
		} else if (conflict->isForwardDecl) {
			if (!objectEqual(conflict->type, type)) {
				// Whine about conflicting type
				diagErrorStart(name2->base.pos.start, name2->base.pos.end);
				diagPushText("Conflicting types for ");
				diagPushQoutedText(name2->base.pos.start, name2->base.pos.end);
				diagPushText(".");
				diagEndMsg();

				__auto_type firstRef = conflict->refs[0];
				diagNoteStart(firstRef->pos.start, firstRef->pos.end);
				diagPushText("Declared here:");
				diagHighlight(firstRef->pos.start, firstRef->pos.end);
				diagEndMsg();
			}
		}
	}

loop:;
	__auto_type find = mapFuncGet(currentScopeFuncs, name2->text);
	if (!find) {
		struct function dummy;
		dummy.isForwardDecl = func->type == NODE_FUNC_FORWARD_DECL;
		dummy.refs = NULL;
		dummy.type = (struct object *)type;
		dummy.node = func;
		dummy.name = malloc(strlen(name2->text) + 1);
		strcpy(dummy.name, name2->text);

		mapFuncInsert(currentScopeFuncs, name2->text, dummy);
		goto loop;
	}

	find->refs = strParserNodeAppendItem(find->refs, (struct parserNode *)name);
}
