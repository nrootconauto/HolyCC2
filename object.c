#include <hashTable.h>
#include <parserA.h>
#include <object.h>
#include <string.h>
#include <assert.h>
#include <base64.h>
#include <stdio.h>
MAP_TYPE_DEF(struct object *, Object);
MAP_TYPE_FUNCS(struct object *, Object);
STR_TYPE_DEF(char,Char);
STR_TYPE_FUNCS(char,Char);
static __thread mapObject objectRegistry = NULL;
static char *strClone(const char *str) {
		__auto_type len = strlen(str);
		char *retVal = malloc(len + 1);
		strcpy(retVal, str);
		return retVal;
}
static char* ptr2Str(const void *a) {
		return base64Enc((void*)&a, sizeof(a));
}

static const char *hashObject(struct object *obj,int *alreadyExists) {
		if(obj->name) {
				if(alreadyExists) *alreadyExists=1;
				return obj->name;
		}
		
		char *retVal=NULL;
		switch(obj->type) {
		case TYPE_U8i:
				retVal=strClone("U8i");goto end;
		case TYPE_U16i:
				retVal= strClone("U16i");goto end;
		case TYPE_U32i:
				retVal=strClone("U32i"); goto end;
		case TYPE_U64i:
				retVal=strClone("U64i"); goto end;
		case TYPE_I8i:
				retVal= strClone("I8i"); goto end;
		case TYPE_I16i:
				retVal= strClone("I16i"); goto end;
		case TYPE_I32i:
				retVal= strClone("I32i"); goto end;
		case TYPE_I64i:
				retVal=strClone("I64i"); goto end;
		case TYPE_ARRAY: {
				struct objectArray *arr=(void*)obj;
				const char *baseH =hashObject(obj, NULL);
				//If integer dim
				if(arr->dim->type==NODE_LIT_INT) {
						struct parserNodeLitInt *lint=(void*)arr->dim;
						long len=snprintf(NULL, 0, "%s[%lli]", baseH,(long long)lint->value.value.sLong);
						char buffer[len+1];
						sprintf(buffer, "%s[%lli]", baseH,(long long)lint->value.value.sLong);

						retVal=strClone(buffer);
						goto end;
				} else {
						//Isn't an int-dim
						__auto_type dimStr=ptr2Str(arr->dim);
						long len=snprintf(NULL, 0, "%s[%p]", baseH,arr->dim);
						char buffer[len+1];
						sprintf(buffer, "%s[%p]", baseH,arr->dim);

						free(dimStr);
						retVal= strClone(buffer);
						goto end;
				}
		}
		case TYPE_Bool: {
				retVal=strClone("Bool"); goto end;
		}
		case TYPE_FORWARD: {
				struct objectForwardDeclaration *fwd=(void*)obj;

				const char *typeStr=NULL;
				if(obj->type==TYPE_CLASS)
						typeStr="class";
				else if(obj->type==TYPE_UNION)
						typeStr="union";
				
				struct parserNodeName *name=(void*)fwd->name;
				long len=snprintf(NULL, 0, "FWD(%s):%s", typeStr,name->text);
				char buffer[len+1];
				sprintf(buffer, "FWD(%s):%s", typeStr,name->text);

				retVal=strClone(buffer); goto end;
		}
		case TYPE_CLASS:
				retVal=ptr2Str(obj); goto end;
		case TYPE_F64:
				retVal=strClone("F64"); goto end;
		case TYPE_FUNCTION: {
				struct objectFunction *func=(void*)obj;
				__auto_type retType=hashObject(func->retType, NULL);
				
				strChar argStr=NULL;
				for(long i=0;i!=strFuncArgSize(func->args);i++) {
						__auto_type dftValStr=ptr2Str(func->args[i].dftVal);

						const char *argName=NULL;
						struct parserNodeName *name=(void*)func->args[i].name;
						if(name)
								if(name->base.type==NODE_NAME)
										argName=name->text;
						
						long len=snprintf(NULL,0,"%s %s=%s",hashObject(func->args[i].type, NULL),argName,dftValStr);
						char buffer[len+1];
						sprintf(buffer,"%s %s=%s",hashObject(func->args[i].type, NULL),argName,dftValStr);

						argStr=strCharAppendData(argStr, buffer, strlen(buffer));
				}

				long len=snprintf(NULL, 0, "%s(*)(%s)",retVal,argStr);
				char buffer[len+1];
				sprintf(buffer, "%s(*)(%s)",retVal,argStr);
				
				strCharDestroy(&argStr);
				retVal=strClone(buffer); goto end;
		}
		case TYPE_PTR:  {
				struct objectPtr *ptr=(void*)obj;
				const char *base=hashObject(ptr->type, NULL);

				long len=snprintf(NULL, 0, "%s*", base);
				char buffer[len+1];
				sprintf(buffer,"%s*", base);

				retVal=strClone(buffer); goto end;
		}
		case TYPE_U0:
				retVal=strClone("U0"); goto end;
		case TYPE_UNION:
				retVal=ptr2Str(obj); goto end;
		}
	end:
		if(alreadyExists) *alreadyExists=0;
		
		obj->name=retVal;
		return obj->name;
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

		hashObject((void*)newClass, NULL);
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

		hashObject((void*)newUnion, NULL);
		return (struct object *)newUnion;
	fail:
		objectDestroy((struct object **)&newUnion);
		return NULL;
}
struct object *objectPtrCreate(struct object *baseType) {
		//Check if item is in registry prior to making a new one
		
