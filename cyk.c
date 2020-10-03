#include <assert.h>
#include <cyk.h>
#include <graph.h>
#include <libdill.h>
#include <linkedList.h>
#include <math.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
enum __cykRuleType { CYK_TERMINAL, CYK_NONTERMINAL };
struct __cykRule {
	enum __cykRuleType type;
	int value;
	double weight;
	struct {
		int a;
		int b;
	} nonTerminal;
};
struct __cykBinaryMatrix {
	int w;
	int **bits; //[y][x]
	int grammarSize;
};
STR_TYPE_DEF(int, Int);
STR_TYPE_FUNCS(int, Int);
#define INT_BITS (8 * sizeof(int))
struct __cykRule *cykRuleCreateTerminal(int value, double weight) {
	struct __cykRule rule;
	rule.type = CYK_TERMINAL;
	rule.weight = weight;
	rule.value = value;

	struct __cykRule *retVal = malloc(sizeof(struct __cykRule));
	return (*retVal = rule, retVal);
}
struct __cykRule *cykRuleCreateNonterminal(int value, double weight, int a,
                                           int b) {
	struct __cykRule rule;
	rule.type = CYK_NONTERMINAL;
	rule.weight = weight;
	rule.value = value;
	rule.nonTerminal.a = a;
	rule.nonTerminal.b = b;

	struct __cykRule *retVal = malloc(sizeof(struct __cykRule));
	return (*retVal = rule, retVal);
}
static int bitsSearchReverse(int *buffer, int intCount, int startAt) {
	int index = startAt;
	for (int i = index / INT_BITS; i >= 0; i--) {
		unsigned int clone = buffer[i];
		__auto_type shift = INT_BITS - index % INT_BITS;
		clone <<= shift;
		clone >>= shift;

		if (clone == 0) {
			index = index / INT_BITS * INT_BITS - INT_BITS;
			int res = index / INT_BITS;
			assert(i - 1 == res);
			continue;
		}

		int offset = __builtin_clz(clone);
		return i * INT_BITS + INT_BITS - offset - 1;
	}
	return -1;
}
static int bitsSearch(int *buffer, int intCount, int startAt) {
	int index = startAt;
	for (int i = index / INT_BITS; i < intCount; i++) {
		unsigned int clone = buffer[i];
		clone >>= index % INT_BITS;
		clone <<= index % INT_BITS;

		if (clone == 0) {
			index = index / INT_BITS * INT_BITS + INT_BITS;
			continue;
		}

		int offset = __builtin_ffs(clone) - 1;
		return INT_BITS * i + offset;
	}
	return -1;
}
static void __cykSetBit(struct __cykBinaryMatrix *table, int grammarSize, int y,
                        int x, int r) {
	__auto_type bit = x * grammarSize + r;
	table->bits[y][bit / INT_BITS] |= 1 << (bit % INT_BITS);
}
static int __cykCheckBit(const struct __cykBinaryMatrix *table, int grammarSize,
                         int y, int x, int r) {
	__auto_type bit = x * grammarSize + r;
	return 0 != (table->bits[y][bit / INT_BITS] & (1u << (bit % INT_BITS)));
}
static coroutine void cykCheckRule(const struct __cykBinaryMatrix *table,
                                   int grammarSize, int s, int l,
                                   struct __cykRule *rule,
                                   _Atomic(char) * dumpto1) {
	if (rule->type == CYK_TERMINAL)
		return;

	for (int p = 1; p <= l - 1; p++) {
		if (0 !=
		    __cykCheckBit(table, grammarSize, p - 1, s - 1, rule->nonTerminal.a))
			if (0 != __cykCheckBit(table, grammarSize, l - p - 1, s + p - 1,
			                       rule->nonTerminal.b)) {
				// printf("%i at (%i,%i) from (%i,%i),(%i,%i)\n", rule->value, l - 1,
				//       s - 1, p - 1, s - 1, l - p - 1, s + p - 1);
				dumpto1[rule->value] = 1;
				return;
			}
	}
}
static coroutine void cykCheckRules(const struct __cykBinaryMatrix *table,
                                    int grammarSize, const strCYKRulesP rules,
                                    int s, int l, _Atomic(char) * dumpTo) {
	__auto_type rulesSize = strCYKRulesPSize(rules);

	int b = bundle();
	for (int i = 0; i != rulesSize; i++) {
		__auto_type rule = rules[i];
		bundle_go(b, cykCheckRule(table, grammarSize, s, l, rule, dumpTo));
	}
	bundle_wait(b, -1);
	hclose(b);
}
static void ckyS(const struct __cykBinaryMatrix *table, int inputSize,
                 int grammarSize, const strCYKRulesP rules, int l,
                 _Atomic(char) * dumpTo) {
	int b = bundle();
	for (int s = 1; s <= inputSize - l + 1; s++) {
		__auto_type dumpTo2 = &dumpTo[grammarSize * (s - 1)];
		bundle_go(b, cykCheckRules(table, grammarSize, rules, s, l, dumpTo2));
	}
	bundle_wait(b, -1);
	hclose(b);
}

