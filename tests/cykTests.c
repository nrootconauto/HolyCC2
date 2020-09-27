#include <assert.h>
#include <cyk.h>
#include <string.h>
static int Si = 0, VPi = 1, PPi = 2, NPi = 3, Vi = 4, Pi = 5, Ni = 6, Deti = 7;
struct __pair {
	const char *text;
	struct __cykRule *value;
};
STR_TYPE_DEF(struct __pair, Pair);
STR_TYPE_FUNCS(struct __pair, Pair);
static strCYKRulesP classify(const void *item, const void *pairs) {
	const strPair *Pairs = pairs;
	strCYKRulesP retVal = NULL;

	for (int i = 0; i != strPairSize(Pairs[0]); i++) {
		if (0 == strcmp(Pairs[0][i].text, *(char **)item))
			retVal = strCYKRulesPAppendItem(retVal, Pairs[0][i].value);
	}

	return retVal;
}
STR_TYPE_DEF(char *, String);
STR_TYPE_FUNCS(char *, String);
void cykTests() {
	struct __pair terminalPairs[] = {
	    (struct __pair){"a", cykRuleCreateTerminal(Deti, 1)},
	    (struct __pair){"fork", cykRuleCreateTerminal(Ni, 1)},
	    (struct __pair){"fish", cykRuleCreateTerminal(Ni, 1)},
	    (struct __pair){"with", cykRuleCreateTerminal(Pi, 1)},
	    (struct __pair){"eats", cykRuleCreateTerminal(Vi, 1)},
	    (struct __pair){"she", cykRuleCreateTerminal(NPi, 1)},
	    (struct __pair){"eats", cykRuleCreateTerminal(VPi, 1)},
	};
	__auto_type size = sizeof(terminalPairs) / sizeof(*terminalPairs);
	__auto_type terminalPairs2 = strPairResize(NULL, size);
	memcpy(terminalPairs2, terminalPairs, size * sizeof(struct __pair));

	struct __cykRule *nonTermRules[] = {
	    cykRuleCreateNonterminal(Si, 1, NPi, VPi),
	    cykRuleCreateNonterminal(VPi, 1, VPi, PPi),
	    cykRuleCreateNonterminal(VPi, 1, Vi, NPi),
	    cykRuleCreateNonterminal(PPi, 1, Pi, NPi),
	    cykRuleCreateNonterminal(NPi, 1, Deti, Ni),
	};
	__auto_type size2 = sizeof(nonTermRules) / sizeof(*nonTermRules);

	strCYKRulesP grammar = strCYKRulesPResize(NULL, size + size2);
	for (int i = 0; i != size; i++)
		grammar[i] = terminalPairs[i].value;
	for (int i = 0; i != size2; i++)
		grammar[i + size] = nonTermRules[i];

	strString sentence = strStringResize(NULL, 7);
	sentence[0] = "she";
	sentence[1] = "eats";
	sentence[2] = "a";
	sentence[3] = "fish";
	sentence[4] = "with";
	sentence[5] = "a";
	sentence[6] = "fork";

	__auto_type table =
	    __cykBinary(grammar, (struct __vec *)sentence, sizeof(const char *),
	                strCYKRulesPSize(grammar), classify, &terminalPairs2);

	cykBinaryMatrixDestroy(&table);
}
