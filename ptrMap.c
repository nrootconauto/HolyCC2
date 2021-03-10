#include <linkedList.h>
#include <ptrMap.h>
#include <stdlib.h>
#include <str.h>
STR_TYPE_DEF(struct __ll *, LL);
STR_TYPE_FUNCS(struct __ll *, LL);
STR_TYPE_DEF(long, Long);
STR_TYPE_FUNCS(long, Long);
struct __ptrMap {
	strLL buckets;
	strLong bucketSizes;
	long size;
};
long __ptrMapSize(struct __ptrMap *map) {
	return map->size;
}
static long __ptrMapHash(const struct __ptrMap *map, const void *ptr) {
	return (((unsigned long)ptr >> 3u) / 3u) % strLLSize(map->buckets);
}
static int ptrPtrCmp(const void *a, const void *b) {
		return *(void**)a-*(void**)b;
}
static int llPtrInsertPred(const void *a, const void *b) {
	return ptrPtrCmp(a, b);
}
static int llPtrGetPred(const void *a, const void *b) {
	return ptrPtrCmp(&a, b);
}
void __ptrMapDestroy(struct __ptrMap *map, void (*destroy)(void *)) {
	if (!map)
		return;
	if (destroy) {
		void *dumpTo[__ptrMapSize(map)];
		__ptrMapKeys(map, dumpTo);
		for (long i = 0; i != __ptrMapSize(map); i++)
			destroy(__ptrMapGet(map, dumpTo[i]));
	}

	for (long i = 0; i != strLLSize(map->buckets); i++)
		__llDestroy(map->buckets[i], NULL);

	strLLDestroy(&map->buckets);
	strLongDestroy(&map->bucketSizes);
}
static int __ptrMapNeedsRehash(struct __ptrMap *map) {
	long total = 0;
	for (long i = 0; i != strLongSize(map->bucketSizes); i++)
		total += map->bucketSizes[i];

	if (total / strLongSize(map->bucketSizes) >= 32)
		return 1;

	return 0;
}
static void __ptrMapRehash(struct __ptrMap *map, long newBuckets, long dataSize) {
	struct __ptrMap new;
	new.bucketSizes = strLongResize(NULL, newBuckets);
	new.buckets = strLLResize(NULL, newBuckets);
	new.size = 0;

	for (long i = 0; i != newBuckets; i++) {
		new.buckets[i] = NULL;
		new.bucketSizes[i] = 0;
	}

	for (long i = 0; i != strLLSize(map->buckets); i++) {
		for (__auto_type node = __llGetFirst(map->buckets[i]); node != NULL; node = __llNext(node)) {
			__ptrMapAdd(&new, __ptrMapGet(map, *(const void **)__llValuePtr(node)), *(void **)__llValuePtr(node), dataSize);
		}
	}

	__ptrMapDestroy(map, NULL);
	*map = new;
}
void __ptrMapAdd(struct __ptrMap *map, const void *data, const void *key, long dataSize) {
	char buffer[dataSize + sizeof(void *)];
	__auto_type ll = __llCreate(buffer, dataSize + sizeof(void *));
	*(const void **)__llValuePtr(ll) = key;
	memcpy(__llValuePtr(ll) + sizeof(void *), data, dataSize);

	__auto_type bucketI = __ptrMapHash(map, key);
	__llInsert(map->buckets[bucketI], ll, llPtrInsertPred);
	map->buckets[bucketI] = ll;

	map->bucketSizes[bucketI]++;
	map->size++;

	if (__ptrMapNeedsRehash(map))
		__ptrMapRehash(map, strLLSize(map->buckets) * 3, dataSize);
}
void *__ptrMapGet(struct __ptrMap *map, const void *key) {
	__auto_type bucketI = __ptrMapHash(map, key);
	__auto_type find = __llFind(map->buckets[bucketI], key, llPtrGetPred);
	if (!find)
		return NULL;

	return sizeof(void *) + __llValuePtr(find);
}
void __ptrMapRemove(struct __ptrMap *map, const void *key) {
	__auto_type bucketI = __ptrMapHash(map, key);
	__auto_type find = __llFind(map->buckets[bucketI], key, llPtrGetPred);
	if (!find)
		return;

	map->buckets[bucketI] = __llRemoveNode(find);
	map->bucketSizes[bucketI]--;
	map->size--;
}
struct __ptrMap *__ptrMapCreate() {
	struct __ptrMap retVal;
	retVal.bucketSizes = NULL;
	retVal.buckets = NULL;
	retVal.size = 0;

	__ptrMapRehash(&retVal, 4, 0);

	struct __ptrMap *alloced = calloc(sizeof(retVal),1);
	*alloced = retVal;

	return alloced;
}
void __ptrMapKeys(const struct __ptrMap *map, void **dumpTo) {
	long count = 0;
	for (long i = 0; i != strLLSize(map->buckets); i++) {
		for (__auto_type node = __llGetFirst(map->buckets[i]); node != NULL; node = __llNext(node)) {
			dumpTo[count++] = *(void **)__llValuePtr(node);
		}
	}
}
