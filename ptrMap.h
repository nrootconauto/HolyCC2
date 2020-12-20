#pragma once
struct __ptrMap;
void __ptrMapAdd(struct __ptrMap *map,const void *data,const void *key,long dataSize);
void __ptrMapRemove(struct __ptrMap *map,const void * key);
void *__ptrMapGet(struct __ptrMap *map,const void * key);
void __ptrMapDestroy(struct __ptrMap *map,void(*destroy)(void*));
#define PTR_MAP_FUNCS(inPtrType,type,suffix)											\
		__attribute__((always_inline)) inline void ptrMap##suffix##Add(struct __ptrMap *map,inPtrType key,type data) {__ptrMapAdd(map, key, &data, sizeof(data));} \
		__attribute__((always_inline)) inline void ptrMap##suffix##Remove(struct __ptrMap *map,inPtrType key) {__ptrMapRemove(map, key);} \
		__attribute__((always_inline)) inline type *ptrMap##suffix##Get(struct __ptrMap *map,inPtrType key) {return __ptrMapGet(map, key); } \
		__attribute__((always_inline)) inline void ptrMap##suffix##Destroy(struct __ptrMap *map,void(*destroy)(void*)) { __ptrMapDestroy(map, destroy);  }
PTR_MAP_FUNCS(void*,int,Int);
