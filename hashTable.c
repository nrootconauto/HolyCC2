#include <assert.h>
#include <hashTable.h>
#include <linkedList.h>
#include <stdio.h>
#include <str.h>
#include <string.h>
STR_TYPE_DEF(int, Int);
STR_TYPE_FUNCS(int, Int);
STR_TYPE_DEF(struct __ll *, LLP);
STR_TYPE_FUNCS(struct __ll *, LLP);
struct __map {
	strLLP buckets; // Bucket has form (itemSize(long),hash(int),itemValue,key)
	strInt bucketSizes;
};
static float __mapCalculateLoad(struct __map *map);
static void __mapRehash(struct __map *map, int scaleUp);
// https://algs4.cs.princeton.edu/34hash/
static int __mapHash(const char *key, const long buckets) {
	if (key == NULL)
		return 0;
	__auto_type len = strlen(key);
	int retVal = 0;
	for (int i = 0; i != len; i++) {
		retVal = (31 * retVal + key[i]) % buckets;
	}
	return retVal;
}
static struct __ll *__mapNodeCreate(const char *key, const void *item,
                                    const long itemSize, const int hash) {
	__auto_type itemSize2 = (item == NULL) ? 0 : itemSize;
	__auto_type strLen = strlen(key);
	__auto_type totalSize = strLen + itemSize2 + sizeof(long) + sizeof(int) + 1;
	char buffer[totalSize];
	*(long *)buffer = itemSize2;
	*(int *)(buffer + sizeof(long)) = hash;
	memcpy(buffer + sizeof(long) + sizeof(int), item, itemSize2);
	memcpy(buffer + sizeof(long) + itemSize2 + sizeof(int), key, strLen);
	buffer[sizeof(long) + sizeof(int) + itemSize2 + strLen] = '\0';
	return __llCreate(buffer, totalSize);
}
static char *__mapNodeKey(const void *nodeValue) {
	__auto_type data = nodeValue;
	data += sizeof(int) + sizeof(long) + *(long *)data;
	return (char *)data;
}
static int *__mapNodeHashValue(const void *nodeValue) {
	__auto_type data = nodeValue;
	data += sizeof(long);
	return (int *)data;
}
static void *__mapNodeValue(const void *nodeValue) {
	return (void *)nodeValue + sizeof(int) + sizeof(long);
}
static int __mapBucketInsertPred(const void *current, const void *item) {
	__auto_type res = __mapNodeHashValue(item) - __mapNodeHashValue(current);
	if (res != 0)
		return res;
	return strcmp(__mapNodeKey(item), __mapNodeKey(current));
}
struct __mapKeyValuePair {
	const char *key;
	int hash;
};
static int __mapBucketGetPred(const void *item, const void *current) {
	const struct __mapKeyValuePair *pair = item;
	__auto_type result = pair->hash - *__mapNodeHashValue(current);
	if (result != 0)
		return result;
	return strcmp(pair->key, __mapNodeKey(current));
}
void *__mapGet(const struct __map *map, const char *key) {
	__auto_type hash = __mapHash(key, strLLPSize(map->buckets));
	__auto_type bucket = map->buckets[hash % strLLPSize(map->buckets)];
	__auto_type first = __llGetFirst(bucket);
	struct __mapKeyValuePair pair = {key, hash};
	__auto_type res = __llFindRight(first, &pair, __mapBucketGetPred);
	if (res == NULL)
		return NULL;
	return __mapNodeValue(__llValuePtr(res));
}
int __mapInsert(struct __map *map, const char *key, const void *item,
                const long itemSize) {
	if (NULL != __mapGet(map, key))
		return -1;
	__auto_type hash = __mapHash(key, strLLPSize(map->buckets));
	__auto_type newNode = __mapNodeCreate(key, item, itemSize, hash);
	__auto_type bucketI = hash % strLLPSize(map->buckets);
	map->buckets[bucketI] =
	    __llInsert(map->buckets[bucketI], newNode, __mapBucketInsertPred);
	map->bucketSizes[hash % strLLPSize(map->buckets)]++;
	//
	__auto_type load = __mapCalculateLoad(map);
	if (load > 3.5) {
		__mapRehash(map, 1);
	} else if (load < 0.1) {
		__mapRehash(map, 0);
	}
	//
	return 0;
}
static void __mapBucketRehash(struct __ll *bucket, int newBucketCount,
                                        struct __ll **dumpToStart) {
	__auto_type first = __llGetFirst(bucket);
	for (__auto_type node = first; node != NULL; node = __llNext(node)) {
		__auto_type hash =
		    __mapHash(__mapNodeKey(__llValuePtr(node)), newBucketCount);
		*__mapNodeHashValue(__llValuePtr(node)) = hash;
		__llRemoveNode(node);
		*(dumpToStart++) = node;
	}
}
static void __mapBucketInsert(struct __map *map, int bucketIndex,struct __ll *node) {
		map->buckets[bucketIndex] =
		    __llInsert(map->buckets[bucketIndex], node, __mapBucketInsertPred);
		map->bucketSizes[bucketIndex]++;
}
static float __mapCalculateLoad(struct __map *map) {
	float filled = 0;
	float unfilled = 0;
	for (int i = 0; i != strIntSize(map->bucketSizes); i++) {
		if (map->buckets[i] == NULL)
			unfilled++;
		else
			filled++;
	}
	if (unfilled == 0)
		return 0;
	else
		return filled / unfilled;
}
static void __mapRehash(struct __map *map, int scaleUp) {
	//
	__auto_type oldBucketCount = strIntSize(map->bucketSizes);
	__auto_type newBucketCount =
	    (scaleUp) ? oldBucketCount * 4 : oldBucketCount / 4;
	if (newBucketCount < 8)
		newBucketCount = 8;
	//
	__auto_type count = 0;
	int bucketStarts[strIntSize(map->bucketSizes)];
	for (int i = 0; i != strIntSize(map->bucketSizes); i++) {
		bucketStarts[i] = count;
		count += map->bucketSizes[i];
	}
	struct __ll *rehashedNodes[count];
	for (int i = 0; i != strIntSize(map->bucketSizes); i++) {
		__mapBucketRehash(map->buckets[i], newBucketCount,
		                               rehashedNodes + bucketStarts[i]);
	}
	//
	strIntDestroy(&map->bucketSizes);
	strLLPDestroy(&map->buckets);
	map->bucketSizes = strIntResize(NULL, newBucketCount);
	map->buckets = strLLPResize(NULL, newBucketCount);
	for(long i=0;i!=newBucketCount;i++) {
	 map->buckets[i]=NULL;
	 map->bucketSizes [i]=0;
	}
	//
	for (int i1 = 0; i1 != count; i1++) {
		__auto_type ptr = rehashedNodes[i1];
		__auto_type bucket =
		    *__mapNodeHashValue(__llValuePtr(ptr)) % newBucketCount;
		__mapBucketInsert(map,bucket,ptr);
	}
}
void __mapDestroy(struct __map *map, void (*kill)(void *)) {
	for (int i = 0; i != strLLPSize(map->buckets); i++) {
		if (kill != NULL)
			for (__auto_type node2 = __llGetFirst(map->buckets[i]); node2 != NULL;
			     node2 = __llNext(node2))
				kill(__mapNodeValue(__llValuePtr(node2)));

		__llDestroy(map->buckets[i], NULL);
	}
	strIntDestroy(&map->bucketSizes);
	strLLPDestroy(&map->buckets);
}
struct __map *__mapCreate() {
	struct __map *retVal = malloc(sizeof(struct __map));
	retVal->bucketSizes = strIntResize(NULL, 8);
	retVal->buckets = strLLPResize(NULL, 8);
	for (int i = 0; i != 8; i++) {
		retVal->bucketSizes[i] = 0;
		retVal->buckets[i] = NULL;
	}
	return retVal;
}
void __mapRemove(struct __map *map, const char *key, void (*kill)(void *)) {
	__auto_type hash = __mapHash(key, strIntSize(map->bucketSizes));
	__auto_type bucket = hash % strIntSize(map->bucketSizes);
	struct __mapKeyValuePair pair = {key, hash};
	__auto_type node = __llFindRight(__llGetFirst(map->buckets[bucket]), &pair,
	                                 __mapBucketGetPred);
	if (node == NULL)
		return;
	map->buckets[bucket] = __llRemoveNode(node);
	if (kill != NULL)
		kill(__mapNodeValue(__llValuePtr(node)));
	__llDestroy(node, NULL);

	map->bucketSizes[bucket]--;
	__auto_type load = __mapCalculateLoad(map);
	if (load > 3.5) {
		__mapRehash(map, 1);
	} else if (load < 0.1) {
		__mapRehash(map, 0);
	}
}
const char *__mapKeyByPtr(const void *valuePtr) {
	return __mapNodeKey(valuePtr - sizeof(int) - sizeof(long));
}
struct __map *__mapClone(struct __map *map,
                         void (*cloneData)(void *, const void *),
                         long itemSize) {
	struct __map *retVal = __mapCreate();

	for (long i = 0; i != strLLPSize(map->buckets); i++) {
		for (__auto_type node = __llGetFirst(map->buckets[i]); node != NULL;
		     node = __llNext(node)) {
			__auto_type mapNode = __llValuePtr(node);

			char buffer[itemSize];
			if (cloneData != NULL) {
				cloneData(buffer, __mapNodeValue(mapNode));
			} else {
				memcpy(buffer, __mapNodeValue(mapNode), itemSize);
			}

			__mapInsert(retVal, __mapNodeKey(mapNode), buffer, itemSize);
		}
	}

	return retVal;
}
static int strCmp(const void *a, const void *b) {
	return strcmp(*(const char **)a, *(const char **)b);
}
void __mapKeys(const struct __map *map, const char **dumpTo, long *count) {
	long computedCount = 0;
	for (long i = 0; i != strLLPSize(map->buckets); i++) {
		computedCount += map->bucketSizes[i];
	}

	if (count != NULL)
		*count = computedCount;

	if (dumpTo != NULL) {
		const char *buffer[computedCount];
		long inserted = 0;
		for (long i = 0; i != strLLPSize(map->buckets); i++) {
			for (__auto_type node = __llGetFirst(map->buckets[i]); node != NULL;
			     node = __llNext(node)) {
				buffer[inserted++] = __mapNodeKey(__llValuePtr(node));
				assert(inserted <= computedCount);
			}
		}
		assert(inserted == computedCount);

		qsort(buffer, inserted, sizeof(const char *), strCmp);
		for (long i = 0; i != inserted; i++)
			dumpTo[i] = buffer[i];
	}
}
