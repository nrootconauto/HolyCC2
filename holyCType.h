#pragma once
#include <linkedList.h>
#include <str.h>
enum holyCTypeKind {
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
};
struct holyCMemberAttr {
	struct __vec *name;
	struct parserNode *value;
};
LL_TYPE_DEF(struct holyCMemberAttr, TypeAttr);
LL_TYPE_FUNCS(struct holyCMemberAttr, TypeAttr);

struct holyCType;
struct holyCTypeMember {
struct holyCType* type;	
 struct __vec *name;
	llTypeAttr attrs;
};
LL_TYPE_DEF(struct holyCTypeMember, TypeMember);
LL_TYPE_FUNCS(struct holyCTypeMember, TypeMember);
struct holyCType {
	enum holyCTypeKind type;
};
struct holyCTypeClass {
	struct holyCType base;
	struct __vec *name;
	llTypeMember members;
};
struct holyCTypeUnion {
	struct holyCType base;
	struct __vec *name;
	llTypeMember members;
};
struct holyCTypePtr {
	struct holyCType base;
	struct holyCType *type;
};
struct holyCTypeArray {
	struct holyCType base;
	struct holyCType *type;
};
