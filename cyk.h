#pragma once
#include <graph.h>
#include <str.h>
struct __cykRule *cykRuleCreateTerminal(int value, double weight);
struct __cykRule *cykRuleCreateNonterminal(int value, double weight, int a,
                                           int b);
STR_TYPE_DEF(struct __cykRule *, CYKRulesP);
STR_TYPE_FUNCS(struct __cykRule *, CYKRulesP);
GRAPH_TYPE_DEF(int, int , CYKTree);
GRAPH_TYPE_FUNCS(int, int , CYKTree);
int cykRuleValue(struct __cykRule *rule) ;
struct __cykBinaryMatrix *
__cykBinary(const strCYKRulesP rules, struct __vec *items, long itemSize,
            int grammarSize, strCYKRulesP(classify)(const void *, const void *),
            void *data);
void cykBinaryMatrixDestroy(struct __cykBinaryMatrix **mat);
graphNodeCYKTree CYKTree(const strCYKRulesP grammar, int grammarSize,
                         struct __vec *items,
                         struct __cykBinaryMatrix *binaryTable, int Y, int X,
                         int R, long itemSize,
                         strCYKRulesP (*classify)(const void *, const void *),
                         void *data);
struct __cykIterator {
	int x;
	int y;
	int r;
};
int __cykIteratorPrev(struct __cykBinaryMatrix *table,
                      struct __cykIterator *iter);
int __cykIteratorNext(struct __cykBinaryMatrix *table,
                      struct __cykIterator *iter);
int __cykIteratorInitEnd(struct __cykBinaryMatrix *table,
                         struct __cykIterator *iter);
int __cykIteratorInitStart(struct __cykBinaryMatrix *table,
                           struct __cykIterator *iter);