static int intCountFromBits(int bits) {
	return bits / INT_BITS + (bits % INT_BITS ? 1 : 0);
}
struct __cykBinaryMatrix __cykBinaryMatrixCreate(int grammarSize) {
	struct __cykBinaryMatrix retVal;
	retVal.bits = NULL;
	retVal.w = 0;
	retVal.grammarSize = grammarSize;
	return retVal;
}
static void __cykBinaryMatrixResize(struct __cykBinaryMatrix *table,
                                    int itemsCount) {
	int newSize = table->grammarSize * itemsCount;
	__auto_type difference = itemsCount - table->w;

	if (difference < 0)
		for (int i = table->w - 1; i >= table->w; i++)
			free(table->bits[i]);

	table->bits = realloc(table->bits, itemsCount * sizeof(int *));

	for (int i = 0; i != itemsCount; i++) {
		__auto_type oldIntCount =
		    (table->w > i) ? intCountFromBits(table->w - i) : 0;
		__auto_type ptr = (table->w > i) ? table->bits[i] : NULL;
		ptr = realloc(ptr, intCountFromBits(newSize) * sizeof(int));

		__auto_type count = intCountFromBits(newSize - i) - oldIntCount;
		memset(ptr + oldIntCount, 0, count * sizeof(int));
		table->bits[i] = ptr;
	}

	table->w = itemsCount;
}

