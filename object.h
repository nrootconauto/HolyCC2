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
		char *name;
};
struct objectMethod {
		struct parserNode *name;
		struct object *type;
};
LL_TYPE_DEF(struct objectMethod, Method);
LL_TYPE_FUNCS(struct objectMethod, Method);
struct objectClass {
	struct object base;
	struct parserNode *name;
	strObjectMember members;
		llMethod methods;
		long align;
	long size;
};
struct objectUnion {
	struct object base;
	struct parserNode *name;
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
		struct parserNode *name;
		enum holyCTypeKind type;
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
struct object *objectUnionCreate(const struct parserNode *name,
                                 const struct objectMember *members,
                                 long count);
struct object *objectClassCreate(const struct parserNode *name,
                                 const struct objectMember *members,
                                 long count);
long objectSize(const struct object *type, int *success);
void objectMemberDestroy(struct objectMember *member);
void objectMemberAttrDestroy(struct objectMemberAttr *attr);
long objectAlign(const struct object *type, int *success);
struct object *objectForwardDeclarationCreate(const struct parserNode  *name,enum holyCTypeKind type);
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
char *object2Str(struct object *obj);
int objectEqual(const struct object *a,const struct object *b);
int objectIsCompat(const struct object *a,const struct object *b);
