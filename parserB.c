#include <assert.h>
#include "diagMsg.h"
#include "hashTable.h"
#include "parserB.h"
#include "string.h"
#include "registers.h"
#include "preprocessor.h"
#define ALLOCATE(x)																																																					\
	({                                                                                                                                                               \
		__auto_type len = sizeof(x);                                                                                                                                   \
		void *$retVal = calloc(len,1);																																									\
		memcpy($retVal, &x, len);                                                                                                                                      \
		$retVal;                                                                                                                                                       \
	})
MAP_TYPE_DEF(struct parserSymbol*,Symbol);
MAP_TYPE_FUNCS(struct parserSymbol*, Symbol);
static __thread mapSymbol symbolTable = NULL;
static llScope currentScope = NULL;
void parserSymTableNames(const char **keys, long *count) {
	mapSymbolKeys(symbolTable, keys, count);
}
const struct linkage *parserGlobalSymLinkage(const char *name) {
	__auto_type find = mapSymbolGet(symbolTable, name);
	if (!find)
		return NULL;
	return &find[0]->link;
}
const char  *parserGetGlobalSymLinkageName(const char *name) {
		__auto_type find = mapSymbolGet(symbolTable, name);
		if(find)
				if(find[0]->link.fromSymbol)
						return find[0]->link.fromSymbol;
		return name;
}
static struct object *symbolType(struct parserNode *find) {
		switch(find->type) {
		case NODE_CLASS_DEF: {
				struct parserNodeClassDef *def=(void*)find;
				return def->type;
		}
		case NODE_CLASS_FORWARD_DECL:  {
				struct parserNodeClassFwd *fwd=(void*)find;
				return fwd->type;
		}
		case NODE_FUNC_DEF: {
				struct parserNodeFuncDef *func=(void*)find;
				return func->funcType;
		}
		case NODE_FUNC_FORWARD_DECL:  {
				struct parserNodeFuncForwardDec *func=(void*)find;
				return func->funcType;
		}
		case NODE_UNION_DEF: {
				struct parserNodeUnionDef *un=(void*)find;
				return un->type;
		}
		case NODE_UNION_FORWARD_DECL:  {
				struct parserNodeUnionFwd *fwd=(void*)find;
				return fwd->type;
		}
		case NODE_VAR: {
				struct parserNodeVar *var=(void*)find;
				return var->var->type;
		}
		case NODE_ASM_LABEL_GLBL: {
				return objectPtrCreate(&typeU0);
		}
		default:
				fputs("Symbol type not implemnted", stderr);
				abort();
		}
}
struct parserSymbol *parserGetGlobalSym(const char *name) {
	__auto_type find = mapSymbolGet(symbolTable, name);
	if(!find)
			return NULL;
	return find[0];
}
strParserSymbol parserSymbolTableSyms() {
	long count;
	mapSymbolKeys(symbolTable, NULL, &count);
	const char *keys[count];
	mapSymbolKeys(symbolTable, keys, NULL);
	strParserSymbol retVal = strParserSymbolResize(NULL, count);
	for (long i = 0; i != count; i++)
			retVal[i] = *mapSymbolGet(symbolTable, keys[i]);
	return retVal;
}
/**
	* Lesser items get replaced
	*/
