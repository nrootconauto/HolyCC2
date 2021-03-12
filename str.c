#include <stdlib.h>
#include "str.h"
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
struct __vec *__vecResize(struct __vec *a, long size) {
	if (a == NULL) {
			a = calloc(2 * sizeof(long) + size,1);
		memset(a, 0, 2 * sizeof(long));
		if (a == NULL)
			return NULL;

		a = (void *)a + 2 * sizeof(long);
	}
	//
	if (size == 0) {
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
	memmove((void *)a + oldASize, b, __vecSize(b));
	return a;
}
struct __vec *__vecReserve(struct __vec *a, long capacity) {
	if (a == NULL) {
			a = calloc(2 * sizeof(long),1);
		memset(a, 0, 2 * sizeof(long));
		if (a == NULL)
			return NULL;

		a = (void *)a + 2 * sizeof(long);
	}
	//
	if (capacity == 0) {
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
struct __vec *__vecAppendItem(struct __vec *a, const void *item, long itemSize) {
	__auto_type capacity = __vecCapacity(a);
	__auto_type size = __vecSize(a);
	if (capacity < size + itemSize) {
		__auto_type newCap = (capacity + itemSize) * 4;
		a = __vecReserve(a, newCap);
	}
	if (a == NULL)
		return a;

	memmove((void *)a + size, item, itemSize);
	*__vecSizePtr(a) += itemSize;
	return a;
}
struct __vec *__vecSortedInsert(struct __vec *a, const void *item, long itemSize, int predicate(const void *, const void *)) {
	__auto_type oldSize = __vecSize(a);
	a = __vecAppendItem(a, item, itemSize); // increase size by one
	for (int i = 0; i != oldSize; i += itemSize) {
		__auto_type where = (void *)a + i;
		__auto_type result = predicate(item, where);
		if (result <= 0) {
			memmove(where + itemSize, where, oldSize - i);
			memmove(where, item, itemSize);
			break;
		}
	}
	return a;
}
// https://cplusplus.com/reference/algorithm/set_difference/
struct __vec *__vecSetDifference(struct __vec *a, const struct __vec *b, long itemSize, int (*pred)(const void *, const void *)) {
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
	memmove(result, aStart, aEnd - aStart);
	result += aEnd - aStart;
	a = __vecResize(a, result - (void *)a);
	return a;
}
void *__vecSortedFind(const struct __vec *a, const void *item, long itemSize, int predicate(const void *, const void *)) {
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
struct __vec *__vecRemoveIf(struct __vec *a, long itemSize, int predicate(const void *, const void *), const void *data) {
	if (a == NULL)
		return NULL;

	__auto_type size = __vecSize(a);
	void *first = a, *last = (void *)a + size;
	__auto_type result = first;
	while (first != last) {
		if (!predicate(data, first)) {
			memmove(result, first, itemSize);
			result += itemSize;
		}
		first += itemSize;
	}
	*__vecSizePtr(a) = (char *)result - (char *)a;
	return a;
}
static int longCmp(const void *a, const void *b) {
	const long *A = a, *B = b;
	if (*A > *B)
		return 1;
	if (*A < *B)
		return -1;
	return 0;
}
// https://www.cplusplus.com/reference/algorithm/unique/
struct __vec *__vecUnique(struct __vec *vec, long itemSize, int (*pred)(const void *, const void *), void (*kill)(void *)) {
	long end = __vecSize(vec) / itemSize;
	long moveBuffer[__vecSize(vec) / itemSize];
	for (long i = 0; i != end; i++)
		moveBuffer[i] = i;

	if (end == 0)
		return vec;

	long res = 0, first = 0;
	for (;;) {
		if (++first == end)
			break;

		if (0 != pred((void *)vec + moveBuffer[res] * itemSize, (void *)vec + moveBuffer[first] * itemSize)) {
			moveBuffer[++res] = moveBuffer[first];
		}
	}

	// Remove items not appearing in result
	if (kill != NULL) {
		qsort(moveBuffer, res, sizeof(long), longCmp);
		for (long i = 0; i != end; i++) {
			if (NULL == bsearch(&i, moveBuffer, res, sizeof(long), longCmp)) {
				kill((void *)vec + itemSize * i);
			}
		}
	}

	res++;
	for (long i = 0; i != res; i++)
		memmove((void *)vec + itemSize * i, (void *)vec + itemSize * moveBuffer[i], itemSize);

	return __vecResize(vec, res * itemSize);
}
// https://www.cplusplus.com/reference/algorithm/set_intersection/
struct __vec *__vecSetIntersection(struct __vec *a, const struct __vec *b, long itemSize, int (*pred)(const void *, const void *), void (*kill)(void *)) {
	long count = __vecSize(a) / itemSize;
	long moveBuffer[count];
	for (long i = 0; i != count; i++)
		moveBuffer[i] = i;

	const void *first1 = a;
	const void *first2 = b;
	const void *last1 = first1 + __vecSize(a);
	const void *last2 = first2 + __vecSize(b);
	long result = 0;
	while (first1 != last1 && first2 != last2) {
		if (pred(first2, first1) > 0)
			first1 += itemSize;
		else if (pred(first1, first2) > 0)
			first2 += itemSize;
		else {
			moveBuffer[result / itemSize] = (first1 - (void *)a) / itemSize;
			first1 += itemSize, first2 += itemSize, result += itemSize;
		}
	}

	if (kill != NULL) {
		// Kill items not appearing in result
		qsort(moveBuffer, result / itemSize, sizeof(long), longCmp);
		for (long i = 0; i != __vecSize(a) / itemSize; i++) {
			if (NULL == bsearch(&i, moveBuffer, result / itemSize, sizeof(long), longCmp)) {
				kill((void *)a + itemSize * i);
			}
		}
	}

	for (long i = 0; i != result / itemSize; i++) {
		memmove((void *)a + i * itemSize, (void *)a + moveBuffer[i] * itemSize, itemSize);
	}

	return __vecResize(a, result);
}
// https://www.cplusplus.com/reference/algorithm/set_union/
struct __vec *__vecSetUnion(struct __vec *a, struct __vec *b, long itemSize, int (*pred)(const void *, const void *)) {
	void *s1 = a;
	void *s2 = b;
	void *e1 = s1 + __vecSize(a);
	void *e2 = s2 + __vecSize(b);

	__auto_type retVal = __vecResize(NULL, __vecSize(a) + __vecSize(b));
	void *r = retVal;
	while (1) {
		if (s1 == e1) {
			memmove(r, s2, e2 - s2);
			r += e2 - s2;

			return __vecResize(retVal, r - (void *)retVal);
		}
		if (s2 == e2) {
			memmove(r, s1, e1 - s1);
			r += e1 - s1;

			return __vecResize(retVal, r - (void *)retVal);
		}
		if (pred(s2, s1) > 0) {
			memmove(r, s1, itemSize);
			s1 += itemSize;
		} else if (pred(s1, s2) > 0) {
			memmove(r, s2, itemSize);
			s2 += itemSize;
		} else {
			memmove(r, s1, itemSize);
			s1 += itemSize;
			s2 += itemSize;
		}
		r += itemSize;
	}
}
void __vecDestroy(struct __vec **vec) {
	if (*vec == NULL)
		return;

	free((void *)*vec - 2 * sizeof(long));
}
struct __vec *__vecRemoveItem(struct __vec *str, long itemSize, const void *item, int (*pred)(const void *, const void *)) {
	void *find = bsearch(item, str, __vecSize(str) / itemSize, itemSize, pred);
	if (!find)
		return str;
	memmove(find, find + itemSize, __vecSize(str) - (find - (void *)str) - itemSize);
	*__vecSizePtr(str) -= itemSize;
	return str;
}
// https://www.cplusplus.com/reference/algorithm/reverse/
struct __vec *__vecReverse(struct __vec *str, long itemSize) {
	void *first = (void *)str;
	void *last = first + __vecSize(str);
	while ((first != last) && (first != (last -= itemSize))) {
		char buffer[itemSize];
		memmove(buffer, first, itemSize);
		memmove(first, last, itemSize);
		memmove(last, buffer, itemSize);
		first += itemSize;
	}
	return str;
}
