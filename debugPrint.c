#include <debugPrint.h>
#include <hashTable.h>
#include <base64.h>
#include <string.h>
MAP_TYPE_DEF(char *,Str);
MAP_TYPE_FUNCS(char *,Str);
static __thread mapStr ptrNames=NULL;
static char *ptr2Str(const void *ptr) {
		return base64Enc((void*)&ptr, sizeof(ptr));
}
static void init() __attribute__((constructor));
static void init() {
		ptrNames=mapStrCreate();
}
static void free2(void *ptrPtr) {
		free(*(void**)ptrPtr);
}
static void deinit() __attribute__((destructor));
static void deinit() {
		mapStrDestroy(ptrNames, free2);
}
static char *strClone(const char *str) {
		char *retVal=malloc(strlen(str)+1);
		strcpy(retVal, str);

		return retVal;
}
void debugRemovePtrName(const void *a) {
		char *key=ptr2Str(a);
		if(NULL!=mapStrGet(ptrNames, key))
					mapStrRemove(ptrNames, key, free2);
		
		free(key);
}
void debugAddPtrName(const void *a,const char *text) {
		char *key=ptr2Str(a);

		debugRemovePtrName(a);
		mapStrInsert(ptrNames, key, strClone(text));
				
		free(key);
}
char *debugGetPtrName(const void *a) {
		char *key=ptr2Str(a);

		char *retVal=NULL;
		__auto_type find=mapStrGet(ptrNames, key);
		if(find)
				retVal=strClone(*find);
		
		free(key);

		return retVal;
}
