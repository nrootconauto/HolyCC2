#include <assert.h>
#include <cykGrammarGenerator.h>
#include <hashTable.h>
#include <linkedList.h>
#include <stdarg.h>
#include <stdio.h>
#include <str.h>
enum ruleType {
	RULE_OPT,
	RULE_REPEAT,
	RULE_SEQUENCE,
	RULE_TERMINAL,
	RULE_COMPUTED,
};
struct grammarRule {
	enum ruleType type;
	float prec;
	strCYKRulesP cyk;
	char *name;
	// Data appended here
};
struct grammarRuleOpt {
	struct grammarRule base;
	const struct grammarRule *rule;
};
struct grammarRuleRepeat {
	struct grammarRule base;
	const struct grammarRule *rule;
};
struct grammarRuleSequence {
	struct grammarRule base;
	strRuleP rules;
};
struct grammarRuleComputed {
	struct grammarRule base;
};
struct grammarRuleTerminal {
	struct grammarRule base;
	int (*validate)(const void *, const void *);
	void (*destroyData)(void *);
	void *data;
};
static __thread strRuleP rules = NULL;
static __thread strCYKRulesP cykRules = NULL;
static __thread long maximumRuleValue = 0;
static __thread mapCYKRules cykRulesByName = NULL;
static int ptrCmp(const void *a, const void *b) {
	if (a > b)
		return 1;
	else if (a == b)
		return 0;
	else
		return -1;
}
static char *stringClone(const char *text) {
	__auto_type len = strlen(text);
	char *retVal = malloc(len + 1);
	strcpy(retVal, text);
	return retVal;
}
static const struct grammarRule *
registerRule2CYK(const struct grammarRule **rules, long len, float prec) {
	strCYKRulesP newCYKRules = NULL;
	const char *name = rules[len - 1]->name;

	__auto_type i = len - 1;
	if (rules[i]->type == RULE_OPT) {
		if (i >= 1) {
			/**
			 * New rule will contain an alias to (prev),and a rule ->(prev) (opt)
			 * will be added to the new rule
			 */
			__auto_type prevComputed = registerRule2CYK(rules, len - 1, 1);
			__auto_type optComputed = registerRule2CYK(rules + len - 1, 1, 1);
			maximumRuleValue++;
			for (long i = 0; i != strCYKRulesPSize(optComputed->cyk); i++) {
				for (long i2 = 0; i2 != strCYKRulesPSize(prevComputed->cyk); i2++) {
					//
					__auto_type opt = cykRuleValue(optComputed->cyk[i]);
					__auto_type prev = cykRuleValue(prevComputed->cyk[i2]);

					//(new)->(prev) (opt)
					__auto_type newCYKRule =
					    cykRuleCreateNonterminal(maximumRuleValue, prec, prev, opt);
					cykRules = strCYKRulesPAppendItem(cykRules, newCYKRule);
					newCYKRules = strCYKRulesPAppendItem(newCYKRules, newCYKRule);
				}
			}

			goto registerRule;
		} else {
			assert(0); // Whine about not being reducable
		}
	} else if (rules[i]->type == RULE_REPEAT) {
		__auto_type res = registerRule2CYK(rules, i, 1);

		maximumRuleValue++;
		for (long i = 0; i != strCYKRulesPSize(res->cyk); i++) {
			for (long i2 = 0; i2 != strCYKRulesPSize(res->cyk); i2++) {
				// (new)->(old) (old)
				__auto_type newRule = cykRuleCreateNonterminal(
				    maximumRuleValue, prec, cykRuleValue(res->cyk[i]),
				    cykRuleValue(res->cyk[i2]));

				cykRules = strCYKRulesPAppendItem(cykRules, newRule);
				newCYKRules = strCYKRulesPAppendItem(newCYKRules, newRule);
			}
		}
		goto registerRule;
	} else if (rules[i]->type == RULE_SEQUENCE) {
		struct grammarRuleSequence *seq = (struct grammarRuleSequence *)rules[i];
		long seqLen = strRulePSize(seq->rules);
		__auto_type front = registerRule2CYK(
		    (const struct grammarRule **)seq->rules + seqLen - 1, 1, 1);
		__auto_type back = registerRule2CYK((const struct grammarRule **)seq->rules,
		                                    seqLen - 1, 1);

		maximumRuleValue++;
		for (long i = 0; i != strCYKRulesPSize(front->cyk); i++) {
			for (long i2 = 0; i2 != strCYKRulesPSize(back->cyk); i2++) {
				__auto_type newRule = cykRuleCreateNonterminal(
				    maximumRuleValue, prec, cykRuleValue(back->cyk[i]),
				    cykRuleValue(front->cyk[i2]));

				cykRules = strCYKRulesPAppendItem(cykRules, newRule);
				newCYKRules = strCYKRulesPAppendItem(newCYKRules, newRule);
			}
		}

		goto registerRule;
	} else if (rules[i]->type == RULE_TERMINAL) {
		// TODO
	} else if (rules[i]->type == RULE_COMPUTED) {
		return rules[i];
	} else {
		assert(0);
	}
registerRule : {
	char *newName;
	if (len == 1) {
		newName = stringClone(name);
	} else {
		char newName2[1024];
		sprintf(newName2, "%s_R%li", name, maximumRuleValue);
		newName = stringClone(newName2);
	}
	struct grammarRuleComputed newRule;
	newRule.base.cyk = newCYKRules;
	newRule.base.prec = 1;
	newRule.base.type = RULE_COMPUTED;
	newRule.base.name = stringClone(newName);

	struct grammarRule *retVal = malloc(sizeof(struct grammarRuleComputed));
	*retVal = newRule.base;

loop:;
	__auto_type find = mapCYKRulesGet(cykRulesByName, newName);
	if (find == NULL) {
		mapCYKRulesInsert(cykRulesByName, newName, NULL);
		goto loop;
	} else {
		for (long i = 0; i != strCYKRulesPSize(newCYKRules); i++)
			*find = strCYKRulesPAppendItem(*find, newCYKRules[i]);
	}

	return retVal;
}
}
static void strCYKRulesPDestroy2(void *value) { strCYKRulesPDestroy(value); }
strCYKRulesP CYKGrammarCreate(const strRuleP grammar, mapCYKRules *retVal) {
	if (cykRulesByName != NULL)
		mapCYKRulesDestroy(cykRulesByName, strCYKRulesPDestroy2);
	strRulePDestroy(&rules);
	strCYKRulesPDestroy(&cykRules);
	rules = NULL;
	cykRules = NULL;

	for (long i = 0; i != strRulePSize(grammar); i++) {
		registerRule2CYK((const struct grammarRule **)grammar + i, 1,
		                 grammar[i]->prec);
	}

	strCYKRulesP retValCYK = (strCYKRulesP)__vecAppendItem(
	    NULL, cykRules, __vecSize((struct __vec *)cykRules));
	
	return retValCYK;
}
struct grammarRule *grammarRuleSequenceCreate(const char *name, double prec,
                                              ...) {
	struct grammarRuleSequence *retVal =
	    malloc(sizeof(struct grammarRuleSequence));
	retVal->base.prec = prec;
	retVal->base.type = RULE_SEQUENCE;
	retVal->base.cyk = NULL;
	retVal->base.name = stringClone(name);
	retVal->rules = NULL;

	va_list list;
	va_start(list, prec);
	for (int firstRun = 1;; firstRun = 0) {
		struct grammarRule *rule = va_arg(list, struct grammarRule *);
		if (rule != NULL)
			break;

		retVal->rules = strRulePAppendItem(retVal->rules, rule);
	}
	va_end(list);

	return (struct grammarRule *)retVal;
}
struct grammarRule *grammarRuleOptCreate(const char *name, double prec,
                                         const struct grammarRule *rule) {
	struct grammarRuleOpt newRule;
	newRule.base.prec = prec;
	newRule.base.cyk = NULL;
	newRule.base.type = RULE_OPT;
	newRule.base.name = stringClone(name);
	newRule.rule = rule;

	struct grammarRule *retVal = malloc(sizeof(newRule));
	return *(struct grammarRuleOpt *)retVal = newRule, retVal;
}
struct grammarRule *grammarRuleRepeatCreate(const char *name, double prec,
                                            const struct grammarRule *rule) {
	struct grammarRuleRepeat newRule;
	newRule.base.prec = prec;
	newRule.base.cyk = NULL;
	newRule.base.type = RULE_REPEAT;
	newRule.base.name = stringClone(name);
	newRule.rule = rule;

	struct grammarRule *retVal = malloc(sizeof(newRule));
	return *(struct grammarRuleRepeat *)retVal = newRule, retVal;
}
struct grammarRule *
grammarRuleTerminalCreate(const char *name, double prec,
                          int (*validate)(const void *, const void *),
                          void *data, void (*destroyData)(void *)) {
	struct grammarRuleTerminal newRule;
	newRule.base.prec = prec;
	newRule.base.cyk = NULL;
	newRule.base.type = RULE_TERMINAL;
	newRule.base.name = stringClone(name);
	newRule.validate = validate;
	newRule.data = data;
	newRule.destroyData = destroyData;

	struct grammarRule *retVal = malloc(sizeof(newRule));
	return *(struct grammarRuleTerminal *)retVal = newRule, retVal;
}
void grammarRuleDestroy(struct grammarRule **rule) {
	if (rule[0]->type == RULE_OPT || rule[0]->type == RULE_REPEAT) {
	} else if (rule[0]->type == RULE_SEQUENCE) {
		struct grammarRuleSequence **rule2 = (struct grammarRuleSequence **)rule;
		strRulePDestroy(&rule2[0]->rules);
	} else if (rule[0]->type == RULE_TERMINAL) {
		struct grammarRuleTerminal **rule2 = (struct grammarRuleTerminal **)rule;
		if (rule2[0]->destroyData != NULL)
			rule2[0]->destroyData(rule2[0]->data);
	} else if (rule[0]->type == RULE_COMPUTED) {

	} else {
		assert(0);
	}

	if (rule[0]->name != NULL)
		free(rule[0]->name);
	free(*rule);
}
