#pragma once
#include <linkedList.h>
#include <str.h>
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
enum linkage {
 LINKAGE_STATIC=1,
 LINKAGE_PUBLIC,
 LINKAGE_EXTERN,
 LINKAGE__EXTERN,
 LINKAGE_IMPORT,
 LINKAGE__IMPORT,
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
};
STR_TYPE_DEF(struct objectMember, ObjectMember);
STR_TYPE_FUNCS(struct objectMember, ObjectMember);
struct object {
	enum holyCTypeKind type;
	enum linkage link;
};
struct objectClass {
	struct object base;
	char *name;
	strObjectMember members;
	long align;
	long size;
};
struct objectUnion {
	struct object base;
	char *name;
	strObjectMember members;
	long align;
	long size;
};
struct objectPtr {
	struct object base;
	struct object *type;
};
struct objectArray {
	struct object base;
	struct object *type;
	struct parserNode *dim;
};
struct objectForwardDeclaration {
	struct object base;
	char *name;
};
struct objectFuncArg {
	struct object *type;
	struct parserNode *name;
	struct parserNode *dftVal;
};
STR_TYPE_DEF(struct objectFuncArg, FuncArg);
STR_TYPE_FUNCS(struct objectFuncArg, FuncArg);
struct objectFunction {
	struct object base;
	struct object *retType;
	strFuncArg args;
};
struct object;
struct object *objectArrayCreate(struct object *baseType,
                                 struct parserNode *dim);
struct object *objectPtrCreate(struct object *baseType);
struct object *objectUnionCreate(const char *name,
                                 const struct objectMember *members,
                                 long count);
struct object *objectClassCreate(const char *name,
                                 const struct objectMember *members,
                                 long count);
long objectSize(const struct object *type, int *success);
void objectDestroy(struct object **type);
void objectMemberDestroy(struct objectMember *member);
void objectMemberAttrDestroy(struct objectMemberAttr *attr);
long objectAlign(const struct object *type, int *success);
struct object *objectForwardDeclarationCreate(const char *name);
struct object *objectByName(const char *name);
struct object *objectFuncCreate(struct object *retType, strFuncArg args);

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

void strFuncArgDestroy2(strFuncArg *args) ;
