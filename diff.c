#include <assert.h>
#include <diff.h>
#include <libdill.h>
#include <limits.h>
#include <linkedList.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
struct __intPair {
	int x;
	int y;
	int d;
};
STR_TYPE_DEF(struct __intPair, IntPair);
STR_TYPE_FUNCS(struct __intPair, IntPair);
LL_TYPE_DEF(struct __diff, Diff);
LL_TYPE_FUNCS(struct __diff, Diff);
#define SWAP(a, b)                                                             \
	({                                                                           \
		__auto_type t = a;                                                         \
		a = b;                                                                     \
		b = t;                                                                     \
	})
static int __diffFollowPath(const void *a, const void *b, int x, int y,
                            long aSize, long bSize, long itemSize,
                            int direction,
                            int (*pred)(const void *, const void *)) {
	int len = 0;
	if (direction == -1) {
		x--, y--;
	}
	for (;; x += direction, y += direction) {
		if (aSize / itemSize < x)
			break;
		if (bSize / itemSize < y)
			break;
		if (x < 0)
			break;
		if (y < 0)
			break;
		if (!pred(a + x * itemSize, b + y * itemSize))
			break;
		len++;
	}
	return len;
}
static bool __diffPathLeadsToPath(const void *a, const void *b, long aSize,
                                  long bSize, int x1, int y1, int x2, int y2,
                                  long itemSize, int *firstX, int *firstY,
                                  int *lastX, int *lastY,
                                  int (*pred)(const void *, const void *)) {
	if (x1 > x2) {
		SWAP(x1, x2);
		SWAP(y1, y2);
	}
	if (firstX != NULL)
		*firstX = x1;
	if (firstY != NULL)
		*firstY = y1;
	if (lastX != NULL)
		*lastX = x2;
	if (lastY != NULL)
		*lastY = y2;
	//
	for (;; x1++, y1++) {
		if (x1 == x2 && y1 == y2)
			return true;
		if (aSize / itemSize <= x1)
			break;
		if (bSize / itemSize <= y1)
			break;
		if (!pred(a + x1 * itemSize, b + y1 * itemSize)) {
			break;
		}
	}
	return false;
}
static void __findUpperLowerBound(const strIntPair array, int totalSize, int d,
                                  int *lower, int *higher) {
	int i = totalSize - d;
	for (i = 0; array[i].x == -1 && i != totalSize + d + 1; i++)
		;
	*lower = i;
	for (; array[i].x != -1 && i != totalSize + d + 1; i++)
		;
	*higher = i;
}
static bool __diffCheckIfMeet(int totalSize, int d, const void *a,
                              const void *b, int aSize, int bSize, int itemSize,
                              const strIntPair forward,
                              const strIntPair backward, int *resultFirstX,
                              int *resultFirstY, int *resultLastX,
                              int *resultLastY,
                              int (*pred)(const void *, const void *)) {
	int maxD = INT_MIN;
	bool foundItem = false;
	int fL, fH;
	__findUpperLowerBound(forward, totalSize, d, &fL, &fH);
	int bL, bH;
	__findUpperLowerBound(backward, totalSize, d, &bL, &bH);
	for (int i1 = fL; i1 != fH; i1++) {
		__auto_type x = forward[i1].x;
		__auto_type y = forward[i1].y;
		for (int i2 = bL; i2 != bH; i2++) {
			__auto_type otherX = backward[i2].x;
			__auto_type otherY = backward[i2].y;
			//
			int sharedDiagDist = 0;
			bool match = x == otherX && y == otherY;
			int fX, fY, lX, lY;
			if (!match) {
				match = __diffPathLeadsToPath(a, b, aSize, bSize, x, y, otherX, otherY,
				                              itemSize, &fX, &fY, &lX, &lY, pred);
				if (match)
					sharedDiagDist = lX - fX;
			} else {
				fX = x;
				fY = y;
				lX = x;
				lY = y;
			}
			//
			if (match) {
				int k = fX - fY;
				int otherK = (aSize / itemSize - lX) - (bSize / itemSize - lY);
				int d = forward[totalSize + k].d;
				if (d < maxD)
					continue;
				if (forward[totalSize + k].x + backward[totalSize + otherK].x >=
				    aSize / itemSize) {
					foundItem = true;
					maxD = d;
					if (resultFirstX != NULL)
						*resultFirstX = fX;
					if (resultFirstY != NULL)
						*resultFirstY = fY;
					if (resultLastX != NULL)
						*resultLastX = lX;
					if (resultLastY != NULL)
						*resultLastY = lY;
				}
			}
		}
	}
	return foundItem;
}
static llDiff __diffRecur(const void *a, const void *b, long aSize, long bSize,
                          long itemSize, llDiff prev, llDiff next,
                          int (*pred)(const void *, const void *));
