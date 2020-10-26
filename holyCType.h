#pragma once
#include <linkedList.h>
#include <str.h>
enum holyCTypeKind {
	TYPE_U0,
	TYPE_U8i,
	TYPE_U16i,
	TYPE_U32i,
	TYPE_U64i,
	TYPE_I8i,
	TYPE_I16i,
	TYPE_I32i,
	TYPE_I64i,
	TYPE_CLASS,
	TYPE_UNION,
	TYPE_PTR,
	TYPE_ARRAY,
	TYPE_FORWARD,
};
struct objectMemberAttr {
	char *name;
	struct parserNode *value;
};
LL_TYPE_DEF(struct memberAttr, ObjectMemberAttr);
LL_TYPE_FUNCS(struct objectMemberAttr, ObjectMemberAttr);

struct object;
struct objectMember {
	struct object *type;
	char *name;
	llObjectMemberAttr attrs;
	long offset;
};
LL_TYPE_DEF(struct objectMember, ObjectMember);
LL_TYPE_FUNCS(struct objectMember, ObjectMember);
struct object {
	enum holyCTypeKind type;
};
struct objectClass {
	struct object base;
	char *name;
	llObjectMember members;
	long align;
	long size;
};
struct objectUnion {
	struct object base;
	char *name;
	llObjectMember members;
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
struct object *objectArrayCreate(struct object *baseType,
                                 struct parserNode *dim);
struct object *objectPtrCreate(struct object *baseType);
struct object *objectUnionCreate(const char *name,
                                 const struct objectMember **members,
                                 long count);
struct object *objectClassCreate(const char *name,
                                 const struct objectMember **members,
                                 long count);
long objectSize(const struct object *type, int *success);
void objectDestroy(struct object **type);
void objectMemberDestroy(struct objectMember *member);
void objectMemberAttrDestroy(struct objectMemberAttr *attr);
long objectAlign(const struct object *type, int *success);
struct object *objectForwardDeclarationCreate(const char *name);
