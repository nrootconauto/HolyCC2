#include <holyCType.h>
#include <string.h>
#include <hashTable.h>
static char *strClone(const char *str) {
	__auto_type len = strlen(str);
	char *retVal = malloc(len + 1);
	strcpy(retVal, str);
	return retVal;
}
long objectAlign(const struct object *type, int *success) {
	if (success != NULL)
		*success = 1;

	switch (type->type) {
	case TYPE_ARRAY:
		// TODO check if constant size
		*success = 0;
		return -1;
	case TYPE_U0: {
		return 0;
	}
	case TYPE_PTR: {
		// TODO check architecture
		return 0;
	}
	case TYPE_UNION: {
		__auto_type ptr = (struct objectUnion *)type;
		return ptr->size;
	}
	case TYPE_CLASS: {
		__auto_type ptr = (struct objectClass *)type;
		return ptr->size;
	}
	case TYPE_I8i:
	case TYPE_U8i: {
		return 1;
	}
	case TYPE_I16i:
	case TYPE_U16i: {
		return 2;
	}
	case TYPE_I32i:
	case TYPE_U32i: {
		return 4;
	}
	case TYPE_I64i:
	case TYPE_U64i: {
		return 2;
	}
	}
}
void objectMemberAttrDestroy(struct objectMemberAttr *attr) {
	free(attr->name);
	// TODO free expression
}
void objectMemberDestroy(struct objectMember *member) {
	free(member->name);
	llObjectMemberAttrDestroy(&member->attrs,
	                          (void (*)(void *))objectMemberAttrDestroy);
}
void objectDestroy(struct object **type) {
	struct object *type2 = *type;
	switch (type2->type) {
	case TYPE_CLASS: {
		__auto_type item = (struct objectClass *)type2;
		free(item->name);
		llObjectMemberDestroy(&item->members,
		                      (void (*)(void *))objectMemberDestroy);
		free(type2);
		return;
	}
	case TYPE_UNION: {
		__auto_type item = (struct objectUnion *)type2;
		free(item->name);
		llObjectMemberDestroy(&item->members,
		                      (void (*)(void *))objectMemberDestroy);
		free(type2);
		return;
	}
	default:;
	}
}
long objectSize(const struct object *type, int *success) {
	if (success != NULL)
		*success = 1;

	switch (type->type) {
	case TYPE_ARRAY:
		// TODO check if const size
		if (success != NULL)
			*success = 0;
		return -1;
	case TYPE_U0: {
		return 0;
	}
	case TYPE_PTR: {
		// TODO check architecture
		return 0;
	}
	case TYPE_UNION: {
		__auto_type ptr = (struct objectUnion *)type;
		return ptr->size;
	}
	case TYPE_CLASS: {
		__auto_type ptr = (struct objectClass *)type;
		return ptr->size;
	}
	case TYPE_I8i:
	case TYPE_U8i: {
		return 1;
	}
	case TYPE_I16i:
	case TYPE_U16i: {
		return 2;
	}
	case TYPE_I32i:
	case TYPE_U32i: {
		return 4;
	}
	case TYPE_I64i:
	case TYPE_U64i: {
		return 2;
	}
	}
}
struct object *objectClassCreate(const char *name,
                                 const struct objectMember **members,
                                 long count) {
	struct objectClass *newClass = malloc(sizeof(struct objectClass));
	newClass->name = strClone(name);
	newClass->base.type = TYPE_CLASS;
	newClass->members = NULL;

	long largestMemberAlign = 0;
	int success;
	for (long i = 0; i != count; i++) {
		__auto_type align = objectAlign(members[i]->type, &success);
		if (!success)
			goto fail;

		if (align > largestMemberAlign)
			largestMemberAlign = align;
	}

	newClass->align = largestMemberAlign;

	newClass->members = NULL;
	long offset = 0;
	for (long i = 0; i != count; i++) {
		offset += offset & objectAlign(members[i]->type, &success);
		if (!success)
			goto fail;

		struct objectMember clone = *members[i];
		clone.offset = offset;
		clone.name = strClone(clone.name);

		llObjectMember tmp = llObjectMemberCreate(clone);
		llObjectMemberAttrInsertListAfter(llObjectMemberLast(newClass->members),
		                                  tmp);
	}

	newClass->size = offset + (largestMemberAlign % offset);

	return (struct object *)newClass;
fail:
	objectDestroy((struct object **)&newClass);
	return NULL;
}
struct object *objectUnionCreate(const char *name,
                                 const struct objectMember **members,
                                 long count) {
	int success;

	struct objectUnion *newUnion = malloc(sizeof(struct objectUnion));
	newUnion->name = strClone(name);
	newUnion->base.type = TYPE_CLASS;
	newUnion->members = NULL;

	long largestMemberAlign = 0;
	long largestSize = 0;
	for (long i = 0; i != count; i++) {
		struct objectMember clone = *members[i];
		clone.offset = 0;

		__auto_type align = objectAlign(members[i]->type, &success);
		if (!success)
			goto fail;
		__auto_type size = objectSize(members[i]->type, &success);
		if (!success)
			goto fail;

		if (align > largestMemberAlign)
			largestMemberAlign = align;
		if (size > largestSize)
			largestSize = size;

		clone.name = strClone(clone.name);
		llObjectMemberInsertListAfter(llObjectMemberLast(newUnion->members),
		                              llObjectMemberCreate(clone));
	}
	largestSize += largestSize % largestMemberAlign;
	newUnion->size = largestSize;
	newUnion->align = largestMemberAlign;

	return (struct object *)newUnion;
fail:
	objectDestroy((struct object **)&newUnion);
	return NULL;
}
struct object *objectPtrCreate(struct object *baseType) {
	struct objectPtr *ptr = malloc(sizeof(struct objectPtr));
	ptr->base.type = TYPE_PTR;
	ptr->type = baseType;

