#pragma once
struct __ptrMap;
void __ptrMapAdd(struct __ptrMap *map,const void *data,const void *key,long dataSize);
void __ptrMapRemove(struct __ptrMap *map,const void * key);
void *__ptrMapGet(struct __ptrMap *map,const void * key);
void __ptrMapDestroy(struct __ptrMap *map,void(*destroy)(void*));
long __ptrMapSize(struct __ptrMap *map);
void __ptrMapKeys(const struct __ptrMap *map,void **dumpTo);
struct __ptrMap *__ptrMapCreate();
#define PTR_MAP_DEF(suffix) typedef struct __ptrMap *ptrMap##suffix;
#define PTR_MAP_FUNCS(inPtrType,type,suffix)																												\
		PTR_MAP_DEF(suffix)																																																			\
				__attribute__((always_inline)) inline void ptrMap##suffix##Add(struct __ptrMap *map,inPtrType key,type data) {__ptrMapAdd(map, &data,key, sizeof(data));} \
		__attribute__((always_inline)) inline void ptrMap##suffix##Remove(struct __ptrMap *map,inPtrType key) {__ptrMapRemove(map, key);} \
		__attribute__((always_inline)) inline type *ptrMap##suffix##Get(struct __ptrMap *map,inPtrType key) {return __ptrMapGet(map, key); } \
		__attribute__((always_inline)) inline void ptrMap##suffix##Destroy(struct __ptrMap *map,void(*destroy)(void*)) { __ptrMapDestroy(map, destroy);  } \
		__attribute__((always_inline)) inline struct __ptrMap *ptrMap##suffix##Create() {return __ptrMapCreate();}; \
		__attribute__((always_inline)) inline long ptrMap##suffix##Size(struct __ptrMap *map) {return __ptrMapSize(map);} \
		__attribute__((always_inline)) inline void ptrMap##suffix##Keys(struct __ptrMap *map,inPtrType *dumpTo) {return __ptrMapKeys(map,(void**)dumpTo);}