static coroutine void __diffRecurSplit(const void *a, const void *b, long aSize,
                                       long bSize, long itemSize,
                                       int (*pred)(const void *, const void *),
                                       llDiff *result) {
	*result = __diffRecur(a, b, aSize, bSize, itemSize, NULL, NULL, pred);
}
static llDiff __diffRecur(const void *a, const void *b, long aSize, long bSize,
                          long itemSize, llDiff prev, llDiff next,
                          int (*pred)(const void *, const void *)) {
	const void *aEnd = a + aSize;
	const void *bEnd = b + bSize;
	// Check if nothing
	if (aSize == 0 && bSize == 0) {
		return (next == NULL) ? prev : next;
	}
	// Check if same before going into main body
	if (aSize == bSize) {
		if (aSize / itemSize ==
		    __diffFollowPath(a, b, 0, 0, aSize, bSize, itemSize, 1, pred)) {
			struct __diff diff;
			diff.len = aSize / itemSize;
			diff.type = DIFF_SAME;
			__auto_type newN = llDiffCreate(diff);
			llDiffInsertListBefore(__llGetFirst(next), newN);
			llDiffInsertListAfter(__llGetEnd(prev), newN);
			return newN;
		}
	} else if (aSize == 0) {
		struct __diff diff;
		diff.len = bSize / itemSize;
		diff.type = DIFF_INSERT;
		__auto_type newN = llDiffCreate(diff);
		llDiffInsertListBefore(__llGetFirst(next), newN);
		llDiffInsertListAfter(__llGetEnd(prev), newN);
		return newN;
	} else if (bSize == 0) {
		struct __diff diff;
		diff.len = aSize / itemSize;
		diff.type = DIFF_REMOVE;
		__auto_type newN = llDiffCreate(diff);
		llDiffInsertListBefore(__llGetFirst(next), newN);
		llDiffInsertListAfter(__llGetEnd(prev), newN);
		return newN;
	}
	//
	long totalSize = (aSize + bSize) / itemSize;
	strIntPair forward;
	strIntPair backward;
	forward = strIntPairResize(NULL, totalSize * 2 + 1);
	backward = strIntPairResize(NULL, totalSize * 2 + 1);
	for (int i = 0; i != totalSize * 2 + 1; i++) {
		forward[i].x = -1;
		forward[i].y = -1;
		forward[i].d = -1;
		backward[i].x = -1;
		backward[i].y = -1;
		backward[i].d = -1;
	}
	forward[totalSize].x = 0;
	forward[totalSize].y = 0;
	backward[totalSize].x = aSize / itemSize;
	backward[totalSize].y = bSize / itemSize;
	forward[totalSize].d = 0;
	backward[totalSize].d = 0;
	for (int d = 1;; d++) {
		for (int k = -d; k <= d; k += 2) {
			{
				bool moveDown = k == -d;
				bool moveRight = k == d;
				int offset;
				if (moveDown)
					offset = 1;
				else if (moveRight)
					offset = -1;
				else {
					offset = (forward[totalSize + k - 1].x < forward[totalSize + k + 1].x)
					             ? 1
					             : -1;
				}
				int x = forward[k + totalSize + offset].x;
				if (moveRight) {
					x++;
				}
				int y = x - k;
				if (x == 3 && y == 1)
					printf("g\n");
				//
				__auto_type a2 = a + x * itemSize;
				__auto_type b2 = b + y * itemSize;
			loop1:
				if (a2 < aEnd && b2 < bEnd) {
					if (pred(a2, b2)) {
						x++;
						y++;
						a2 += itemSize;
						b2 += itemSize;
						goto loop1;
					}
				}
				//
				bool assign = false;
				if (forward[k + totalSize].x < x) {
					forward[k + totalSize].x = x;
					assign = true;
				}
				if (assign || forward[k + totalSize].d == -1 ||
				    forward[k + totalSize].d > d) {
					forward[k + totalSize].d = d;
					forward[k + totalSize].y = y;
				}
			}
			{
				bool moveUp = k == -d;
				bool moveLeft = k == d;
				int offset;
				if (moveUp) {
					offset = 1;
				} else if (moveLeft) {
					offset = -1;
				} else {
					offset =
					    (backward[totalSize + k - 1].x > backward[totalSize + k + 1].x)
					        ? 1
					        : -1;
				}
				int x = backward[k + totalSize + offset].x;
				if (moveLeft) {
					x--;
				}
				int y = (bSize / itemSize) - ((aSize / itemSize - x) - k);
				__auto_type a2 = a + x * itemSize - itemSize;
				__auto_type b2 = b + y * itemSize - itemSize;
				if (x == 3 && y == 1)
					printf("g\n");
			loop2:
				if (a2 < aEnd && b2 < bEnd && a2 >= a && b2 >= b) {
					if (pred(a2, b2)) {
						x--;
						y--;
						a2 -= itemSize;
						b2 -= itemSize;
						goto loop2;
					}
				}
				bool assign = false;
				if (backward[k + totalSize].x > x || backward[k + totalSize].x == -1) {
					backward[k + totalSize].x = x;
					assign = true;
				}
				if (assign || backward[k + totalSize].d == -1 ||
				    backward[k + totalSize].d > d) {
					backward[k + totalSize].d = d;
					backward[k + totalSize].y = y;
				}
			}
		}
		int resultFirstX, resultFirstY, resultLastX, resultLastY;
		if (__diffCheckIfMeet(totalSize, d, a, b, aSize, bSize, itemSize, forward,
		                      backward, &resultFirstX, &resultFirstY, &resultLastX,
		                      &resultLastY, pred)) {
			strIntPairDestroy(&forward);
			strIntPairDestroy(&backward);
			//
			int k = resultFirstX - resultFirstY;
			//
			llDiff newN = __llGetEnd(prev);
			__auto_type xOffset1 = resultFirstX * itemSize;
			__auto_type yOffset1 = resultFirstY * itemSize;
			__auto_type xOffset2 = resultLastX * itemSize;
			__auto_type yOffset2 = resultLastY * itemSize;
			//
			int bun = bundle();
			llDiff left = NULL;
			llDiff middle = NULL;
			llDiff right = NULL;
			// left
			bundle_go(bun, __diffRecurSplit(a, b, xOffset1, yOffset1, itemSize, pred,
			                                &left));
			// middle
			if (!(xOffset1 == xOffset2 && yOffset1 == yOffset2)) {
				bundle_go(bun, __diffRecurSplit(
				                   a + xOffset1, b + yOffset1, xOffset2 - xOffset1,
				                   yOffset2 - yOffset1, itemSize, pred, &middle));
			}
			// right
			bundle_go(bun,
			          __diffRecurSplit(a + xOffset2, b + yOffset2, aSize - xOffset2,
			                           bSize - yOffset2, itemSize, pred, &right));
			bundle_wait(bun, -1);
			llDiffInsertListAfter(__llGetEnd(left), __llGetFirst(middle));
			newN = (middle == NULL) ? left : middle;
			llDiffInsertListAfter(__llGetEnd(newN), __llGetFirst(right));
			newN = (right == NULL) ? newN : right;
			return __llGetEnd(newN);
		}
	}
}
strDiff __diff(const void *a, const void *b, long aSize, long bSize,
               long itemSize, int (*pred)(const void *a, const void *b)) {
	__auto_type res = __diffRecur(a, b, aSize, bSize, itemSize, NULL, NULL, pred);
	__auto_type size = llDiffSize(res);
	strDiff retVal = strDiffReserve(NULL, size);
	__auto_type node = llDiffFirst(res);
	for (int i = 0; i != size; i++) {
		__auto_type diff = *llDiffValuePtr(node);
		node = llDiffNext(node);
		if (i == 0) {
			goto insertNew;
		}
		if (retVal[strDiffSize(retVal) - 1].type == diff.type) {
			retVal[strDiffSize(retVal) - 1].len += diff.len;
			continue;
		}
	insertNew : {
		strDiffAppendItem(retVal, diff);
		continue;
	}
	}
	return retVal;
}