struct __cykBinaryMatrix *
__cykBinary(const strCYKRulesP rules, struct __vec *items, long itemSize,
            int grammarSize, strCYKRulesP(classify)(const void *, const void *),
            void *data) {
	__auto_type totalSize = __vecSize(items) / itemSize;

	__auto_type width = intCountFromBits(totalSize * grammarSize);

	struct __cykBinaryMatrix table = __cykBinaryMatrixCreate(grammarSize);
	__cykBinaryMatrixResize(&table, totalSize);

	for (int i1 = 0; i1 != totalSize; i1++) {
		__auto_type categories = classify((void *)items + itemSize * i1, data);
		for (int i2 = 0; i2 != strCYKRulesPSize(categories); i2++)
			__cykSetBit(&table, grammarSize, 0, i1, categories[i2]->value);

		strCYKRulesPDestroy(&categories);
	}

	_Atomic(char) buffer[totalSize * grammarSize];
	for (int l = 2; l <= totalSize; l++) {
		memset(buffer, 0, totalSize * grammarSize * sizeof(char));
		ckyS(&table, totalSize, grammarSize, rules, l, buffer);

		// Fill bits
		for (int i = 1; i <= totalSize - l + 1; i++) {
			for (int r = 0; r != grammarSize; r++)
				if (buffer[(i - 1) * grammarSize + r])
					__cykSetBit(&table, grammarSize, l - 1, i - 1, r);
		}
	}

	struct __cykBinaryMatrix *retVal = malloc(sizeof(struct __cykBinaryMatrix));
	*retVal = table;
	return retVal;
}
STR_TYPE_DEF(double, Double);
STR_TYPE_FUNCS(double, Double);
struct __CYKEntry {
	int x, y, r;
	double prob;
	struct __CYKEntry *a, *b;
};
STR_TYPE_DEF(struct __CYKEntry, CYKEntry);
STR_TYPE_FUNCS(struct __CYKEntry, CYKEntry);
int __CYKEntryPred(const void *a, const void *b) {
	const struct __CYKEntry *A = a, *B = b;
	__auto_type res = A->y - B->y;
	if (res != 0)
		return res;

	res = A->x - B->x;
	if (res != 0)
		return res;

	res = A->r - B->r;
	return res;
}
static int CYKProbalisticCheckRuleS(const strCYKRulesP grammar,
                                    strCYKEntry entries, int s, int l, int p,
                                    int r, struct __CYKEntry *dumpTo) {
	struct __CYKEntry retVal;
	if (dumpTo != NULL)
		retVal.prob = dumpTo->prob;
	else
		retVal.prob = NAN;

	for (int i = 0; i != strCYKRulesPSize(grammar); i++) {
		if (grammar[i]->value != r)
			continue;

		if (grammar[i]->type == CYK_NONTERMINAL) {
			struct __CYKEntry entryA;
			entryA.r = grammar[i]->nonTerminal.a;
			entryA.y = p - 1;
			entryA.x = s - 1;
			__auto_type A = strCYKEntrySortedFind(entries, entryA, __CYKEntryPred);

			struct __CYKEntry entryB;
			entryB.r = grammar[i]->nonTerminal.b;
			entryB.y = l - p - 1;
			entryB.x = p + s - 1;
			__auto_type B = strCYKEntrySortedFind(entries, entryB, __CYKEntryPred);

			if (A != NULL && B != NULL) {
				if (retVal.prob != NAN) {
					if (retVal.prob > grammar[i]->weight) {
						continue;
					} else {
						retVal.prob = grammar[i]->weight;
					}
				} else {
					retVal.prob = grammar[i]->weight;
				}

				retVal.a = A;
				retVal.b = B;
				retVal.r = r;
				retVal.x = s - 1;
				retVal.y = l - 1;
			}
		}
	}

	if (dumpTo != NULL)
		*dumpTo = retVal;
	return retVal.prob != NAN;
}
static coroutine void CYKProbalisticS(const strCYKRulesP grammar,
                                      strCYKEntry entries, int s, int l, int r,
                                      struct __CYKEntry *dumpTo) {
	struct __CYKEntry retVal;
	retVal.prob = NAN;

	int found = 0;
	for (int p = 1; p <= l - 1; p++) {
		found |= CYKProbalisticCheckRuleS(grammar, entries, s, l, p, r, &retVal);
	}

	assert(found != 0);

	if (dumpTo != NULL)
		*dumpTo = retVal;
}
static int findFirstAtRow(const void *a, const void *b) {
	const int *A = a;
	const struct __CYKEntry *B = b;
	return (*A <= B->y) ? 0 : 1;
}
STR_TYPE_DEF(struct __CYKEntry *, CYKEntryP);
STR_TYPE_FUNCS(struct __CYKEntry *, CYKEntryP);
graphNodeCYKTree CYKTree(const strCYKRulesP grammar, int grammarSize,
                         struct __vec *items,
                         struct __cykBinaryMatrix *binaryTable, int Y, int X,
                         int R, long itemSize,
                         strCYKRulesP (*classify)(const void *, const void *),
                         void *data) {
	if (binaryTable == NULL)
		binaryTable =
		    __cykBinary(grammar, items, itemSize, grammarSize, classify, data);
	// Compute total bit count

	int popCount = 0;
	for (int y = 0; y != binaryTable->w; y++) {
		__auto_type intCount = intCountFromBits((binaryTable->w - y) * grammarSize);
		for (int i = 0; i != intCount; i++)
			popCount += __builtin_popcount(binaryTable->bits[y][i]);
	}

	strCYKEntry entries = strCYKEntryResize(NULL, popCount);
	int entryIndex = 0;
	for (int y = 0; y != binaryTable->w; y++) {
		__auto_type intCount = intCountFromBits((binaryTable->w - y) * grammarSize);
		for (int i = bitsSearch(binaryTable->bits[y], intCount, 0); i != -1;
		     i = bitsSearch(binaryTable->bits[y], intCount, i + 1)) {
			int x = i / grammarSize;
			int r = i % grammarSize;

			struct __CYKEntry entry;
			entry.prob = NAN;
			entry.r = r;
			entry.x = x;
			entry.y = y;
			entry.a = NULL;
			entry.b = NULL;

			if (y == 0) {
				__auto_type result = classify((void *)items + itemSize * x, data);

				double maxWeight = NAN;
				for (int i = 0; i != strCYKRulesPSize(result); i++) {
					if (maxWeight == NAN)
						maxWeight = result[i]->weight;
					else
						maxWeight =
						    (maxWeight > result[i]->weight) ? maxWeight : result[i]->weight;
				}

				entry.prob = maxWeight;
				strCYKRulesPDestroy(&result);
			}

			entries[entryIndex++] = entry;
		}
	}

	for (int l = 2; l <= binaryTable->w; l++) {
		// Find lower bound of row l-1
		int l2 = l - 1;
		__auto_type lowerBoundPtr = __vecSortedFind(
		    (struct __vec *)entries, &l2, sizeof(*entries), findFirstAtRow);
		if (lowerBoundPtr == NULL)
			lowerBoundPtr = entries + strCYKEntrySize(entries);
		__auto_type lowerBound =
		    (lowerBoundPtr - (void *)entries) / sizeof(*entries);

		// Find upper bound of row l-1
		int l3 = l - 1 + 1;
		__auto_type upperBoundPtr = (struct __CYKEntry *)__vecSortedFind(
		    (struct __vec *)entries, &l3, sizeof(*entries), findFirstAtRow);
		if (upperBoundPtr == NULL)
			upperBoundPtr = entries + strCYKEntrySize(entries);
		__auto_type upperBound =
		    ((void *)upperBoundPtr - (void *)entries) / sizeof(*entries);

		int b = bundle();
		struct __CYKEntry dumpTo[binaryTable->w];
		for (int i = lowerBound; i != upperBound; i++) {
			__auto_type s = entries[i].x + 1;
			__auto_type l = entries[i].y + 1;
			__auto_type dumpTo2 = &dumpTo[i - lowerBound];
			bundle_go(b,
			          CYKProbalisticS(grammar, entries, s, l, entries[i].r, dumpTo2));
		}
		bundle_wait(b, -1);
		hclose(b);

		for (int i = lowerBound; i != upperBound; i++) {
			entries[i] = dumpTo[i - lowerBound];
		}
	}

	struct __CYKEntry expected;
	expected.x = X;
	expected.y = Y;
	expected.r = R;

	__auto_type res = strCYKEntrySortedFind(entries, expected, __CYKEntryPred);
	if (res == NULL)
		return NULL;

	int childIndex = 0;
	strInt childStack = strIntAppendItem(NULL, 0);
	strCYKEntryP stack = strCYKEntryPAppendItem(NULL, res);
	__auto_type node = graphNodeCYKTreeCreate(res->r, 0);
	while (strCYKEntryPSize(stack) != 0) {
		__auto_type top = stack[strCYKEntryPSize(stack) - 1];
		bool hasChildren = top->a != NULL && top->b != NULL;

		if (!hasChildren) {
			goto pop;
		} else {
			goto push;
		}
		continue;
	push : {
		__auto_type child = childStack[strIntSize(childStack) - 1];
		childStack = strIntAppendItem(childStack, 0);
		__auto_type next = (child == 0) ? top->a : top->b;
		stack = strCYKEntryPAppendItem(stack, next);

		__auto_type newNode = graphNodeCYKTreeCreate(next->r, 0);
		graphNodeCYKTreeConnect(node, newNode, child);
		node = newNode;
		continue;
	}
	pop : {
		stack = strCYKEntryPResize(stack, strCYKEntryPSize(stack) - 1);
		__auto_type newChildStackSize = strIntSize(childStack) - 1;
		childStack = strIntResize(childStack, newChildStackSize);

		if (strIntSize(childStack) == 0)
			break;

		// Goto parent node on graph
		__auto_type incoming = graphNodeCYKTreeIncoming(node);
		assert(incoming != NULL);
		node = graphEdgeCYKTreeIncoming(incoming[0]);
		strGraphEdgeCYKTreePDestroy(&incoming);

		// Go to next child,
		childStack[newChildStackSize-1]++;
		// Pop is past second child
		if (childStack[newChildStackSize-1] == 2)
			goto pop;

		continue;
	}
	}
	strCYKEntryPDestroy(&stack);
	strIntDestroy(&childStack);

	return node;
}
void cykBinaryMatrixDestroy(struct __cykBinaryMatrix **mat) {
	for (int i = 0; i != mat[0]->w; i++)
		free(mat[0]->bits[i]);

	free(mat[0]->bits);
	free(mat[0]);
}
int __cykIteratorPrev(struct __cykBinaryMatrix *table,
                      struct __cykIterator *iter) {
	__auto_type retVal = *iter;

	__auto_type index = retVal.x * table->grammarSize + iter->r - 1;
	if (index < 0) {
		retVal.y--;
		retVal.r = table->grammarSize - 1;
		retVal.x = table->w - retVal.y;
	} else {
		retVal.x = index / table->grammarSize;
		retVal.r = index % table->grammarSize;
	}

	for (; retVal.x >= 0 && retVal.y >= 0;) {
		__auto_type intCount = intCountFromBits(table->w - retVal.y);
		__auto_type newX = bitsSearchReverse(
		    table->bits[retVal.y], intCount,
		    retVal.r +
		        table->grammarSize * retVal.x); // Add table->grammarSize to
		                                        // start search at and of column

		if (newX == -1) {
			retVal.y--;

			if (retVal.y >= 0) {
				retVal.x = table->w - retVal.y;
				retVal.r = table->grammarSize - 1;
			} else
				break;

			continue;
		}

		retVal.x = newX / table->grammarSize;
		retVal.r = newX % table->grammarSize;
		*iter = retVal;
		return 1;
	}
	return 0;
}
int __cykIteratorNext(struct __cykBinaryMatrix *table,
                      struct __cykIterator *iter) {
	__auto_type retVal = *iter;

	__auto_type index = retVal.x * table->grammarSize + iter->r + 1;
	if (index >= table->grammarSize * table->w) {
		retVal.y++;
		retVal.r = 0;
		retVal.x = 0;
	} else {
		retVal.x = index / table->grammarSize;
		retVal.r = index % table->grammarSize;
	}

	for (; retVal.y < table->w;) {
		__auto_type intCount =
		    intCountFromBits((table->w - retVal.y) * table->grammarSize);
		__auto_type newX = bitsSearch(table->bits[retVal.y], intCount,
		                              retVal.r + retVal.x * table->grammarSize);

		if (newX == -1) {
			retVal.y++;
			retVal.x = 0;
			retVal.r = 0;
			continue;
		}

		retVal.x = newX / table->grammarSize;
		retVal.r = newX % table->grammarSize;
		*iter = retVal;
		return 1;
	}

	return 0;
}
int __cykIteratorInitStart(struct __cykBinaryMatrix *table,
                           struct __cykIterator *iter) {
	struct __cykIterator start;
	start.r = 0;
	start.x = 0;
	start.y = 0;
	*iter = start;
	// Search forward as start is a dummy value,then try to go backwards
	__auto_type res = __cykIteratorNext(table, iter);
	__auto_type clone = *iter;
	if (res == 1)
		if (__cykIteratorPrev(table, iter))
			return 1;

	*iter = clone;
	return res;
}
int __cykIteratorInitEnd(struct __cykBinaryMatrix *table,
                         struct __cykIterator *iter) {
	struct __cykIterator start;
	start.r = table->grammarSize - 1;
	start.x = 0;
	start.y = table->w - 1;
	*iter = start;

	// Search backward as start is a dummy value,then try to go forward
	__auto_type res = __cykIteratorPrev(table, iter);
	__auto_type clone = *iter;
	if (res == 1)
		if (__cykIteratorNext(table, iter))
			return 1;

	*iter = clone;
	return res;
}
