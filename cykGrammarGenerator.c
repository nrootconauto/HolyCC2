#include <assert.h>
#include <cyk.h>
#include <linkedList.h>
#include <str.h>
enum ruleType {
	RULE_OPT,
	RULE_REPEAT,
	RULE_SEQUENCE,
	RULE_TERMINAL,
};
struct grammarRule {
	enum ruleType type;
	float prec;
	unsigned int canBeEmpty : 1;
	unsigned int cykRuleValue : 31;
	// Data appended here
};
STR_TYPE_DEF(struct grammarRule *, RuleP);
STR_TYPE_FUNCS(struct grammarRule *, RuleP);
struct grammarRuleOpt {
	struct grammarRule base;
	struct grammarRule *rule;
};
struct grammarRuleRepeat {
	struct grammarRule base;
	struct grammarRule *rule;
};
struct grammarRuleSequence {
	struct grammarRule base;
	strRuleP rules;
};
static __thread strCYKRulesP cykRules = NULL;
static __thread strRuleP terminals = NULL;
static int ptrCmp(const void *a, const void *b) {
	if (a > b)
		return 1;
	else if (a == b)
		return 0;
	else
		return -1;
}
struct __cykRule *grammarRuleSequence2CYK(const struct grammarRule *rules,
                                          long len, float prec);
struct __cykRule *grammarRuleAtomize(struct grammarRule *rule) {
	if (rule->type == RULE_OPT) {
		__auto_type optRule = (struct grammarRuleOpt *)rule;
		return grammarRuleAtomize(optRule->rule);
	} else if (rule->type == RULE_REPEAT) {
		__auto_type repeatRule = (struct grammarRuleRepeat *)rule;
		return grammarRuleAtomize(repeatRule->rule);
	} else if (rule->type == RULE_TERMINAL) {
		// Check if terminal is already registered
		__auto_type find= strRulePSortedFind(terminals,rule,ptrCmp);
		if(find==NULL) {
		
		}
	}
}
struct __cykRule *grammarRuleSequence2CYK(const struct grammarRule *rules,
                                          long len, float prec) {
	for (long i = len - 1; i >= 0; i--) {
		if (rules[i].type == RULE_OPT) {
			if (i >= 1) {
				__auto_type res = grammarRuleSequence2CYK(rules, i, 1);
				__auto_type newRule =
				    cykRuleCreateNonterminal(strCYKRulesPSize(cykRules), 1,
				                             cykRuleValue(res), rules[i].cykRuleValue);
				cykRules = strCYKRulesPAppendItem(cykRules, newRule);
			} else {
				assert(0); // Whine about not being reducable
			}
		} else if (rules[i].type == RULE_REPEAT) {
			__auto_type res = grammarRuleSequence2CYK(rules, i, 1);
		}
	}
}
