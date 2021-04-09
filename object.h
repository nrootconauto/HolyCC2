#pragma once
#include "linkedList.h"
#include "str.h"
#include "parserA.h"
enum holyCTypeKind {
	TYPE_Bool,
	TYPE_U0,
	TYPE_U8i,
	TYPE_U16i,
	TYPE_U32i,
	TYPE_U64i,
	TYPE_I8i,
	TYPE_I16i,
	TYPE_I32i,
	TYPE_I64i,
	TYPE_F64,
	TYPE_CLASS,
	TYPE_UNION,
	TYPE_PTR,
	TYPE_ARRAY,
	TYPE_FORWARD,
	TYPE_FUNCTION,
};
struct objectMemberAttr {
	char *name;
	struct parserNode *value;
};
STR_TYPE_DEF(struct objectMemberAttr, ObjectMemberAttr);
STR_TYPE_FUNCS(struct objectMemberAttr, ObjectMemberAttr);

struct object;
struct objectMember {
		struct object *type;
		char *name;
		strObjectMemberAttr attrs;
		long offset;
		struct object *belongsTo;
};
STR_TYPE_DEF(struct objectMember, ObjectMember);
STR_TYPE_FUNCS(struct objectMember, ObjectMember);
STR_TYPE_DEF(struct objectMember *, ObjectMemberP);
STR_TYPE_FUNCS(struct objectMember *, ObjectMemberP);
struct object {
	enum holyCTypeKind type;
	char *name;
};
struct objectClass {
	struct object base;
	struct object *baseType;
	struct parserNode *name;
	strObjectMember members;
	long align;
	long size;
		llLexerItem __cacheStart,__cacheEnd;
};
struct objectUnion {
	struct object base;
	struct object *baseType;
	struct parserNode *name;
	strObjectMember members;
	long align;
	long size;
		llLexerItem __cacheStart,__cacheEnd;
};
struct objectPtr {
	struct object base;
	struct object *type;
};
struct objectArray {
	struct object base;
	struct object *type;
	struct parserNode *dim;
		void* dimIR;
};
struct objectForwardDeclaration {
	struct object base;
	struct parserNode *name;
	enum holyCTypeKind type;
};
struct objectFuncArg {
		struct object *type;
		struct parserNode *name;
		struct parserNode *dftVal;
		struct parserVar *var;
};
STR_TYPE_DEF(struct objectFuncArg, FuncArg);
STR_TYPE_FUNCS(struct objectFuncArg, FuncArg);
struct objectFunction {
		struct object base;
		struct object *retType;
		strFuncArg args;
		int hasVarLenArgs;
		struct parserVar *argcVar;
		struct parserVar *argvVar;
};
struct object;
struct object *objectArrayCreate(struct object *baseType, struct parserNode *dim,void *dimIR);
struct object *objectPtrCreate(struct object *baseType);
struct object *
objectUnionCreate(const struct parserNode *name , const struct objectMember *members, long count,struct object *baseType);
struct object *objectClassCreate(const struct parserNode *name, const struct objectMember *members, long count);
long objectSize(const struct object *type, int *success);
void objectMemberDestroy(struct objectMember *member);
void objectMemberAttrDestroy(struct objectMemberAttr *attr);
long objectAlign(const struct object *type, int *success);
struct object *objectForwardDeclarationCreate(const struct parserNode *name, enum holyCTypeKind type);
struct object *objectByName(const char *name);
struct object *objectFuncCreate(struct object *retType, strFuncArg args,int varLenArgs);

extern struct object typeBool;
extern struct object typeU0;
extern struct object typeU8i;
extern struct object typeU16i;
extern struct object typeU32i;
extern struct object typeU64i;
extern struct object typeI8i;
extern struct object typeI16i;
extern struct object typeI32i;
extern struct object typeI64i;
extern struct object typeF64;

char *object2Str(struct object *obj);
int objectEqual(const struct object *a, const struct object *b);
int objectIsCompat(const struct object *a, const struct object *b);
struct object *objectBaseType(const struct object *obj);
void initObjectRegistry();
struct objectMember *objectMemberGet(struct object *aType,struct parserNodeName *nm);
long objectArrayIsConstSz(struct object *type);
void objectArrayDimValues(struct object *type,long *dimCount,long *values);
