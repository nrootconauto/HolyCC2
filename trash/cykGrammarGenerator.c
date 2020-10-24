#include <assert.h>
#include <cykGrammarGenerator.h>
#include <hashTable.h>
#include <linkedList.h>
#include <stdarg.h>
#include <stdio.h>
#include <str.h>
enum ruleType {
	RULE_OPT,
	RULE_OR,
	RULE_REPEAT,
	RULE_SEQUENCE,
	RULE_TERMINAL,
	RULE_COMPUTED,
};
STR_TYPE_DEF(int, Int);
STR_TYPE_FUNCS(int, Int);
MAP_TYPE_DEF(struct grammarRule *, CYKRulePtrToGrammarRule);
MAP_TYPE_FUNCS(struct grammarRule *, CYKRulePtrToGrammarRule);
MAP_TYPE_DEF(void *, CYKNodeValueByPtr);
MAP_TYPE_FUNCS(void *, CYKNodeValueByPtr);
STR_TYPE_DEF(void *, TopData);
STR_TYPE_FUNCS(void *, TopData);
struct parsing {
	mapCYKNodeValueByPtr nodeValueByPtr;
	struct __cykBinaryMatrix *binaryTable;
	strGraphNodeCYKTreeP tops;
	strTopData topValues;
	struct __cykIterator iter;
	strGraphNodeCYKTreeP explored;
};
struct grammar {
	strCYKRulesP cykRules;
	mapCYKRules names;
	mapCYKRulePtrToGrammarRule rulePtrs;
	mapCYKRulePtrToGrammarRule terminalPtrs;
	strRuleP topLevelRules;
	strInt terminalCYKIndexes;
	long grammarSize;
};
struct grammarRule {
	enum ruleType type;
	float prec;
	strCYKRulesP cyk;
	void *(*callBack)(const mapTemplateData templates, const void *data);
	void *ruleData;
	void (*ruleDataDestroy)(void *);
	char *name;
	strInt leftMostRules;
	const char *templateName;
	// Data appended here
};
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
struct grammarRuleComputed {
	struct grammarRule base;
	strCYKRulesP startRules;
	const char *leftTemplateName;
	const char *rightTemplateName;
	const char *terminalTemplateName;
};
struct grammarRuleTerminal {
	struct grammarRule base;
	int (*validate)(const void *, const void *);
	void (*destroyData)(void *);
	void *data;
};
struct grammarRuleOr {
	struct grammarRule base;
	strRuleP canidates;
};
static __thread strRuleP rules = NULL;
static __thread strCYKRulesP cykRules = NULL;
static __thread long maximumRuleValue = 0;
static __thread mapCYKRules cykRulesByName = NULL;
static __thread mapCYKRulePtrToGrammarRule cykRulePtrToGrammarRule = NULL;
MAP_TYPE_DEF(struct grammarRule *, NormalToComputed);
MAP_TYPE_FUNCS(struct grammarRule *, NormalToComputed);
static __thread mapNormalToComputed ruleCache = NULL;
static mapCYKRulePtrToGrammarRule cykTerminalToRule = NULL;
static int ptrPtrCmp(const void *a, const void *b) {
	const void *a2 = *(const void **)a, *b2 = *(const void **)b;
	if (a2 > b2)
		return 1;
	else if (a2 == b2)
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
static int intCmp(const void *a, const void *b) {
	return *(int *)a - *(int *)b;
}
static struct grammarRule *
registerRule(const struct grammarRule *baseRule, const char *name,
             long uniqueName, strCYKRulesP newCYKRules2,
             const strInt leftMostRules, const char *leftTemplateName,
             const char *rightTemplateName, const char *terminalTemplateName) {
	char *newName;
	if (uniqueName == 1) {
		newName = stringClone(name);
	} else {
		char newName2[1024];
		sprintf(newName2, "%s_R%li", name, maximumRuleValue);
		newName = stringClone(newName2);
	}

	// Only insert if doesnt already exist
	for (long i = 0; i != strCYKRulesPSize(newCYKRules2); i++) {
		if (NULL == strCYKRulesPSortedFind(cykRules, newCYKRules2[i], ptrPtrCmp))
			cykRules = strCYKRulesPSortedInsert(cykRules, newCYKRules2[i], ptrPtrCmp);
	}

	struct grammarRuleComputed newRule;
	if (baseRule != NULL) {
		newRule.base.callBack = baseRule->callBack;
		newRule.base.prec = baseRule->prec;
		newRule.base.ruleData = baseRule->ruleData;
		newRule.base.ruleDataDestroy = baseRule->ruleDataDestroy;
	} else {
		newRule.base.callBack = NULL;
		newRule.base.prec = 1;
		newRule.base.ruleData = NULL;
		newRule.base.ruleDataDestroy = NULL;
	}

	newRule.base.cyk = newCYKRules2;
	newRule.base.type = RULE_COMPUTED;
	newRule.base.name = stringClone(newName);

	// Assign left-most rules
	__auto_type len = strIntSize(leftMostRules);
	newRule.base.leftMostRules =
	    strIntAppendData(NULL, leftMostRules, strIntSize(leftMostRules));
	qsort(newRule.base.leftMostRules, len, sizeof(*newRule.base.leftMostRules),
	      intCmp);

	struct grammarRule *retVal = malloc(sizeof(struct grammarRuleComputed));
	*retVal = newRule.base;

	/**
	 * Insert cykRules by name(only un-registered rules are cyk rules unique to
	 * grammar rule)
	 */
	for (long i = 0; i != strCYKRulesPSize(newCYKRules2); i++) {
		char buffer[64];
		sprintf(buffer, "%p", newCYKRules2[i]);
		if (NULL ==
		    mapCYKRulePtrToGrammarRuleGet(cykRulePtrToGrammarRule, buffer)) {
			printf("%i,%s\n", cykRuleValue(newCYKRules2[i]),
			       retVal->name); // TODO remo
			mapCYKRulePtrToGrammarRuleInsert(cykRulePtrToGrammarRule, buffer, retVal);
		}
	}

loop:;
	__auto_type find = mapCYKRulesGet(cykRulesByName, newName);
	if (find == NULL) {
		mapCYKRulesInsert(cykRulesByName, newName, NULL);
		goto loop;
	} else {
		for (long i = 0; i != strCYKRulesPSize(newCYKRules2); i++)
			*find = strCYKRulesPAppendItem(*find, newCYKRules2[i]);
	}

	maximumRuleValue++;
	return retVal;
}
static struct grammarRule *registerRule2CYK(struct grammarRule **rules,
                                            int index, long len, float prec,
                                            long *consumed,
                                            const char **templateName);
static struct grammarRule *registerRule2CYKSequenceRecur(
    const struct grammarRule *baseRule, struct grammarRule **rules,
    const char *name, long len, strInt *firstRules, const char **templateName) {
	strCYKRulesP newCYKRules = NULL;

	const char *frontTemplate, *backTemplate;

	long consumed2;
	strCYKRulesP firstRules2 __attribute__((cleanup(strCYKRulesPDestroy)));
	firstRules2 = NULL;
	__auto_type front =
	    registerRule2CYK(rules, len - 1, len, 1, &consumed2, &frontTemplate);

	const struct grammarRule *back = NULL;

	if (len - 1 - consumed2 > 0)
		back = registerRule2CYKSequenceRecur(NULL, rules, name, len - 1 - consumed2,
		                                     firstRules, &backTemplate);

	if (back != NULL) {
		for (long i = 0; i != strCYKRulesPSize(front->cyk); i++) {
			for (long i2 = 0; i2 != strCYKRulesPSize(back->cyk); i2++) {
				__auto_type newRule = cykRuleCreateNonterminal(
				    maximumRuleValue, 1, cykRuleValue(back->cyk[i2]),
				    cykRuleValue(front->cyk[i]));

				newCYKRules = strCYKRulesPAppendItem(newCYKRules, newRule);
			}
		}

		if (templateName != NULL)
			*templateName = NULL;

		return registerRule(baseRule, name, len == 1, newCYKRules, NULL,
		                    backTemplate, NULL, frontTemplate);
	} else {
		*firstRules = strIntAppendData(NULL, front->leftMostRules,
		                               strIntSize(front->leftMostRules));

		__auto_type len = strCYKRulesPSize(front->cyk);
		newCYKRules = strCYKRulesPResize(NULL, len);
		for (long i = 0; i != len; i++)
			newCYKRules[i] = front->cyk[i];

		if (templateName != NULL)
			*templateName = front->templateName;

		return front;
	}
}
/**
 * firstRules is the rules that apear at the left-most side of the parse
 */
static struct grammarRule *registerRule2CYK(struct grammarRule **rules,
                                            int index, long len, float prec,
                                            long *consumed,
                                            const char **templateName) {
	const char *leftTemplateName = NULL, *rightTemplateName = NULL,
	           *terminalTemplateName = NULL;

	if (templateName != NULL)
		*templateName = rules[index]->templateName;

	strCYKRulesP newCYKRules = NULL;
	const char *name = rules[len - 1]->name;
	int isLeftMostRule = 0;
	strInt leftRules = NULL;
	int mangleName = -1;
	int allowCache = -1;

	__auto_type i = index;

	char buffer[64];
	sprintf(buffer, "%p", rules[i]);
	__auto_type cached = mapNormalToComputedGet(ruleCache, buffer);

	if (rules[i]->type == RULE_OPT) {
		mangleName = 1;
		/**
		 * CANNOT use cached value as previous item is aliased to value of the
		 * resulting rule
		 */
		allowCache = 0;

		if (i >= 1) {
			const char *optTemplate, *prevTemplate;
			/**
			 * New rule will contain an alias to (prev),and a rule ->(prev) (opt)
			 * will be added to the new rule
			 */
			struct grammarRuleOpt *opt = (struct grammarRuleOpt *)rules[i];

			__auto_type optComputed =
			    registerRule2CYK(&opt->rule, 0, 1, 1, NULL, &optTemplate);
			long consumed2 = 0;

			__auto_type prevComputed =
			    registerRule2CYK(rules, i - 1, len - 1, 1, &consumed2, &prevTemplate);
			if (consumed != NULL)
				*consumed = consumed2;

			for (long i = 0; i != strCYKRulesPSize(optComputed->cyk); i++) {
				for (long i2 = 0; i2 != strCYKRulesPSize(prevComputed->cyk); i2++) {
					//
					__auto_type opt = cykRuleValue(optComputed->cyk[i]);
					__auto_type prev = cykRuleValue(prevComputed->cyk[i2]);

					//(new)->(prev) (opt)
					__auto_type newCYKRule =
					    cykRuleCreateNonterminal(maximumRuleValue, prec, prev, opt);
					newCYKRules = strCYKRulesPAppendItem(newCYKRules, newCYKRule);
				}
			}

			/**
			 * Make rule aliased to previous rules,so if opt isnt present,it will
			 * accept the previous item's rules
			 */
			newCYKRules = strCYKRulesPAppendData(
			    newCYKRules, (const struct __cykRule **)prevComputed->cyk,
			    strCYKRulesPSize(prevComputed->cyk));

			for (long i = 0; i != strCYKRulesPSize(prevComputed->cyk); i++)
				leftRules =
				    strIntAppendItem(leftRules, cykRuleValue(prevComputed->cyk[i]));
			isLeftMostRule = 1;

			leftTemplateName = prevTemplate;
			rightTemplateName = optTemplate;

			goto registerRule;
		} else {
			assert(0); // Whine about not being reducable
		}
	} else if (rules[i]->type == RULE_REPEAT) {
		mangleName = 0;
		const char *resTemplateName;
		/**
		 * Can use cached value as isn't dependant on previous items
		 */
		allowCache = 1;

		long consumed2 = 0;
		if (consumed != NULL)
			*consumed = consumed2;

		if (cached == NULL) {
			struct grammarRuleRepeat *repeat = (void *)rules[i];
			__auto_type res = registerRule2CYK(&repeat->rule, 0, 1, 1, &consumed2,
			                                   &resTemplateName);

			// (new)->(old) (old)
			for (long i = 0; i != strCYKRulesPSize(res->cyk); i++) {
				for (long i2 = 0; i2 != strCYKRulesPSize(res->cyk); i2++) {

					__auto_type newRule = cykRuleCreateNonterminal(
					    maximumRuleValue, prec, cykRuleValue(res->cyk[i]),
					    cykRuleValue(res->cyk[i2]));

					newCYKRules = strCYKRulesPAppendItem(newCYKRules, newRule);
				}
			}
			//(new)->(new) (old)
			for (long i = 0; i != strCYKRulesPSize(res->cyk); i++) {
				__auto_type newRule =
				    cykRuleCreateNonterminal(maximumRuleValue, prec, maximumRuleValue,
				                             cykRuleValue(res->cyk[i]));

				newCYKRules = strCYKRulesPAppendItem(newCYKRules, newRule);
			}

			for (long i = 0; i != strCYKRulesPSize(res->cyk); i++) {
				leftRules = strIntAppendItem(leftRules, cykRuleValue(res->cyk[i]));
				newCYKRules = strCYKRulesPAppendItem(newCYKRules, res->cyk[i]);
			}
			isLeftMostRule = 1;

			leftTemplateName = resTemplateName;
			rightTemplateName = resTemplateName;
			goto registerRule;
		} else {
			return *cached;
		}
	} else if (rules[i]->type == RULE_SEQUENCE) {
		mangleName = 0;
		/**
		 * Can use cached value as isn't dependant on previous items
		 */
		allowCache = 1;

		if (cached != NULL)
			return *cached;

		struct grammarRuleSequence *seq = (struct grammarRuleSequence *)rules[i];
		__auto_type rule = registerRule2CYKSequenceRecur(
		    rules[i], (struct grammarRule **)seq->rules, rules[i]->name,
		    strRulePSize(seq->rules), &leftRules, NULL);
		if (consumed != NULL)
			*consumed = len;

		return rule;
	} else if (rules[i]->type == RULE_TERMINAL) {
		terminalTemplateName = rules[i]->templateName;

		mangleName = 0;
		/**
		 * Can use cached value as isn't dependant on previous items
		 */
		allowCache = 1;

		if (consumed != NULL)
			*consumed = 1;

		if (cached == NULL) {
			newCYKRules = strCYKRulesPAppendItem(
			    newCYKRules, cykRuleCreateTerminal(maximumRuleValue, prec));
			leftRules = strIntAppendItem(leftRules, cykRuleValue(newCYKRules[0]));

			for (long i2 = 0; i2 != strCYKRulesPSize(newCYKRules); i2++) {
				char buffer[64];
				sprintf(buffer, "%p", newCYKRules[i2]);
				if (NULL == mapCYKRulePtrToGrammarRuleGet(cykTerminalToRule, buffer))
					mapCYKRulePtrToGrammarRuleInsert(cykTerminalToRule, buffer, rules[i]);
			}
			goto registerRule;
		} else
			return *cached;

	} else if (rules[i]->type == RULE_COMPUTED) {
		return (struct grammarRule *)rules[i];
	} else {
		assert(0);
	}
registerRule : {
	if (cached != NULL && allowCache) {
		strCYKRulesPDestroy(&newCYKRules);
		strIntDestroy(&leftRules);
		return *cached;
	}

	__auto_type computed = registerRule(rules[i], name, mangleName, newCYKRules,
	                                    leftRules, NULL, NULL, NULL);

	sprintf(buffer, "%p", rules[i]);
	mapNormalToComputedInsert(ruleCache, buffer, computed);

	return computed;
}
}
static void strCYKRulesPClone(void *a, const void *b) {
	*(strCYKRulesP *)a = (strCYKRulesP)__vecAppendItem(NULL, b, __vecSize(b));
}
void mapCYKRulesDestroy2(mapCYKRules *map) {
	mapCYKRulesDestroy(*map, (void (*)(void *))strCYKRulesPDestroy2);
}
struct grammar *grammarCreate(const strRuleP grammarRules) {
	if (cykRulePtrToGrammarRule != NULL)
		mapCYKRulePtrToGrammarRuleDestroy(cykRulePtrToGrammarRule, NULL);
	if (cykRulesByName != NULL)
		mapCYKRulesDestroy(cykRulesByName, (void (*)(void *))strCYKRulesPDestroy2);
	if (ruleCache != NULL)
		mapNormalToComputedDestroy(ruleCache, NULL);
	if (cykTerminalToRule != NULL)
		mapCYKRulePtrToGrammarRuleDestroy(cykTerminalToRule, NULL);

	strRulePDestroy(&rules);
	strCYKRulesPDestroy(&cykRules);
	rules = NULL;
	cykRules = NULL;
	ruleCache = mapNormalToComputedCreate();
	maximumRuleValue = 0;
	cykRulesByName = mapCYKRulePtrToGrammarRuleCreate();
	cykRulePtrToGrammarRule = mapCYKRulePtrToGrammarRuleCreate();
	cykTerminalToRule = mapCYKRulePtrToGrammarRuleCreate();

	strRuleP grammarRules2 __attribute__((cleanup(strRulePDestroy)));
	grammarRules2 = NULL;
	for (long i = 0; i != strRulePSize(grammarRules); i++) {
		__auto_type newRule =
		    registerRule2CYK((struct grammarRule **)grammarRules + i, 0, 1,
		                     grammarRules[i]->prec, NULL, NULL);

		// Ignore cached rules;
		if (NULL == strRulePSortedFind(grammarRules2, newRule, ptrPtrCmp))
			grammarRules2 = strRulePSortedInsert(grammarRules2, newRule, ptrPtrCmp);
	}

	CYKRulesRemoveRepeats(&cykRules);

	struct grammar *retVal = malloc(sizeof(struct grammar));
	retVal->cykRules = (strCYKRulesP)__vecAppendItem(
	    NULL, cykRules, __vecSize((struct __vec *)cykRules));
	retVal->names = mapCYKRulesClone(cykRulesByName, strCYKRulesPClone);
	retVal->terminalCYKIndexes = NULL;
	retVal->grammarSize = maximumRuleValue + 1;
	retVal->rulePtrs = mapCYKNodeValueByPtrClone(cykRulePtrToGrammarRule, NULL);
	retVal->terminalPtrs =
	    mapCYKRulePtrToGrammarRuleClone(cykTerminalToRule, NULL);
	retVal->topLevelRules = (strRuleP)__vecAppendItem(
	    NULL, grammarRules2, __vecSize((struct __vec *)grammarRules2));

	for (long i = 0; i != strCYKRulesPSize(retVal->cykRules); i++) {
		if (CYKRuleIsTerminal(retVal->cykRules[i])) {
			retVal->terminalCYKIndexes =
			    strIntAppendItem(retVal->terminalCYKIndexes, i);
		}
	}

	return retVal;
}
struct grammarRule *grammarRuleSequenceCreate(const char *name,
                                              const char *templateName,
                                              double prec, ...) {
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
		if (rule == NULL)
			break;

		retVal->rules = strRulePAppendItem(retVal->rules, rule);
	}
	va_end(list);

	return (struct grammarRule *)retVal;
}
struct grammarRule *grammarRuleOrCreate(const char *name,
                                        const char *templateName, double prec,
                                        const strRuleP canidates) {
	struct grammarRuleOr *retVal = malloc(sizeof(struct grammarRuleOr));
	retVal->base.callBack = NULL;
	retVal->base.cyk = NULL;
	retVal->base.leftMostRules = NULL;
	retVal->base.name = stringClone(templateName);
	retVal->base.prec = prec;
	retVal->base.ruleData = NULL;
	retVal->base.ruleDataDestroy = NULL;
	retVal->base.type = RULE_OR;

	retVal->canidates = strRulePAppendData(
	    NULL, (const struct grammarRule **)canidates, strRulePSize(canidates));

	return (struct grammarRule *)retVal;
}
void grammarRuleSetCallback(struct grammarRule *rule,
                            void *(*cb)(const mapTemplateData, const void *),
                            void *data) {
	rule->callBack = cb;
	rule->ruleData = data;
}
struct grammarRule *grammarRuleOptCreate(const char *name,
                                         const char *templateName, double prec,
                                         struct grammarRule *rule) {
	struct grammarRuleOpt newRule;
	newRule.base.prec = prec;
	newRule.base.cyk = NULL;
	newRule.base.type = RULE_OPT;
	newRule.base.name = stringClone(name);
	newRule.rule = rule;

	struct grammarRule *retVal = malloc(sizeof(newRule));
	return *(struct grammarRuleOpt *)retVal = newRule, retVal;
}
struct grammarRule *grammarRuleRepeatCreate(const char *name,
                                            const char *templateName,
                                            double prec,
                                            struct grammarRule *rule) {
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
grammarRuleTerminalCreate(const char *name, const char *templateName,
                          double prec,
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
	if (rule[0]->ruleDataDestroy)
		rule[0]->ruleDataDestroy(rule[0]->ruleData);
	free(*rule);
}
static strCYKRulesP grammarTerminalsValidate(const void *item,
                                             const void *grammar) {
	const struct grammar *grammar2 = grammar;
	strCYKRulesP results = NULL;

	for (long i = 0; i != strIntSize(grammar2->terminalCYKIndexes); i++) {
		__auto_type term = grammar2->cykRules[grammar2->terminalCYKIndexes[i]];

		char buffer[64];
		sprintf(buffer, "%p", term);
		__auto_type find =
		    (struct grammarRuleTerminal **)mapCYKRulePtrToGrammarRuleGet(
		        grammar2->terminalPtrs, buffer);
		assert(find != NULL);

		if (find[0]->validate(item, find[0]->data))
			results = strCYKRulesPAppendItem(results, term);
	}

	return results;
}
void grammarDestroy(struct grammar **grammar) {
	strCYKRulesPDestroy2(&grammar[0]->cykRules);
	mapCYKRulesDestroy2(&grammar[0]->names);
	strIntDestroy(&grammar[0]->terminalCYKIndexes);
}
static void *updateNodeValue(const graphNodeCYKTree node,
                             const struct grammar *grammar);
static const char *templateNameFromNode(const struct grammar *grammar,
                                        const graphNodeCYKTree node) {
	__auto_type entry = graphNodeCYKTreeValuePtr(node);
	char ptrStr[64];
	sprintf(ptrStr, "%p", entry->rule);

	__auto_type find = mapCYKRulePtrToGrammarRuleGet(grammar->rulePtrs, ptrStr);
	assert(find != NULL);

	return find[0]->templateName;
}
static void *registerTemplateItem(const struct grammar *grammar,
                                  mapTemplateData templates,
                                  const graphNodeCYKTree node, int computeLeft,
                                  int computeRight) {
	__auto_type templateName = templateNameFromNode(grammar, node);

	__auto_type outGoing = graphNodeCYKTreeOutgoing(node);
	for (long i = 0; i != strGraphEdgeCYKTreePSize(outGoing); i++) {
		__auto_type node = graphEdgeCYKTreeOutgoing(outGoing[i]);
		__auto_type templateName = templateNameFromNode(grammar, node);

		if (templateName == NULL)
			continue;
		// Assert one rule of a name
		assert(NULL == mapTemplateDataGet(templates, templateName));

		void *data = NULL;
		if (i == 1 && computeRight) {
			data = updateNodeValue(node, grammar);
		} else if (i == 0 && computeLeft) {
			data = updateNodeValue(node, grammar);
		}

		mapTemplateDataInsert(templates, templateName, data);
	}
	strGraphEdgeCYKTreePDestroy(&outGoing);
}
static void *updateNodeValue(const graphNodeCYKTree node,
                             const struct grammar *grammar) {
	char ptrStr[128];
	sprintf(ptrStr, "%p", graphNodeCYKTreeValuePtr(node)->rule);
	__auto_type find = mapCYKRulePtrToGrammarRuleGet(grammar->rulePtrs, ptrStr);
	assert(find != NULL);
	if (find[0]->callBack == NULL)
		return NULL;

	strGraphNodeP rules = NULL;

	{
		for (graphNodeCYKTree node2 = node; node2 != NULL;) {
			__auto_type nodeRule = graphNodeCYKTreeValuePtr(node2)->rule;

			__auto_type sides = graphNodeCYKTreeOutgoing(node2);

			// If hit terminal quit
			if (strGraphEdgeCYKTreePSize(sides) == 0) {
				rules = strGraphNodePAppendItem(rules, node2);
				strGraphEdgeCYKTreePDestroy(&sides);
				goto breakLoop;
			}
			// If hit left-most rule of sequenct quit

			// (Every rule is unique)
			if (NULL != strIntSortedFind(find[0]->leftMostRules,
			                             cykRuleValue(nodeRule), intCmp)) {
				rules = strGraphNodePAppendItem(rules, node2);
				strGraphEdgeCYKTreePDestroy(&sides);
				goto breakLoop;
			}
			// Go to left-side node
			node2 = graphEdgeCYKTreeOutgoing(sides[0]);

			__auto_type rightSide = graphEdgeCYKTreeOutgoing(sides[1]);
			rules = strGraphNodePAppendItem(rules, rightSide);

			strGraphEdgeCYKTreePDestroy(&sides);
		}
	}
breakLoop:;
	/**
	 * Walk back up tree till reaches initial node,while registering items who
	 * have a template
	 */
	mapTemplateData templates = mapTemplateDataCreate();
	for (long i = 0; i != strGraphNodePSize(rules); i++) {
		registerTemplateItem(grammar, templates, rules[i], i == 0, 1);
	}
	__auto_type data = find[0]->callBack(templates, find[0]->ruleData);

	mapTemplateDataDestroy(templates, NULL); // TODO free me
	strGraphNodePDestroy(&rules);

	return data;
}
STR_TYPE_DEF(struct __CYKEntry, CYKEntry);
STR_TYPE_FUNCS(struct __CYKEntry, CYKEntry);
static int cykEntryPred(const void *a, const void *b) {
	const struct __CYKEntry *a2 = a, *b2 = b;
	if (a2->y != b2->y)
		return a2->y - b2->y;
	if (a2->x != b2->x)
		return a2->x - b2->x;
	if (a2->r != b2->r)
		return a2->r - b2->r;
	return 0;
}
static int visitPred(const struct __graphNode *node,
                     const struct __graphEdge *edge, const void *data) {
	const strCYKEntry *visited = (void *)data;
	return NULL == strCYKEntrySortedFind(
	                   *visited,
	                   *graphNodeCYKTreeValuePtr((graphNodeCYKTree)node),
	                   cykEntryPred);
}
static void addToVisited(struct __graphNode *node, void *data) {
	strCYKEntry *visited = (void *)data;
	*visited = strCYKEntryAppendItem(
	    *visited, *graphNodeCYKTreeValuePtr((graphNodeCYKTree)node));
}
/**
 * Returns true if cordnate exists in area covered in existing CYK parses
 */
static int existsInDomain(const strCYKEntry tops, int x, int y) {
	for (long i = 0; i != strCYKEntrySize(tops); i++) {
		if (tops[i].x >= x) {
			long diff = x - tops[i].x;
			if (y <= tops[i].y - diff)
				return 1;
		}
	}
	return 0;
}
struct parsing *grammarCreateParsingFromData(struct grammar *grammar,
                                             struct __vec *items,
                                             long itemSize) {
	__auto_type grammarSize = grammar->grammarSize;
	struct parsing *retVal = malloc(sizeof(struct parsing));
	retVal->binaryTable =
	    __cykBinary(grammar->cykRules, items, itemSize, grammarSize,
	                grammarTerminalsValidate, grammar);
	retVal->nodeValueByPtr = mapCYKNodeValueByPtrCreate();
	retVal->explored = NULL;
	retVal->tops = NULL;
	retVal->topValues = NULL;

	strCYKEntry visited = NULL;
	strCYKEntry tops = NULL;
	if (__cykIteratorInitEnd(retVal->binaryTable, &retVal->iter)) {
		for (int firstRun = 1;; firstRun = 0) {
			for (long i = 0; i != strRulePSize(grammar->topLevelRules); i++) {
				__auto_type cyk = &grammar->topLevelRules[i]->cyk;
				for (long i2 = 0; i2 != strCYKRulesPSize(*cyk); i2++) {
					if (retVal->iter.r == cykRuleValue(cyk[0][i2])) {
						// Check ig already visited
						struct __CYKEntry dummy;
						dummy.r = retVal->iter.r, dummy.x = retVal->iter.x,
						dummy.y = retVal->iter.y;
						if (NULL != strCYKEntrySortedFind(visited, dummy, cykEntryPred))
							continue;

						/**
						 * Checks if current entry is in area covered by an existing parse
						 */
						if (existsInDomain(tops, retVal->iter.x, retVal->iter.y))
							break;

						// Compute node value.
						__auto_type node = CYKTree(grammar->cykRules, grammarSize, items,
						                           retVal->binaryTable, retVal->iter.y,
						                           retVal->iter.x, retVal->iter.r, itemSize,
						                           grammarTerminalsValidate, grammar);
						retVal->tops = strGraphNodeCYKTreePAppendItem(retVal->tops, node);
						retVal->topValues = strTopDataAppendItem(
						    retVal->topValues, updateNodeValue(node, grammar));

						// Add visited nodes to visited
						graphNodeCYKTreeVisitForward(node, &visited, visitPred,
						                             addToVisited);

						tops = strCYKEntryAppendItem(tops, dummy);
						/**
						 * Go to next matrix entry,multiple rules may share same value,so
						 * move to next x/y
						 */
						break;
					}
				}
			}

			if (!__cykIteratorPrev(retVal->binaryTable, &retVal->iter))
				break;
		}
	}

	strCYKEntryDestroy(&tops);
	return retVal;
}
const strGraphNodeCYKTreeP
grammarParsingGetTops(const struct parsing *parsing) {
	return parsing->tops;
}
static const char *getRuleName(int ruleNumber, const void *data) {
	const struct grammar *grammar = data;
	long i;
	for (i = 0; i != strCYKRulesPSize(grammar->cykRules); i++)
		if (cykRuleValue(grammar->cykRules[i]) == ruleNumber)
			break;
	assert(i != strCYKRulesPSize(grammar->cykRules));

	char buffer[64];
	sprintf(buffer, "%p", grammar->cykRules[i]);
	__auto_type find = mapCYKRulePtrToGrammarRuleGet(grammar->rulePtrs, buffer);
	assert(find != NULL);

	return find[0]->name;
}
void grammarPrint(const struct grammar *grammar) {
	CYKRulesPrint(grammar->cykRules, getRuleName, grammar);
}
