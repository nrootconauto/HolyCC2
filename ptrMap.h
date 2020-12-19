#pragma once
struct __ptrMap;
void __ptrMapAdd(struct __ptrMap *map,const void *data,const void *key,long dataSize);
void __ptrMapRemove(struct __ptrMap *map,const void * key);
void *__ptrMapGet(struct __ptrMap *map,const void * key);
void __ptrMapDestroy(struct __ptrMap *map,void(*destroy)(void*));
