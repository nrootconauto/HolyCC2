#include <hashTable.h>
#include <parserA.h>
#include <object.h>
#include <string.h>
#include <assert.h>
MAP_TYPE_DEF(struct object *, Object);
MAP_TYPE_FUNCS(struct object *, Object);
static __thread mapObject objectRegistry = NULL;
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
		case TYPE_FORWARD:
		case TYPE_FUNCTION:
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
		case TYPE_Bool:
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
		case TYPE_F64:
		case TYPE_I64i:
		case TYPE_U64i: {
				return 8;
		}
		}
}
void objectMemberAttrDestroy(struct objectMemberAttr *attr) {
		free(attr->name);
		// TODO free expression
}
void objectMemberDestroy(struct objectMember *member) {
		free(member->name);
		for (long i = 0; i != strObjectMemberAttrSize(member->attrs); i++)
				objectMemberAttrDestroy(&member->attrs[i]);

		strObjectMemberAttrDestroy(&member->attrs);
}
void objectDestroy(struct object **type) {
		struct object *type2 = *type;
		switch (type2->type) {
		case TYPE_CLASS: {
				__auto_type item = (struct objectClass *)type2;
				free(item->name);
				for (long i = 0; i != strObjectMemberSize(item->members); i++)
						objectMemberDestroy(&item->members[i]);
				free(type2);
				return;
		}
		case TYPE_UNION: {
				__auto_type item = (struct objectUnion *)type2;
				free(item->name);
				for (long i = 0; i != strObjectMemberSize(item->members); i++)
						objectMemberDestroy(&item->members[i]);

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
				return 8;
		}
		}
		assert(0);
		return 0;
}
struct object *objectClassCreate(const struct parserNode *name,
                                 const struct objectMember *members,
                                 long count) {
		struct objectClass *newClass = malloc(sizeof(struct objectClass));
		newClass->name = (struct parserNode*)name;
		newClass->base.type = TYPE_CLASS;
		newClass->base.link = 0;
		newClass->methods=NULL;
		newClass->members = NULL;

		long largestMemberAlign = 0;
		int success;
		for (long i = 0; i != count; i++) {
				__auto_type align = objectAlign(members[i].type, &success);
				if (!success)
						goto fail;

				if (align > largestMemberAlign)
						largestMemberAlign = align;
		}

		newClass->align = largestMemberAlign;

		newClass->members = NULL;
		long offset = 0;
		for (long i = 0; i != count; i++) {
				offset += objectAlign(members[i].type, &success);
				if (!success)
						goto fail;

				newClass->members =
		    strObjectMemberAppendItem(newClass->members, members[i]);
		}
		if (offset % largestMemberAlign)
				newClass->size =
		    offset + largestMemberAlign - (offset % largestMemberAlign);

		
		
		if (name) {
				const char *name2=((struct parserNodeName*)name)->text;
				if (NULL == mapObjectGet(objectRegistry, name2))
						mapObjectInsert(objectRegistry, name2, (struct object *)newClass);
				else
						goto fail;
		}

		return (struct object *)newClass;
	fail:
		objectDestroy((struct object **)&newClass);
		return NULL;
}
struct object *objectUnionCreate(const struct parserNode *name,
                                 const struct objectMember *members,
                                 long count) {
		int success;

		struct objectUnion *newUnion = malloc(sizeof(struct objectUnion));
		newUnion->name = (struct parserNode*)name;
		newUnion->base.type = TYPE_CLASS;
		newUnion->base.link = 0;
		newUnion->members = NULL;

		long largestMemberAlign = 0;
		long largestSize = 0;
		for (long i = 0; i != count; i++) {
				struct objectMember clone = members[i];
				clone.offset = 0;

				__auto_type align = objectAlign(members[i].type, &success);
				if (!success)
						goto fail;
				__auto_type size = objectSize(members[i].type, &success);
				if (!success)
						goto fail;

				if (align > largestMemberAlign)
						largestMemberAlign = align;
				if (size > largestSize)
						largestSize = size;

				clone.name = strClone(clone.name);
				newUnion->members =
		    strObjectMemberAppendItem(newUnion->members, members[i]);
		}
		largestSize += largestSize % largestMemberAlign;
		newUnion->size = largestSize;
		newUnion->align = largestMemberAlign;

		if (name) {
				const char *name2=((struct parserNodeName *)name)->text;
				if (NULL == mapObjectGet(objectRegistry, name2))
						mapObjectInsert(objectRegistry, name2, (struct object *)newUnion);
				else
						goto fail;
		}

		return (struct object *)newUnion;
	fail:
		objectDestroy((struct object **)&newUnion);
		return NULL;
}
struct object *objectPtrCreate(struct object *baseType) {
		struct objectPtr *ptr = malloc(sizeof(struct objectPtr));
		ptr->base.link = 0;
		ptr->base.type = TYPE_PTR;
		ptr->type = baseType;