	return (struct object *)ptr;
}
struct object *objectArrayCreate(struct object *baseType,
                                 struct parserNode *dim) {
	struct objectArray *array = malloc(sizeof(struct objectArray));
	array->base.type = TYPE_ARRAY;
	array->dim = dim;
	array->type = baseType;

	return (struct object *)array;
}
struct object *objectForwardDeclarationCreate(const char *name) {
	struct objectForwardDeclaration *retVal =
	    malloc(sizeof(struct objectForwardDeclaration));
	retVal->base.type = TYPE_FORWARD;
	retVal->name = strClone(name);

	return (struct object *)retVal;
}
MAP_TYPE_DEF(struct object *,Object);
MAP_TYPE_FUNCS(struct object *,Object);
static __thread mapObject objectRegistry=NULL;
struct object typeBool ={TYPE_Bool};
struct object typeU0={TYPE_U0};
struct object typeU8i={TYPE_U8i};
struct object typeU16i={TYPE_U16i};
struct object typeU32i={TYPE_U32i};
struct object typeU64i={TYPE_U64i};
struct object typeI8i={TYPE_I8i};
struct object typeI16i={TYPE_I16i};
struct object typeI32i={TYPE_I32i};
struct object typeI64i={TYPE_U64i};
struct object typeF64={TYPE_F64};
static void initObjectRegistry() __attribute__((constructor));
static void destroyObjectRegistry() __attribute__((destructor));
static void destroyObjectRegistry() {
 mapObjectDestroy(objectRegistry,(void(*)(void*))objectDestroy);
}
static void initObjectRegistry() {
 objectRegistry=mapObjectCreate();
 /**
	* Register intristic types
	*/
 mapObjectInsert(objectRegistry,"Bool",&typeBool);
 mapObjectInsert(objectRegistry,"U0",&typeU0);
 mapObjectInsert(objectRegistry,"U8i",&typeU8i);
 mapObjectInsert(objectRegistry,"U16i",&typeU16i);
 mapObjectInsert(objectRegistry,"U32i",&typeU32i);
 mapObjectInsert(objectRegistry,"U64i",&typeU64i);
 mapObjectInsert(objectRegistry,"I8i",&typeI8i);
 mapObjectInsert(objectRegistry,"I16i",&typeI16i);
 mapObjectInsert(objectRegistry,"I32i",&typeI32i);
 mapObjectInsert(objectRegistry,"I64i",&typeI64i);
 mapObjectInsert(objectRegistry,"F64",&typeF64);
}
struct object *objectByName(const char *name) {
	return NULL;
}
struct object *objectFuncCreate(struct object *retType, strFuncArg args) {
	struct objectFunction func;
	func.base.type = TYPE_FUNCTION;
	func.args = strFuncArgAppendData(NULL, args, strFuncArgSize(args));
	func.retType = retType;

	void *retVal=malloc(sizeof(struct objectFunction));
	memcpy(retVal,&func,sizeof(struct objectFunction));
	return retVal;
}
