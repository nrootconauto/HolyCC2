#include <assert.h>
#include <diff.h>
#include <stdbool.h>
#include <stdlib.h>
STR_TYPE_DEF(int, Int);
STR_TYPE_FUNCS(int, Int);
static void __diffRecur(int bundle, const void *a, const void *b, long aSize,
                        long bSize, long itemSize, enum diffType *dumpTo) {
	const void *aEnd = a + aSize;
	const void *bEnd = b + bSize;
	// Check if same before going into main body
	if (aSize == bSize) {
		if (0 == memcmp(a, b, aSize)) {
			for (int i = 0; i != aSize; i += itemSize) {
				*(dumpTo++) = DIFF_SAME;
			}
			return;
		}
	} else if (aSize == 0) {
		for (int i = 0; i != bSize; i += itemSize) {
			*(dumpTo++) = DIFF_INSERT;
		}
		return;
	} else if (bSize == 0) {
		for (int i = 0; i != aSize; i += itemSize) {
			*(dumpTo++) = DIFF_REMOVE;
		}
		return;
	}
	//
	long totalSize = (aSize + bSize) / itemSize;
	strInt forward __attribute__((cleanup(strIntDestroy)));
	strInt backward __attribute__((cleanup(strIntDestroy)));
	strInt forwardBestDist __attribute__((cleanup(strIntDestroy)));
	strInt backwardBestDist __attribute__((cleanup(strIntDestroy)));
	forward = strIntResize(NULL, totalSize * 2 + 1);
	backward = strIntResize(NULL, totalSize * 2 + 1);
	forwardBestDist = strIntResize(NULL, totalSize * 2 + 1);
	backwardBestDist = strIntResize(NULL, totalSize * 2 + 1);
	for (int i = 0; i != totalSize * 2 + 1; i++) {
		forward[i] = -1;
		backward[i] = -1;
		forwardBestDist[i] = -1;
		backwardBestDist[i] = -1;
	}
	forward[totalSize + 1] = 0;
	backward[totalSize + 1] = 0;
	forward[totalSize] = 0;
	backward[totalSize] = 0;
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
				else
					assert(0);
				int x = forward[forward[k + totalSize + offset] + totalSize];
				int y = x - k;
				if (moveRight) {
					x++;
				} else if (moveDown) {
					y++;
				}
				//
				__auto_type a2 = a + x * itemSize;
				__auto_type b2 = b + y * itemSize;
			loop1:
				if (a2 < aEnd && b2 < bEnd) {
					if (memcmp(a2, b2, itemSize)) {
						x++;
						y++;
						a2 += itemSize;
						b2 += itemSize;
						goto loop2;
					}
				}
				//
				for (int i = 0; i != 2 * totalSize + 1; i++) {
					if (forward[i] == -1)
						continue;
					int k = i - totalSize;
					int otherY = bSize / itemSize - (forward[i] - k);
					int otherX = otherY + k;
					if (otherX == x && otherY == y) {
						__auto_type xOffset = x * itemSize;
						__auto_type yOffset = y * itemSize;
						__diffRecur(bundle, a, b, xOffset, yOffset, itemSize,
						            dumpTo - forwardBestDist[i]);
						__diffRecur(bundle, a + xOffset, b + yOffset, aSize - xOffset,
						            bSize - yOffset, itemSize, dumpTo);
						return;
					}
				}
				//
				forward[k + totalSize] = x;
				if (forwardBestDist[k + totalSize] == -1 ||
				    forwardBestDist[k + totalSize] > d)
					forwardBestDist[k + totalSize] = d;
			}
			{
				bool moveUp = k == -d;
				bool moveLeft = k == -d;
				int offset;
				if (moveUp)
					offset = 1;
				else if (moveLeft)
					offset = -1;
				else
					assert(0);
				int x = (aSize / itemSize) -
				        backward[backward[k + totalSize + offset] + totalSize];
				int y = (bSize / itemSize) - backward[backward[k + totalSize + offset]] - k;
				__auto_type a2 = a + x * itemSize - itemSize;
				__auto_type b2 = b + y * itemSize - itemSize;
			loop2:
				if (a2 < aEnd && b2 < bEnd) {
					if (memcmp(a2, b2, itemSize)) {
						x--;
						y--;
						a2 -= itemSize;
						b2 -= itemSize;
						goto loop2;
					}
				}
				for (int i = 0; i != 2 * totalSize + 1; i++) {
					if (forward[i] == -1)
						continue;
					int k = i - totalSize;
					int otherY = forward[i] - k;
					int otherX = otherY + k;
					if (otherX == x && otherY == y) {
						__auto_type xOffset = otherX * itemSize;
						__auto_type yOffset = otherY * itemSize;
						__diffRecur(bundle, a, b, xOffset, yOffset, itemSize,
						            dumpTo - forwardBestDist[i]);
						__diffRecur(bundle, a + xOffset, b + yOffset, aSize - xOffset,
						            bSize - yOffset, itemSize, dumpTo);
						return;
					}
				}
				if (backwardBestDist[k + totalSize] == -1 ||
				    backwardBestDist[k + totalSize] > d)
					backwardBestDist[k + totalSize] = d;
			}
		}
	}
}
strDiff __diff(const void *a, const void *b, long aSize, long bSize,
               long itemSize) {
	long totalSize = (aSize + bSize) / itemSize;
	enum diffType buffer[2 * totalSize];
	for (int i = 0; i != 2 * totalSize; i++) {
		buffer[i] = DIFF_UNDEF;
	}
	__diffRecur(-1, a, b, aSize, bSize, itemSize, &buffer[totalSize]);
	int i;
	for (i = 0; i != 2 * totalSize; i++) {
		if (buffer[i] != DIFF_UNDEF)
			break;
	}
	int i2 = i;
	for (; i2 != 2 * totalSize; i2++) {
		if (buffer[i2] == DIFF_UNDEF)
			break;
	}
	strDiff retVal = strDiffResize(NULL, i2 - i);
	memcpy(retVal, &buffer[i], sizeof(*retVal) * (i2 - i));
	return retVal;
}
