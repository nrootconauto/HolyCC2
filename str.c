#include <stdlib.h>
#include <str.h>
#include <string.h>
/**
 * struct __vec {
 *     long capacity;
 *     long size;
 *     //Data starts here
 * }
 */
struct __vec;
static long *__vecCapacityPtr(const struct __vec *vec) {
	return (void *)vec - 2 * sizeof(long);
}
static long *__vecSizePtr(const struct __vec *vec) {
	return (void *)vec - sizeof(long);
}
void __vecDestroy(struct __vec *a) {
	if (a != NULL)
		free((void *)a - 2 * sizeof(long));
}
struct __vec *__vecResize(struct __vec *a, long size) {
	if (a == NULL) {
		a = malloc(2 * sizeof(long));
		memset(a, 0, 2 * sizeof(long));
		if (a == NULL)
			return NULL;
		a = (void *)a + 2 * sizeof(long);
	}
	//
	if (size == 0) {
		__vecDestroy(a);
		return NULL;
	}
	//
	if (*__vecCapacityPtr(a) > size) {
	} else {
		a = realloc((void *)a - 2 * sizeof(long), 2 * sizeof(long) + size);
		a = (a == NULL) ? NULL : (void *)a + 2 * sizeof(long);
		*__vecCapacityPtr(a) = size;
	}
	*__vecSizePtr(a) = size;
	return a;
}
long __vecSize(const struct __vec *a) {
	if (a == NULL)
		return 0;
	else
		return *__vecSizePtr(a);
}
long __vecCapacity(const struct __vec *a) {
	if (a == NULL)
		return 0;
	else
		return *__vecCapacityPtr(a);
}
struct __vec *__vecConcat(struct __vec *a, const struct __vec *b) {
	__auto_type oldASize = __vecSize(a);
	long totalSize = __vecSize(a) + __vecSize(b);
	a = __vecResize(a, totalSize);
	memcpy((void *)a + oldASize, b, totalSize);
	return a;
}
struct __vec *__vecReserve(struct __vec *a, long capacity) {
	if (a == NULL) {
		a = malloc(2 * sizeof(long));
		memset(a, 0, 2 * sizeof(long));
		if (a == NULL)
			return NULL;
		a = (void *)a + 2 * sizeof(long);
	}
	//
	if (capacity == 0) {
		__vecDestroy(a);
		return NULL;
	}
	//
	a = realloc((void *)a - 2 * sizeof(long), 2 * sizeof(long) + capacity);
	a = (a == NULL) ? NULL : (void *)a + 2 * sizeof(long);
	if (a == NULL)
		return NULL;
	*__vecCapacityPtr(a) = capacity;
	return a;
}
struct __vec *__vecAppendItem(struct __vec *a, const void *item,
                              long itemSize) {
	__auto_type capacity = __vecCapacity(a);
	__auto_type size = __vecSize(a);
	if (capacity < size + itemSize) {
		__auto_type newCap = (capacity + itemSize) * 4;
		a = __vecReserve(a, newCap);
	}
	if (a == NULL)
		return a;

	memcpy((void *)a + size, item, itemSize);
	*__vecSizePtr(a) += itemSize;
	return a;
}
struct __vec *__vecSortedInsert(struct __vec *a, const void *item,
                                long itemSize,
                                int predicate(const void *, const void *)) {
	__auto_type oldSize = __vecSize(a);
	a = __vecAppendItem(a, item, itemSize); // increase size by one
	for (int i = 0; i != oldSize; i += itemSize) {
		__auto_type where = (void *)a + i;
		__auto_type result = predicate(item, where);
		if (result <= 0) {
			memmove(where + itemSize, where, oldSize - i);
			memcpy(where, item, itemSize);
			break;
		}
	}
	return a;
}
// https://cplusplus.com/reference/algorithm/set_difference/
struct __vec *__vecSetDifference(struct __vec *a, const struct __vec *b,
                                 long itemSize,
                                 int (*pred)(const void *, const void *)) {
	__auto_type aEnd = (void *)a + __vecSize(a);
	void *aStart = a;
	__auto_type bEnd = (void *)b + __vecSize(b);
	const void *bStart = b;
	void *result = aStart;
	while (aStart != aEnd && bStart != bEnd) {
		if (pred(aStart, bStart) < 0) {
			memmove(result, aStart, itemSize);
			aStart += itemSize;
			result += itemSize;
		} else if (pred(bStart, aStart) < 0) {
			bStart += itemSize;
		} else {
			aStart += itemSize;
			bStart += itemSize;
		}
	}
	memcpy(result, aStart, aEnd - aStart);
	result += aEnd - aStart;
	a = __vecResize(a, result - (void *)a);
	return a;
}
void *__vecSortedFind(const struct __vec *a, const void *item, long itemSize,
                      int predicate(const void *, const void *)) {
	__auto_type oldSize = __vecSize(a);
	for (int i = 0; i != oldSize; i += itemSize) {
		__auto_type where = (void *)a + i;
		__auto_type result = predicate(item, where);
		if (result < 0) {
			break;
		} else if (result == 0) {
			return where;
		}
	}
	return NULL;
}
// https://www.cplusplus.com/reference/algorithm/remove_if/
struct __vec *__vecRemoveIf(struct __vec *a, long itemSize,
                            int predicate(const void *, const void *),
                            const void *data) {
	__auto_type size = __vecSize(a);
	void *first = a, *last = (void *)a + size;
	__auto_type result = first;
	while (first != last) {
		if (!predicate(first, data)) {
			memcpy(result, first, itemSize);
			result += itemSize;
		}
		first += itemSize;
	}
	*__vecSizePtr(a) = (char *)result - (char *)a;
	return a;
}
