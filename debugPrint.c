#include <base64.h>
#include <debugPrint.h>
#include <hashTable.h>
#include <string.h>
MAP_TYPE_DEF(char *, Str);
MAP_TYPE_FUNCS(char *, Str);
static  mapStr ptrNames  = NULL;
static char *ptr2Str(const void *ptr) {
	return base64Enc((void *)&ptr, sizeof(ptr));
}
 void initDebugPrint();
void initDebugPrint() { ptrNames = mapStrCreate(); }
static char *strClone(const char *str) {
	char *retVal = malloc(strlen(str) + 1);
	strcpy(retVal, str);

	return retVal;
}
void debugRemovePtrName(const void *a) {
	char *key = ptr2Str(a);
	if (NULL != mapStrGet(ptrNames, key))
		mapStrRemove(ptrNames, key, NULL);
}
void debugAddPtrName(const void *a, const char *text) {
	char *key = ptr2Str(a);

	debugRemovePtrName(a);
	mapStrInsert(ptrNames, key, strClone(text));
}
const char *debugGetPtrNameConst(const void *a) {
	char *key = ptr2Str(a);

	const char *retVal = NULL;
	__auto_type find = mapStrGet(ptrNames, key);
	if (find)
		retVal = *find;

	return retVal;
}
char *debugGetPtrName(const void *a) {
	char *key = ptr2Str(a);

	char *retVal = NULL;
	__auto_type find = mapStrGet(ptrNames, key);
	if (find)
		retVal = strClone(*find);

	return retVal;
}