		struct objectPtr *ptr = malloc(sizeof(struct objectPtr));
		ptr->base.link = 0;
		ptr->base.type = TYPE_PTR;
		ptr->type = baseType;
		ptr->base.name=NULL;

		int alreadyExists;
		__auto_type hash=hashObject((void*)ptr, &alreadyExists);
		if(alreadyExists)
				objectDestroy((void*)&ptr);
		
		return *mapObjectGet(objectRegistry, hash);
}
struct object *objectArrayCreate(struct object *baseType,
                                 struct parserNode *dim) {
		struct objectArray *array = malloc(sizeof(struct objectArray));
		array->base.type = TYPE_ARRAY;
		array->base.link = 0;
		array->dim = dim;
		array->type = baseType;

		int alreadyExists;
		__auto_type key=hashObject((void*)array, &alreadyExists);
		if(alreadyExists)
				objectDestroy((void*)&array);
		
		return *mapObjectGet(objectRegistry, key);
}
struct object *objectForwardDeclarationCreate(const struct parserNode *name,enum holyCTypeKind type) {
		struct objectForwardDeclaration *retVal =
				malloc(sizeof(struct objectForwardDeclaration));
		retVal->base.type = TYPE_FORWARD;
		retVal->base.link = 0;
		retVal->name = (struct parserNode*)name;
		retVal->type=type;

		int alreadyExists;
		__auto_type hash=hashObject((void*)retVal, &alreadyExists);
		if(alreadyExists)
				objectDestroy((void*)&retVal);
		
		return *mapObjectGet(objectRegistry, hash);
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
		
		//hashObject assigns name to type
		typeBool.type=TYPE_Bool;
		typeBool.name=NULL;
		hashObject(&typeBool, NULL);
		
		typeU0.type=TYPE_U0;
		typeU0.name=NULL;
		hashObject(&typeU0, NULL);

		typeU8i.type=TYPE_U8i;
		typeU8i.name=NULL;
		hashObject(&typeU8i,NULL);

		typeU16i.type=TYPE_U16i;
		typeU16i.name=NULL;
		hashObject(&typeU16i,NULL);

		typeU32i.type=TYPE_U32i;
		typeU32i.name=NULL;
		hashObject(&typeU32i,NULL);

		typeU64i.type=TYPE_U64i;
		typeU64i.name=NULL;
		hashObject(&typeU64i,NULL);

		typeI8i.type=TYPE_I8i;
		typeI8i.name=NULL;
		hashObject(&typeI8i,NULL);

		typeI16i.type=TYPE_I16i;
		typeI16i.name=NULL;
		hashObject(&typeI16i,NULL);

		typeI32i.type=TYPE_I32i;
		typeI32i.name=NULL;
		hashObject(&typeI32i,NULL);

		typeI64i.type=TYPE_I64i;
		typeI64i.name=NULL;
		hashObject(&typeI64i,NULL);

		typeF64.type=TYPE_F64;
		typeF64.name=NULL;
		hashObject(&typeF64,NULL);
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

	int alreadyExists;
	__auto_type key=hashObject((void*)retVal, &alreadyExists);
	if(alreadyExists)
			objectDestroy((void*)&retVal);
	
	return *mapObjectGet(objectRegistry, key);
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
int objectEqual(const struct object *a,const struct object *b) {
		if(a->type!=b->type)
				return 0;
		if(a->type==TYPE_PTR) {
				struct objectPtr *aBase=(void*)a,*bBase=(void*)b;
				return objectEqual(aBase->type, bBase->type);
		} else if(a->type==TYPE_CLASS) {
				return a==b;
		} else if (a->type==TYPE_UNION) {
				return a==b;
		} else if(a->type==TYPE_UNION) {
				return a==b;
		} else if(a->type==TYPE_FUNCTION) {
				struct objectFunction *aFunc=(void*)a,*bFunc=(void*)b;
				if(strFuncArgSize(aFunc->args)!=strFuncArgSize(bFunc->args))
						return 0;

				for(long i=0;i!=strFuncArgSize(aFunc->args);i++) {
						if(!objectEqual(aFunc->args[i].type, bFunc->args[i].type))
								return 0;

						return 1;
				}
		} else if(a->type==TYPE_ARRAY) {
				struct objectArray *aArr=(void*)a,*bArr=(void*)b;

				//Only checks constant dim
				if(aArr->dim->type==NODE_LIT_INT) {
						if(bArr->dim->type==NODE_LIT_INT) {
								struct parserNodeLitInt *aInt=(void*)aArr->dim;
								struct parserNodeLitInt *bInt=(void*)bArr->dim;
								if(aInt->value.value.sInt==bInt->value.value.sInt)
										return 1;
						}
				}

				return 0;
		}

				return 1;
}
static int isArith(const struct object *type) {
		if(
					type==&typeU8i
					||type==&typeU16i
					||type==&typeU32i
					||type==&typeU64i
					||type==&typeI8i
					||type==&typeI16i
					||type==&typeI32i
					||type==&typeI64i
					||type==&typeF64
					||type->type==TYPE_PTR
					||type->type==TYPE_ARRAY
					) {
				return 1;
		}
		return 0;
}
int objectIsCompat(const struct object *a,const struct object *b) {
		if(objectEqual(a, b))
				return 1;
		return isArith(a)&&isArith(b);
}
