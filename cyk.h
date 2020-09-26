#pragma once
#include <str.h>
enum __cykRuleType { CYK_TERMINAL, CYK_NONTERMINAL };
struct __cykRule {
	enum __cykRuleType type;
	int value;
	int grammarIndex;
	double weight;
		struct {
			int a;
			int b;
		} nonTerminal;
};
STR_TYPE_DEF(struct __cykRule *, CYKRulesP);
STR_TYPE_FUNCS(struct __cykRule *, CYKRulesP);
STR_TYPE_DEF(struct __cykRule, CYKRules);
STR_TYPE_FUNCS(struct __cykRule, CYKRules);
struct __cykBinaryMatrix {
	int w;
	int **bits; //[y][x]
};
struct __cykBinaryMatrix
__cykBinary(const strCYKRules grammar, struct __vec *items, long itemSize,
            int grammarSize, strCYKRulesP(classify)(const void *, const void *),
            void *data);
