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
	for (int i = index / INT_BITS; i > 0; i--) {
		int clone = buffer[i];
		clone >>= index % intCount;
		clone <<= index % intCount;

		if (clone == 0) {
			continue;
		}

		int offset = __builtin_clz(clone);
		return i * INT_BITS + INT_BITS - offset;
	}
	return -1;
}
static int bitsSearch(int *buffer, int intCount, int startAt) {
	int index = startAt;
	for (int i = index / INT_BITS; i < intCount; i++) {
		int clone = buffer[i];
		clone <<= index % INT_BITS;
		clone >>= index % INT_BITS;

		if (clone == 0) {
			continue;
		}

		int offset = __builtin_ffs(clone);
		return INT_BITS * index + offset;
	}
	return -1;
}
static void __cykSetBit(struct __cykBinaryMatrix *table, int grammarSize, int x,
                        int y, int r) {
	__auto_type bit = x * grammarSize + r;
	table->bits[y][bit / INT_BITS] |= 1 << (bit % INT_BITS);
}
static int __cykCheckBit(const struct __cykBinaryMatrix *table, int grammarSize,
                         int x, int y, int r) {
	__auto_type bit = x * grammarSize + r;
	return table->bits[y][bit / INT_BITS] & (1u << (bit % INT_BITS));
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
				printf("%i at (%i,%i)\n", rule->value, l - 1, s - 1);
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
	for (int s = 1; s <= grammarSize - l + 1; s++) {
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
                                    int newSize) {
	__auto_type difference = newSize - table->w;

	if (difference < 0)
		for (int i = table->w - 1; i >= table->w; i++)
			free(table->bits[i]);

	table->bits = realloc(table->bits, newSize * sizeof(int *));

	for (int i = 0; i != newSize; i++) {
		__auto_type oldIntCount =
		    (table->w > i) ? intCountFromBits(table->w - i) : 0;

		__auto_type ptr = (table->w > i) ? table->bits[i] : NULL;
		ptr = realloc(ptr, newSize * sizeof(int));

		__auto_type count = intCountFromBits(newSize - i) - oldIntCount;
		memset(ptr + oldIntCount, 0, count * sizeof(int));
		table->bits[i] = ptr;
	}

	table->w = newSize;
}

struct __cykBinaryMatrix *
__cykBinary(const strCYKRulesP rules, struct __vec *items, long itemSize,
            int grammarSize, strCYKRulesP(classify)(const void *, const void *),
            void *data) {
	__auto_type totalSize = __vecSize(items) / itemSize;

	__auto_type width = intCountFromBits(totalSize * grammarSize);

	struct __cykBinaryMatrix table = __cykBinaryMatrixCreate(grammarSize);
	__cykBinaryMatrixResize(&table, totalSize * grammarSize);

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
int __CYKEntryPred(void *a, void *b) {
	struct __CYKEntry *A = a, *B = b;
	__auto_type res = B->y - A->y;
	if (res != 0)
		return res;

	res = A->x - B->x;
	if (res != 0)
		return res;

	res = A->r - B->r;
	return res;
}
static double CYKProbalisticCheckRuleS(const strCYKRulesP grammar,
                                       strCYKEntry entries, int s, int l,
                                       int p) {
	double retVal = NAN;

	for (int i = 0; i != strCYKRulesPSize(grammar); i++) {
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
			__auto_type B = strCYKEntrySortedFind(entries, entryA, __CYKEntryPred);

			if (A != NULL && B != NULL) {
				if (retVal != NAN) {
					retVal = (retVal > grammar[i]->weight) ? retVal : grammar[i]->weight;
				} else
					retVal = grammar[i]->weight;
			}
		}
	}

	return retVal;
}
static coroutine void CYKProbalisticS(const strCYKRulesP grammar,
                                      strCYKEntry entries, int s, int l,
                                      double *dumpTo) {
	double res;

	for (int p = 1; p <= l - 1; p++) {
		res = CYKProbalisticCheckRuleS(grammar, entries, s, l, p);
	}

	assert(res != NAN);

	*dumpTo = res;
}
static int findFirstAtRow(void *a, void *b) {
	const int *A = a;
	const struct __CYKEntry *B = b;
	return B->y - *A;
}
graphNodeCYKTree
CYKTree(const strCYKRulesP grammar, int grammarSize, struct __vec *items,
        struct __cykBinaryMatrix *binaryTable, int Y, int X, long itemSize,
        strCYKRulesP (*classify)(const void *, const void *), void *data) {
	if (binaryTable == NULL)
		binaryTable =
		    __cykBinary(grammar, items, itemSize, grammarSize, classify, data);
	// Compute total bit count

	int popCount = 0;
	for (int y = 0; y != binaryTable->w; y++) {
		__auto_type intCount = intCountFromBits(binaryTable->w - y);
		for (int i = 0; i != intCount; i++)
			popCount += __builtin_popcount(binaryTable->bits[y][i]);
	}

	strCYKEntry entries = strCYKEntryResize(NULL, popCount);
	int entryIndex = 0;
	for (int y = 0; y != binaryTable->w; y++) {
		__auto_type intCount = intCountFromBits(binaryTable->w - y);
		for (int i = 0; i != -1;
		     i = bitsSearch(binaryTable->bits[y], intCount, i + 1)) {
			int x = i / grammarSize;
			int r = i % grammarSize;

			struct __CYKEntry entry;
			entry.prob = NAN;
			entry.r = r;
			entry.x = x;
			entry.y = y;

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

	// Terminals
	int x, y;
	__auto_type intCount = intCountFromBits(binaryTable->w);
	for (int i = 0; i != binaryTable->w; i++) {
		for (int r = 0; r != grammarSize; r++)
			__cykCheckBit(binaryTable, grammarSize, i, 0, r);
	}

	for (int l = 2; l <= binaryTable->w; l++) {
		// Find lower bound of row l-1
		int l2 = l - 1;
		__auto_type lowerBound =
		    ((struct __CYKEntry *)__vecSortedFind(
		         (struct __vec *)entries, &l2, sizeof(*entries), findFirstAtRow) -
		     entries) /
		    sizeof(*entries);

		// Find upper bound of row l-1
		int l3 = l - 1 + 1;
		__auto_type upperBoundPtr = (struct __CYKEntry *)__vecSortedFind(
		    (struct __vec *)entries, &l3, sizeof(*entries), findFirstAtRow);
		if (upperBoundPtr == NULL)
			upperBoundPtr = entries + strCYKEntrySize(entries);
		__auto_type upperBound = (upperBoundPtr - entries) / sizeof(*entries);

		int b = bundle();
		double dumpTo[binaryTable->w];
		for (int i = lowerBound; i != upperBound; i++) {
			__auto_type s = entries[i].x;
			__auto_type l = entries[i].y;
			__auto_type dumpTo2 = &dumpTo[i - lowerBound];
			bundle_go(b, CYKProbalisticS(grammar, entries, s, l, dumpTo2));
		}
		bundle_wait(b, -1);
		hclose(b);

		for (int i = lowerBound; i != upperBound; i++) {
			entries[i].prob = dumpTo[i - lowerBound];
		}
	}
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
	retVal.x--;
	for (; retVal.y >= 0;) {
		__auto_type intCount = intCountFromBits(table->w - iter->y);
		__auto_type newX =
		    bitsSearchReverse(table->bits[iter->y], intCount,
		                      iter->x * table->grammarSize % INT_BITS);

		if (newX == -1) {
			retVal.y--;

			if (retVal.y >= 0)
				break;

			retVal.x = table->w - retVal.y;
			continue;
		}

		retVal.x = newX;
		*iter = retVal;
		return 1;
	}
	return 0;
}
int __cykIteratorNext(struct __cykBinaryMatrix *table,
                      struct __cykIterator *iter) {
	__auto_type retVal = *iter;
	retVal.x++;
	for (; retVal.y < table->w;) {
		__auto_type intCount = intCountFromBits(table->w - iter->y);
		__auto_type newX = bitsSearch(table->bits[iter->y], intCount,
		                              iter->x * table->grammarSize % INT_BITS);

		if (newX == -1) {
			retVal.y++;
			retVal.x = 0;
			continue;
		}

		retVal.x = newX;
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
	start.r = table->grammarSize;
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
