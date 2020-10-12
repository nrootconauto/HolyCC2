#include <assert.h>
#include <cykGrammarGenerator.h>
static int validate(const void *a, const void *b) {
	return *(int *)a == *(int *)b;
}
static void strRulePDestroy2(strRuleP *vec) {
	for (long i = 0; i != strRulePSize(*vec); i++)
		grammarRuleDestroy(vec[i]);
	strRulePDestroy(vec);
};
#define arrayToVec(vec) ({ __vecAppendItem(NULL, vec, sizeof(vec)); })
static const int terminalsCount = 4;
void cykGrammarGeneratorTests() {
	int a = 1, b = 2, c = 3, d = 4;

	__auto_type one = grammarRuleTerminalCreate("A", 1, validate, &a, NULL);
	__auto_type two = grammarRuleTerminalCreate("B", 1, validate, &b, NULL);
	__auto_type three = grammarRuleTerminalCreate("C", 1, validate, &c, NULL);
	__auto_type four = grammarRuleTerminalCreate("D", 1, validate, &d, NULL);
	{
		__auto_type opt = grammarRuleOptCreate("OPT", 1, one);
		__auto_type seq = grammarRuleSequenceCreate("EXPECTED", 1, a, opt, NULL);
		struct grammarRule *rules[] = {opt, seq, one, two, three, four};
		strRuleP rulesVec = (strRuleP)arrayToVec(rules);
		__auto_type grammar = grammarCreate(rules);
		
		const int data1[] = {1};
		__auto_type data1Vec = arrayToVec(data1);
		const int data2[] = {1, 2};
		__auto_type data2Vec = arrayToVec(data2);

		//grammarParse(grammar,data1Vec,sizeof(int*));
		
		grammarDestroy(&grammar);
		strRulePDestroy2(&rulesVec);
	}
}