		return (struct object *)ptr;
}
struct object *objectArrayCreate(struct object *baseType,
                                 struct parserNode *dim) {
		struct objectArray *array = malloc(sizeof(struct objectArray));
		array->base.type = TYPE_ARRAY;
		array->base.link = 0;
		array->dim = dim;
		array->type = baseType;

		return (struct object *)array;
}
struct object *objectForwardDeclarationCreate(const struct parserNode *name,enum holyCTypeKind type) {
		struct objectForwardDeclaration *retVal =
				malloc(sizeof(struct objectForwardDeclaration));
		retVal->base.type = TYPE_FORWARD;
		retVal->base.link = 0;
		retVal->name = (struct parserNode*)name;
		retVal->type=type;

		return (struct object *)retVal;
}
struct object typeBool = {TYPE_Bool};
struct object typeU0 = {TYPE_U0};
struct object typeU8i = {TYPE_U8i};
struct object typeU16i = {TYPE_U16i};
struct object typeU32i = {TYPE_U32i};
struct object typeU64i = {TYPE_U64i};
struct object typeI8i = {TYPE_I8i};
struct object typeI16i = {TYPE_I16i};
struct object typeI32i = {TYPE_I32i};
struct object typeI64i = {TYPE_I64i};
struct object typeF64 = {TYPE_F64};
static void initObjectRegistry() __attribute__((constructor));
static void destroyObjectRegistry() __attribute__((destructor));
static void destroyObjectRegistry() {
		mapObjectDestroy(objectRegistry, (void (*)(void *))objectDestroy);
}
static void initObjectRegistry() {
		objectRegistry = mapObjectCreate();
		/**
			* Register intristic types
			*/
		mapObjectInsert(objectRegistry, "Bool", &typeBool);
		mapObjectInsert(objectRegistry, "U0", &typeU0);
		mapObjectInsert(objectRegistry, "U8i", &typeU8i);
		mapObjectInsert(objectRegistry, "U16i", &typeU16i);
		mapObjectInsert(objectRegistry, "U32i", &typeU32i);
		mapObjectInsert(objectRegistry, "U64i", &typeU64i);
		mapObjectInsert(objectRegistry, "I8i", &typeI8i);
		mapObjectInsert(objectRegistry, "I16i", &typeI16i);
		mapObjectInsert(objectRegistry, "I32i", &typeI32i);
		mapObjectInsert(objectRegistry, "I64i", &typeI64i);
		mapObjectInsert(objectRegistry, "F64", &typeF64);
}
struct object *objectByName(const char *name) {
		__auto_type find = mapObjectGet(objectRegistry, name);
		if (find == NULL)
				return NULL;

		return *find;
}
struct object *objectFuncCreate(struct object *retType, strFuncArg args) {
		struct objectFunction func;
		func.base.link = 0;
		func.base.type = TYPE_FUNCTION;
	func.args = strFuncArgAppendData(NULL, args, strFuncArgSize(args));
	func.retType = retType;

	void *retVal = malloc(sizeof(struct objectFunction));
	memcpy(retVal, &func, sizeof(struct objectFunction));
	return retVal;
}
void strFuncArgDestroy2(strFuncArg *args) {
	for (long i = 0; i != strFuncArgSize(*args); i++) {
		parserNodeDestroy(&args[0][i].dftVal);
		parserNodeDestroy(&args[0][i].name);
	}
	strFuncArgDestroy(args);
}
char *object2Str(struct object *obj) {
		return NULL;
}