static int shadowPrecedence(struct parserNode *node) {
		switch(node->type) {
		case NODE_ASM_LABEL_GLBL:
				return -1;
		case NODE_CLASS_FORWARD_DECL:
		case NODE_UNION_FORWARD_DECL:
		case NODE_FUNC_FORWARD_DECL:
				return 1;
		case NODE_VAR_DECL:
		case NODE_FUNC_DEF:
		case NODE_CLASS_DEF:
		case NODE_UNION_DEF:
		case NODE_VAR:
				return 2;
		default:
				assert(0);
				return 1;
		}
}
static const char *getSymbolName(struct parserNode *node) {
	switch (node->type) {
	case NODE_CLASS_FORWARD_DECL: {
		struct parserNodeClassFwd *fwd = (void *)node;
		struct parserNodeName *nm = (void *)fwd->name;
		return nm->text;
	}
	case NODE_UNION_FORWARD_DECL: {
		struct parserNodeUnionFwd *fwd = (void *)node;
		struct parserNodeName *nm = (void *)fwd->name;
		return nm->text;
	}
	case NODE_ASM_LABEL_GLBL: {
		struct parserNodeLabelGlbl *lab = (void *)node;
		struct parserNodeName *name = (void *)lab->name;
		return name->text;
	}
	case NODE_VAR: {
		struct parserNodeVar *var = (void *)node;
		return var->var->name;
	}
	case NODE_FUNC_DEF: {
		struct parserNodeFuncDef *def = (void *)node;
		struct parserNodeName *name = (void *)def->name;
		return name->text;
	}
	case NODE_FUNC_FORWARD_DECL: {
		struct parserNodeFuncForwardDec *forward = (void *)node;
		struct parserNodeName *name = (void *)forward->name;
		return name->text;
	}
	case NODE_CLASS_DEF: {
		struct parserNodeClassDef *class = (void *)node;
		struct parserNodeName *name = (void *)class->name;
		return name->text;
	}
	case NODE_UNION_DEF: {
		struct parserNodeUnionDef *class = (void *)node;
		struct parserNodeName *name = (void *)class->name;
		return name->text;
	}
	default:
		assert(0);
		break;
	}
	return NULL;
}
static void parserSymbolDestroy(struct parserSymbol **sym) {
		free(sym[0]->fn);
		free(sym[0]->name);
		free(*sym);
}
static void __addGlobalSymbol(struct parserNode *node, const char *name, struct linkage link) {
		struct parserVar *var=NULL;
		if(node->type==NODE_VAR)
				var=((struct parserNodeVar*)node)->var;
		struct parserSymbol toInsert;
		toInsert.type = symbolType(node);
		toInsert.name=strcpy(calloc(strlen(name)+1,1), name);
		toInsert.link=linkageClone(link);
		toInsert.version=0;
		toInsert.var=var;
		toInsert.shadowPrec=shadowPrecedence(node);
		const char *fn;
		diagLineCol(&fn, node->pos.start, &toInsert.ln,  &toInsert.col);
		toInsert.fn=strcpy(calloc(strlen(fn)+1,1), fn);
		
		if (name) {
				__auto_type find = mapSymbolGet(symbolTable, name);
				if (find) {
						long count = snprintf(NULL, 0, "%s_$%li", name, ++find[0]->version);
						char buffer[count + 1];
						sprintf(buffer, "%s_$%li", name, find[0]->version);
						if(toInsert.shadowPrec>find[0]->shadowPrec) {
								// If we find a conflicting symbol,"version" the older symbol(give it a unique name).
								toInsert.version = find[0]->version;
								// Remove the current symbol(We backed it up as "name:VER")
								parserSymbolDestroy(mapSymbolGet(symbolTable, name));
								mapSymbolRemove(symbolTable, name, NULL);
								mapSymbolInsert(symbolTable, name, ALLOCATE(toInsert));
								return;
						} else {
								toInsert.version = find[0]->version;
								mapSymbolInsert(symbolTable, buffer, ALLOCATE(toInsert));
								return;
						}
				}
				mapSymbolInsert(symbolTable, name, ALLOCATE(toInsert));
		}
}
void parserAddGlobalSym(struct parserNode *node, struct linkage link) {
	__auto_type name = getSymbolName(node);
	__addGlobalSymbol(node, name, linkageClone(link));
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
void parserAddVarLenArgsVars2Func(struct parserVar **Argc,struct parserVar **Argv) {
		{
				const char *name="argc";
				struct parserVar argc;
				argc.type = dftValType();
				argc.refs = NULL;
				argc.isGlobal = 0;
				argc.isNoreg = 0;
				argc.isTmp=0;
				argc.refCount=1;
				argc.inReg=NULL;
				argc.name=strcpy(calloc(strlen(name)+1,1), name);
				__auto_type scope = llScopeValuePtr(currentScope);
				__auto_type find = mapVarGet(scope->vars, argc.name);
				if (find) {
						// TODO whine about re-declaration
				} else {
						mapVarInsert(scope->vars, argc.name, ALLOCATE(argc));
						if(Argc)
								*Argc=*mapVarGet(scope->vars, argc.name);
				}
		}
		{
				const char *name="argv";
				struct parserVar argv;
				argv.type = objectPtrCreate(dftValType());
				argv.refs = NULL;
				argv.isGlobal = 0;
				argv.isNoreg = 0;
				argv.isTmp=0;
				argv.refCount=1;
				argv.inReg=NULL;
				argv.name=strcpy(calloc(strlen(name)+1,1), name);
				__auto_type scope = llScopeValuePtr(currentScope);
				__auto_type find = mapVarGet(scope->vars, argv.name);
				if (find) {
						// TODO whine about re-declaration
				} else {
						mapVarInsert(scope->vars, argv.name, ALLOCATE(argv));
						if(Argv)
								*Argv=*mapVarGet(scope->vars, argv.name);
				}
		}
}
void parserAddVar(const struct parserNode *name, struct object *type,struct reg *inReg,int isNoReg) {
	struct parserVar var;
	var.type = type;
	var.refs = strParserNodeAppendItem(NULL, (struct parserNode *)name);
	var.isGlobal = (llScopeValuePtr(currentScope)->parent == NULL) ? 1 : 0;
	var.isNoreg = isNoReg;
	var.isRefedByPtr=0;
	var.isTmp=0;
	var.inReg=inReg;
	var.refCount=1;
	
	assert(name->type == NODE_NAME);
	struct parserNodeName *name2 = (void *)name;
	var.name = calloc(strlen(name2->text) + 1,1);
	strcpy(var.name, name2->text);

	__auto_type scope = llScopeValuePtr(currentScope);
	__auto_type find = mapVarGet(scope->vars, var.name);
	if (find) {
		// TODO whine about re-declaration
	} else {
			mapVarInsert(scope->vars, var.name, ALLOCATE(var));
	}
}
struct parserVar *parserGetVarByText(const char *name) {
	for (__auto_type scope = currentScope; scope != NULL; scope = llScopeValuePtr(scope)->parent) {
		struct parserVar **find = mapVarGet(llScopeValuePtr(scope)->vars, name);
		if (find) {
			find[0]->refs = strParserNodeAppendItem(find[0]->refs, (struct parserNode *)name);
			return find[0];
		}
	}
	// TODO whine about not found.

	return NULL;
}
struct parserVar *parserGetVar(const struct parserNode *name) {
	assert(name->type == NODE_NAME);
	const struct parserNodeName *name2 = (void *)name;

	for (__auto_type scope = currentScope; scope != NULL; scope = llScopeValuePtr(scope)->parent) {
		struct parserVar **find = mapVarGet(llScopeValuePtr(scope)->vars, name2->text);
		if (find) {
			find[0]->refs = strParserNodeAppendItem(find[0]->refs, (struct parserNode *)name);
			return find[0];
		}
	}
	// TODO whine about not found.

	return NULL;
}
void killParserData() __attribute__((destructor));
void killParserData() {
	llScope top = currentScope;
	for (; currentScope != NULL; currentScope = llScopeValuePtr(currentScope)->parent)
		top = currentScope;

	currentScope = NULL;
}
static void strFree(void *ptrPtr) {
	free(*(char **)ptrPtr);
}
void initParserData() {
	mapSymbolDestroy(symbolTable, NULL);
	__initParserA();
	currentScope = NULL;
	enterScope();
	symbolTable = mapSymbolCreate();
}
struct parserFunction *parserGetFunc(const struct parserNode *name) {
	struct parserNodeName *name2 = (void *)name;
	assert(name2->base.type == NODE_NAME);

	for (llScope scope = currentScope; scope != NULL; scope = llScopeValuePtr(scope)->parent) {
		__auto_type find = mapFuncGet(llScopeValuePtr(scope)->funcs, name2->text);
		if (!find)
			continue;

		find[0]->refs = strParserNodeAppendItem(find[0]->refs, (struct parserNode *)name);
		return *find;
	}
	return NULL;
}
struct parserFunction *parserGetFuncByName(const char *name) {
	for (llScope scope = currentScope; scope != NULL; scope = llScopeValuePtr(scope)->parent) {
		__auto_type find = mapFuncGet(llScopeValuePtr(scope)->funcs, name);
		if (!find)
			continue;

		find[0]->refs = strParserNodeAppendItem(find[0]->refs, (struct parserNode *)name);
		return *find;
	}
	return NULL;
}
void parserAddFunc(const struct parserNode *name, const struct object *type, struct parserNode *func) {
	struct parserNodeName *name2 = (void *)name;
	__auto_type currentScopeFuncs = llScopeValuePtr(currentScope)->funcs;

	__auto_type conflict = mapFuncGet(currentScopeFuncs, name2->text);
	if (conflict) {
		if (!conflict[0]->isForwardDecl) {
			// Whine about redeclaration
			diagErrorStart(name2->base.pos.start, name2->base.pos.end);
			char buffer[1024];
			sprintf(buffer, "Redeclaration of function '%s'.", name2->text);
			diagPushText(buffer);
			diagHighlight(name2->base.pos.start, name2->base.pos.end);
			diagEndMsg();

			__auto_type firstRef = conflict[0]->refs[0];
			diagNoteStart(firstRef->pos.start, firstRef->pos.end);
			diagPushText("Declared here:");
			diagHighlight(firstRef->pos.start, firstRef->pos.end);
			diagEndMsg();
		} else if (conflict[0]->isForwardDecl) {
			if (!objectEqual(conflict[0]->type, type)) {
				// Whine about conflicting type
				diagErrorStart(name2->base.pos.start, name2->base.pos.end);
				diagPushText("Conflicting types for ");
				diagPushQoutedText(name2->base.pos.start, name2->base.pos.end);
				diagPushText(".");
				diagEndMsg();

				__auto_type firstRef = conflict[0]->refs[0];
				diagNoteStart(firstRef->pos.start, firstRef->pos.end);
				diagPushText("Declared here:");
				diagHighlight(firstRef->pos.start, firstRef->pos.end);
				diagEndMsg();
			}
			mapFuncRemove(currentScopeFuncs, name2->text, NULL);
		}
	}

loop:;
	__auto_type find = mapFuncGet(currentScopeFuncs, name2->text);
	if (!find) {
		struct parserFunction dummy;
		dummy.isForwardDecl = func->type == NODE_FUNC_FORWARD_DECL;
		dummy.refs = NULL;
		dummy.type = (struct object *)type;
		dummy.node = func;
		dummy.name = calloc(strlen(name2->text) + 1,1);
		dummy.parentFunction = NULL;
		strcpy(dummy.name, name2->text);

		mapFuncInsert(currentScopeFuncs, name2->text, ALLOCATE(dummy));
		goto loop;
	}

	find[0]->refs = strParserNodeAppendItem(find[0]->refs, (struct parserNode *)name);
}
