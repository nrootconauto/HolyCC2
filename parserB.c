#include <assert.h>
#include "diagMsg.h"
#include "hashTable.h"
#include "parserB.h"
#include "string.h"
#include "registers.h"
#include "preprocessor.h"
#include "exprParser.h"
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
		case NODE_VAR_DECL: {
				struct parserNodeVarDecl *decl=(void*)find;
				return decl->type;
		}
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
	* Lesser items get replaced,negative items can be repeated
	*/
static const char *getSymbolName(struct parserNode *node);
#define SHADOW_FORWARD_DECL (-1)
static double shadowPrecedence(struct parserNode *node,struct linkage *link) {
		if(link) {
				__auto_type type=link->type;
				switch(type) {
				case LINKAGE_EXTERN:
				case LINKAGE_IMPORT:
						return SHADOW_FORWARD_DECL;
				default:;
				}
		}
		
		switch(node->type) {
		case NODE_ASM_LABEL_GLBL:
				return SHADOW_FORWARD_DECL-1; //Is a forward decl essentially
		case NODE_CLASS_FORWARD_DECL:
		case NODE_UNION_FORWARD_DECL:
		case NODE_FUNC_FORWARD_DECL:
				return SHADOW_FORWARD_DECL;
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
	case NODE_VAR_DECL: {
			struct parserNodeVarDecl *decl=(void*)node;
			return ((struct parserNodeName*)decl->name)->text;
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
		if(node->type==NODE_VAR_DECL) {
				return __addGlobalSymbol(((struct parserNodeVarDecl*)node)->var,name,link);
		} else if(node->type==NODE_VAR)
				var=((struct parserNodeVar*)node)->var;
		struct parserSymbol toInsert;
		toInsert.type = symbolType(node);
		toInsert.name=strcpy(calloc(strlen(name)+1,1), name);
		toInsert.link=linkageClone(link);
		toInsert.version=0;
		toInsert.var=var;
		if(node->type==NODE_FUNC_DEF) {
				struct parserNodeFuncDef *def=(void*)node;
				toInsert.function=def->func;
		} else if(node->type==NODE_FUNC_FORWARD_DECL) {
				struct parserNodeFuncForwardDec *fwd=(void*)node;
				toInsert.function=fwd->func;
		} else
				toInsert.function=NULL;
		toInsert.shadowPrec=shadowPrecedence(node,&link);
		const char *fn;
		long _start;
		parserNodeStartEndPos(node->pos.start,node->pos.end , &_start, NULL);
		diagLineCol(&fn, _start, &toInsert.ln,  &toInsert.col);
		toInsert.fn=strcpy(calloc(strlen(fn)+1,1), fn);
		
		if (name) {
				__auto_type find = mapSymbolGet(symbolTable, name);
				if (find) {
						long count = snprintf(NULL, 0, "%s_$%li", name, ++find[0]->version);
						char buffer[count + 1];
						sprintf(buffer, "%s_$%li", name, find[0]->version);
						if(toInsert.shadowPrec>=find[0]->shadowPrec) {
								// If we find a conflicting symbol,"version" the older symbol(give it a unique name).
								toInsert.version = find[0]->version;
								// Remove the current symbol(We backed it up as "name:VER")
								parserSymbolDestroy(mapSymbolGet(symbolTable, name));
								mapSymbolRemove(symbolTable, name, NULL);
								mapSymbolInsert(symbolTable, name, ALLOCATE(toInsert));
								return;
						}
						//Items with negative shadow preceence can be repeated see shadowPrecedence
						if(find[0]->shadowPrec==SHADOW_FORWARD_DECL||toInsert.shadowPrec==SHADOW_FORWARD_DECL) {
								return;
						} else {
								long _start,_end;
								parserNodeStartEndPos(node->pos.start,node->pos.end , &_start, &_end);
								// TODO whine about re-declaration
								diagErrorStart(_start, _end);
								diagPushText("Redeclaration of symbol ");
								diagPushQoutedText(_start, _end);
								diagPushText(".");
								diagEndMsg();
								return;
						}
				}
				mapSymbolInsert(symbolTable, name, ALLOCATE(toInsert));
		}
}
void parserAddGlobalSym(struct parserNode *node, struct linkage link) {
	__auto_type name = getSymbolName(node);
	if(node->type==NODE_FUNC_FORWARD_DECL||node->type==NODE_FUNC_DEF) {
			struct parserNode *nm=NULL;
			if(node->type==NODE_FUNC_FORWARD_DECL) nm=((struct parserNodeFuncForwardDec*)node)->name;
			else nm=((struct parserNodeFuncDef*)node)->name;
			return parserAddFunc(nm, symbolType(node), node, node->pos.start, node->pos.end, link);
	}
	__addGlobalSymbol(node, name, linkageClone(link));
}
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

		currentScope = newNode;
	}
}
void leaveScope() {
	__auto_type par = llScopeValuePtr(currentScope)->parent;
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
int isGlobalScope() {
		return llScopeValuePtr(currentScope)->parent==NULL;
}
void parserAddVar(const struct parserNode *name, struct object *type,struct reg *inReg,int isNoReg,struct linkage *glblLinkage) {
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
	
	if(scope->parent==NULL) {
			int isForwardDecl=0;
			if(glblLinkage)
					isForwardDecl=glblLinkage->type==LINKAGE_IMPORT||glblLinkage->type==LINKAGE_EXTERN;
			__auto_type find=parserGetGlobalSym(var.name);
			if(find)
					if(find->shadowPrec>SHADOW_FORWARD_DECL&&!isForwardDecl)
							goto whineRepeat;
	} else {
	loop:;
			__auto_type findLoc = mapVarGet(scope->vars, var.name);
			if (findLoc) {
			whineRepeat:;
					long _start,_end;
					parserNodeStartEndPos(name2->base.pos.start, name2->base.pos.end, &_start, &_end);
					diagErrorStart(_start,_end);
					diagPushText("Redeclaration of variable ");
					diagPushQoutedText(_start, _end);
					diagPushText(".");
					diagHighlight(_start, _end);
					diagEndMsg();
					mapVarRemove(scope->vars, var.name,NULL);
					goto loop;
			}
	}
	mapVarInsert(scope->vars, var.name, ALLOCATE(var));
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
	initExprParser();
}
struct parserFunction *parserGetFunc(const struct parserNode *name) {
	struct parserNodeName *name2 = (void *)name;
	assert(name2->base.type == NODE_NAME);
	__auto_type nameText=((struct parserNodeName *)name)->text;
	__auto_type find=parserGetFuncByName(nameText);
	if(!find) return NULL;
	if(find) find->refs=strParserNodeAppendItem(find->refs, (struct parserNode*)name);
	return find;
}
struct parserFunction *parserGetFuncByName(const char *name) {
		__auto_type find=mapSymbolGet(symbolTable, name);
		if(!find) return NULL;
		return find[0]->function;
}
void parserAddFunc(const struct parserNode *name, const struct object *type, struct parserNode *func,llLexerItem start,llLexerItem end,struct linkage link) {
	struct parserNodeName *name2 = (void *)name;

	__auto_type conflict = parserGetFunc(name);
	if (conflict) {
		if (!conflict->isForwardDecl&&func->type!=NODE_FUNC_FORWARD_DECL) {
			long _start,_end;
			parserNodeStartEndPos(name2->base.pos.start,name2->base.pos.end , &_start, &_end);
				// Whine about redeclaration
			diagErrorStart(_start, _end);
			char buffer[1024];
			sprintf(buffer, "Redeclaration of function '%s'.", name2->text);
			diagPushText(buffer);
			diagHighlight(_start, _end);
			diagEndMsg();

			__auto_type firstRef = conflict->refs[0];
			parserNodeStartEndPos(firstRef->pos.start,firstRef->pos.end , &_start, &_end);
			diagNoteStart(_start, _end);
			diagPushText("Declared here:");
			diagHighlight(_start, _end);
			diagEndMsg();
		} else if (conflict->isForwardDecl) {
			if (!objectEqual(conflict->type, type)) {
					long _start,_end;
					parserNodeStartEndPos(name2->base.pos.start,name2->base.pos.end , &_start, &_end);
				// Whine about conflicting type		
				diagErrorStart(_start, _end);
				diagPushText("Conflicting types for ");
				diagPushQoutedText(_start, _end);
				diagPushText(".");
				diagEndMsg();	
			
				__auto_type firstRef = conflict->refs[0];
				parserNodeStartEndPos(firstRef->pos.start,firstRef->pos.end , &_start, &_end);
				diagNoteStart(_start, _end);
				diagPushText("Declared here:");
				diagHighlight(_start, _end);
				diagEndMsg();
			}
		}
	}

loop:;
	
		struct parserFunction dummy;
		dummy.isForwardDecl = func->type == NODE_FUNC_FORWARD_DECL;
		dummy.refs = strParserNodeAppendItem(NULL, (struct parserNode *)name);;
		dummy.type = (struct object *)type;
		dummy.node = func;
		dummy.name = calloc(strlen(name2->text) + 1,1);
		dummy.parentFunction = NULL;
		dummy.__cacheEndToken=end;
		dummy.__cacheStartToken=start;
		strcpy(dummy.name, name2->text);

		if(func->type==NODE_FUNC_DEF) {
				struct parserNodeFuncDef *def=(void*)func;
				def->func=ALLOCATE(dummy);
		} else if(func->type==NODE_FUNC_FORWARD_DECL) {
				struct parserNodeFuncForwardDec *fwd=(void*)func;
				fwd->func=ALLOCATE(dummy);
		}
		__addGlobalSymbol(func, name2->text, link);
}
void parserMoveGlobals2Extern() {
		long count;
		parserSymTableNames(NULL, &count);
		const char *names[count];
		parserSymTableNames(names, NULL);
		for(long n=0;n!=count;n++) {
				__auto_type find=parserGetGlobalSym(names[n]);
				if(find->link.type==LINKAGE_STATIC||find->link.type==LINKAGE_INTERNAL) {
						mapSymbolRemove(symbolTable, names[n], (void(*)(void*))parserSymbolDestroy);
				} else if(find->link.type==LINKAGE_LOCAL||find->link.type==LINKAGE_PUBLIC) {
						find->link.type=LINKAGE_IMPORT;
				}
		}
		leaveScope();
		enterScope();

		parserSymTableNames(NULL, &count);
		const char *names2[count];
		parserSymTableNames(names2, NULL);
		for(long v=0;v!=count;v++) {
				__auto_type find=parserGetGlobalSym(names2[v]);
				if(!find->var) continue;
				mapVarInsert(llScopeValuePtr(currentScope)->vars,names2[v],find->var);
		}
}
