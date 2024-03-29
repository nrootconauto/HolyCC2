#include <assert.h>
#include <cykGrammarGenerator.h>
static int validate(const void *a, const void *b) {
	return *(int *)a == *(int *)b;
}
static void strRulePDestroy2(strRuleP *vec) {
	for (long i = 0; i != strRulePSize(*vec); i++)
		grammarRuleDestroy(&vec[0][i]);
	strRulePDestroy(vec);
};
#define arrayToVec(vec) ({ __vecAppendItem(NULL, vec, sizeof(vec)); })
static const int terminalsCount = 4;
void cykGrammarGeneratorTests() {
	int a = 1, b = 2, c = 3, d = 4;

	__auto_type one = grammarRuleTerminalCreate("A", "A", 1, validate, &a, NULL);
	__auto_type two = grammarRuleTerminalCreate("B", "B", 1, validate, &b, NULL);
	__auto_type three =
	    grammarRuleTerminalCreate("C", "C", 1, validate, &c, NULL);
	__auto_type four = grammarRuleTerminalCreate("D", "D", 1, validate, &d, NULL);
	{
		__auto_type opt = grammarRuleOptCreate("OPT", "OPT", 1, one);
		__auto_type seq =
		    grammarRuleSequenceCreate("EXPECTED", "EXPECTED", 1, one, opt, NULL);
		__auto_type rep=grammarRuleRepeatCreate("REP","REP",1,two);
		__auto_type seq2 =
		    grammarRuleSequenceCreate("EXPECTED2", "EXPECTED2", 1, one, rep, NULL);
		struct grammarRule *rules[] = {seq,seq2};
		strRuleP rulesVec = (strRuleP)arrayToVec(rules);
		const struct grammarRule *tops[] = {seq};
		strRuleP topLevels = (strRuleP)arrayToVec(tops);
		
		__auto_type grammar = grammarCreate(rulesVec);
		 grammarPrint(grammar);
		
		const int data1[] = {1};
		__auto_type data1Vec = arrayToVec(data1);
		const int data2[] = {1, 1};
		__auto_type data2Vec = arrayToVec(data2);
		const int data3[] = {1, 2,2,2,2};
		__auto_type data3Vec = arrayToVec(data3);

		__auto_type parsing1 = grammarCreateParsingFromData(
		    grammar, data1Vec, sizeof(int)); // TODO destroy
		__auto_type parsing1Tops = grammarParsingGetTops(parsing1);
		assert(strGraphNodeCYKTreePSize(parsing1Tops) == 1);

		__auto_type parsing2 = grammarCreateParsingFromData(
		    grammar, data2Vec, sizeof(int)); // TODO destroy
		__auto_type parsing2Tops = grammarParsingGetTops(parsing2);
		assert(strGraphNodeCYKTreePSize(parsing2Tops) == 1);

		
		__auto_type parsing3 = grammarCreateParsingFromData(
		    grammar, data3Vec, sizeof(int)); // TODO destroy
		__auto_type parsing3Tops = grammarParsingGetTops(parsing3);
		assert(strGraphNodeCYKTreePSize(parsing3Tops) == 1);
		
		grammarDestroy(&grammar);
		strRulePDestroy2(&rulesVec);
	}
}
