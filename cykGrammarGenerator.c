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
STR_TYPE_DEF(int, Int);
STR_TYPE_FUNCS(int, Int);
MAP_TYPE_DEF(struct grammarRule *, CYKRulePtrToGrammarRule);
MAP_TYPE_FUNCS(struct grammarRule *, CYKRulePtrToGrammarRule);
MAP_TYPE_DEF(void *, CYKNodeValueByPtr);
MAP_TYPE_FUNCS(void *, CYKNodeValueByPtr);
struct parsing {
	mapCYKNodeValueByPtr nodeValueByPtr;
	struct __cykBinaryMatrix *binaryTable;
	strGraphNodeCYKTreeP tops;
	struct __cykIterator iter;
	strGraphNodeCYKTreeP explored;
};
struct grammar {
	strCYKRulesP cykRules;
	mapCYKRules names;
	mapCYKRulePtrToGrammarRule rulePtrs;
	strRuleP allRules;
	strInt terminalRuleIndexes;
	long maximumRuleValue;
};
struct grammarRule {
	enum ruleType type;
	float prec;
	strCYKRulesP cyk;
	void *(*callBack)(const mapTemplateData templates, const void *data);
	void *ruleData;
	void (*ruleDataDestroy)(void *);
	char *name;
	char *templateName;
	strInt leftMostRules;
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
	strCYKRulesP startRules;
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
static int intCmp(const void *a, const void *b) {
	return *(int *)a - *(int *)b;
}
static struct grammarRule *
registerRule(const char *name, const char *templateName, long uniqueName,
             strCYKRulesP newCYKRules, strCYKRulesP leftMostRules,
             int isLeftMost) {
	char *newName;
	if (uniqueName == 1) {
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
	if (templateName != NULL)
		newRule.base.templateName = stringClone(templateName);
	else
		newRule.base.templateName = NULL;
	if (isLeftMost) {
		__auto_type len = strCYKRulesPSize(leftMostRules);
		newRule.base.leftMostRules = strIntResize(NULL, len);
		for (long i = 0; i != len; i++)
			newRule.base.leftMostRules[i] = cykRuleValue(leftMostRules[i]);
		qsort(newRule.base.leftMostRules, len, sizeof(*newRule.base.leftMostRules),
		      intCmp);
	} else
		newRule.base.leftMostRules = NULL;

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
static struct grammarRule *registerRule2CYK(const struct grammarRule **rules,
                                            long len, float prec,
                                            strCYKRulesP *firstRules,
                                            long *consumed);
static struct grammarRule *
registerRule2CYKSequenceRecur(const struct grammarRule **rules, long len,
                              strCYKRulesP *firstRules) {
	strCYKRulesP newCYKRules;

	long consumed2;
	__auto_type front = registerRule2CYK(rules, len, 1, firstRules, &consumed2);

	const struct grammarRule *back = NULL;
	if (len != 1)
		back = registerRule2CYKSequenceRecur(rules, len - consumed2, firstRules);

	for (long i = 0; i != strCYKRulesPSize(front->cyk); i++) {
		for (long i2 = 0; i2 != strCYKRulesPSize(back->cyk); i2++) {
			__auto_type newRule = cykRuleCreateNonterminal(
			    maximumRuleValue, 1, cykRuleValue(back->cyk[i]),
			    cykRuleValue(front->cyk[i2]));

			cykRules = strCYKRulesPAppendItem(cykRules, newRule);
			newCYKRules = strCYKRulesPAppendItem(newCYKRules, newRule);
		}
	}

	return registerRule(front->name, NULL, len != 1, newCYKRules,
	                    (len == 1) ? *firstRules : NULL, len == 1);
}
/**
 * firstRules is the rules that apear at the left-most side of the parse
 */
static struct grammarRule *registerRule2CYK(const struct grammarRule **rules,
                                            long len, float prec,
                                            strCYKRulesP *_firstRules,
                                            long *consumed) {
	strCYKRulesP newCYKRules = NULL;
	const char *name = rules[len - 1]->name;
	int isLeftMostRule = 0;
	strCYKRulesP leftRules = NULL;

	__auto_type i = len - 1;
	if (rules[i]->type == RULE_OPT) {
		if (i >= 1) {
			/**
			 * New rule will contain an alias to (prev),and a rule ->(prev) (opt)
			 * will be added to the new rule
			 */
			long consumed2 = 0;
			__auto_type optComputed =
			    registerRule2CYK(rules + len - 1, 1, 1, NULL, &consumed2);
			long consumed3 = 0;
			__auto_type prevComputed =
			    registerRule2CYK(rules, len - 1 - consumed2, 1, NULL, &consumed3);
			if (consumed != NULL)
				*consumed = consumed2 + consumed3;

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

			for (long i = 0; i != strCYKRulesPSize(prevComputed->cyk); i++)
				leftRules = strCYKRulesPAppendItem(leftRules, prevComputed->cyk[i]);
			isLeftMostRule = 1;

			goto registerRule;
		} else {
			assert(0); // Whine about not being reducable
		}
	} else if (rules[i]->type == RULE_REPEAT) {
		long consumed2 = 0;
		__auto_type res = registerRule2CYK(rules, i, 1, NULL, &consumed2);
		if (consumed != NULL)
			*consumed = consumed2;

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

		for (long i = 0; i != strCYKRulesPSize(res->cyk); i++)
			leftRules = strCYKRulesPAppendItem(leftRules, res->cyk[i]);
		isLeftMostRule = 1;

		goto registerRule;
	} else if (rules[i]->type == RULE_SEQUENCE) {
		__auto_type rule = registerRule2CYKSequenceRecur(rules, len, _firstRules);
		if (consumed != NULL)
			*consumed = len;

		rule->templateName = stringClone(rules[i]->templateName);

		if (i == 0 && _firstRules != NULL)
			leftRules = *_firstRules;
		return rule;
	} else if (rules[i]->type == RULE_TERMINAL) {
		// TODO
	} else if (rules[i]->type == RULE_COMPUTED) {
		return (struct grammarRule *)rules[i];
	} else {
		assert(0);
	}
registerRule:
	return registerRule(name, rules[i]->templateName, len == 1, newCYKRules,
	                    leftRules, len == 1);
}
static void strCYKRulesPClone(void *a, const void *b) {
	*(strCYKRulesP *)a = (strCYKRulesP)__vecAppendItem(NULL, b, __vecSize(b));
}
void mapCYKRulesDestroy2(mapCYKRules *map) {
	mapCYKRulesDestroy(*map, (void (*)(void *))strCYKRulesPDestroy2);
}
struct grammar *grammarCreate(const strRuleP grammarRules) {
	if (cykRulesByName != NULL)
		mapCYKRulesDestroy(cykRulesByName, (void (*)(void *))strCYKRulesPDestroy2);
	strRulePDestroy(&rules);
	strCYKRulesPDestroy(&cykRules);
	rules = NULL;
	cykRules = NULL;
	maximumRuleValue = 0;

	for (long i = 0; i != strRulePSize(grammarRules); i++) {
		registerRule2CYK((const struct grammarRule **)grammarRules + i, 1,
		                 grammarRules[i]->prec, NULL, NULL);
	}

	struct grammar *retVal = malloc(sizeof(grammarRules));
	retVal->allRules = (strRuleP)__vecAppendItem(
	    NULL, grammarRules, __vecSize((struct __vec *)grammarRules));
	retVal->cykRules = (strCYKRulesP)__vecAppendItem(
	    NULL, cykRules, __vecSize((struct __vec *)cykRules));
	retVal->names = mapCYKRulesClone(cykRulesByName, strCYKRulesPClone);
	retVal->terminalRuleIndexes = NULL;
	retVal->maximumRuleValue = maximumRuleValue;

	for (long i = 0; i != strRulePSize(retVal->allRules); i++) {
		if (retVal->allRules[i]->type == RULE_TERMINAL) {
			retVal->terminalRuleIndexes =
			    strIntAppendItem(retVal->terminalRuleIndexes, i);
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
	retVal->base.templateName = stringClone(templateName);
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
struct grammarRule *grammarRuleOptCreate(const char *name,
                                         const char *templateName, double prec,
                                         const struct grammarRule *rule) {
	struct grammarRuleOpt newRule;
	newRule.base.prec = prec;
	newRule.base.cyk = NULL;
	newRule.base.type = RULE_OPT;
	newRule.base.name = stringClone(name);
	newRule.base.templateName = stringClone(templateName);
	newRule.rule = rule;

	struct grammarRule *retVal = malloc(sizeof(newRule));
	return *(struct grammarRuleOpt *)retVal = newRule, retVal;
}
struct grammarRule *grammarRuleRepeatCreate(const char *name,
                                            const char *templateName,
                                            double prec,
                                            const struct grammarRule *rule) {
	struct grammarRuleRepeat newRule;
	newRule.base.prec = prec;
	newRule.base.cyk = NULL;
	newRule.base.type = RULE_REPEAT;
	newRule.base.name = stringClone(name);
	newRule.base.templateName = stringClone(templateName);
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
	newRule.base.templateName = stringClone(templateName);
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
	if (rule[0]->templateName != NULL)
		free(rule[0]->templateName);
	if (rule[0]->ruleDataDestroy)
		rule[0]->ruleDataDestroy(rule[0]->ruleData);
	free(*rule);
}
static strCYKRulesP grammarTerminalsValidate(const void *item,
                                             const void *grammar) {
	const struct grammar *grammar2 = grammar;
	strCYKRulesP results = NULL;

	for (long i = 0; i != strIntSize(grammar2->terminalRuleIndexes); i++) {
		struct grammarRuleTerminal *term =
		    (struct grammarRuleTerminal *)grammar2->allRules[i];

		if (term->validate(item, term->data))
			for (long i2 = 0; i2 != strCYKRulesPSize(term->base.cyk); i2++)
				results = strCYKRulesPAppendItem(results, term->base.cyk[i2]);
	}

	return results;
}
void grammarDestroy(struct grammar **grammar) {
	strRulePDestroy(&grammar[0]->allRules);
	strCYKRulesPDestroy2(&grammar[0]->cykRules);
	mapCYKRulesDestroy2(&grammar[0]->names);
	strIntDestroy(&grammar[0]->terminalRuleIndexes);
}
static void *updateNodeValue(const graphNodeCYKTree node,
                             struct parsing *parsing,
                             const struct grammar *grammar) {

	char ptrStr[128];
	sprintf(ptrStr, "%p", graphNodeCYKTreeValuePtr(node)->rule);
	__auto_type find = mapCYKRulePtrToGrammarRuleGet(grammar->rulePtrs, ptrStr);
	assert(find != NULL);
	if (find[0]->callBack == NULL)
		return NULL;

	strGraphNodeP leftRules = NULL;
	{
		for (graphNodeCYKTree node2 = node; node2 != NULL;) {
			__auto_type nodeRule = graphNodeCYKTreeValuePtr(node2)->rule;
			leftRules = strGraphNodePAppendItem(leftRules, node2);

			// Every rule is unique
			if (NULL != strIntSortedFind(find[0]->leftMostRules,
			                             cykRuleValue(nodeRule), intCmp))
				goto breakLoop;

			// Go to left-side node
			__auto_type leftSide = graphNodeCYKTreeOutgoing(node);
			node2 = graphEdgeCYKTreeOutgoing(leftSide[0]);
			strGraphEdgeCYKTreePDestroy(&leftSide);
		}
	}
breakLoop:;
	/**
	 * Walk back up tree till reaches initial node,while registering items who
	 * have a template
	 */
	mapTemplateData templates = mapTemplateDataCreate();
	for (long i = strGraphNodePSize(leftRules) - 1; i >= 0; i--) {
		char buffer[64];
		sprintf(buffer, "%p", leftRules[i]);
		__auto_type find = mapCYKRulePtrToGrammarRuleGet(grammar->rulePtrs, buffer);
		assert(find != NULL);

		if (find[0]->templateName != NULL) {
		loop:;
			__auto_type find2 = mapTemplateDataGet(parsing->nodeValueByPtr, buffer);
			if (find2 != NULL) {
				// Check if node was visited,
				if (NULL == strGraphNodeCYKTreePSortedFind(parsing->explored,
				                                           leftRules[i], ptrCmp)) {
					// Visit node
					parsing->explored = strGraphNodeCYKTreePSortedInsert(
					    parsing->explored, leftRules[i], ptrCmp);
					updateNodeValue(leftRules[i], parsing, grammar);
					goto loop;
				}
			}

			sprintf(buffer, "%p", *find2);
			__auto_type nodeValue =
			    mapCYKNodeValueByPtrGet(parsing->nodeValueByPtr, buffer);
			mapTemplateDataInsert(templates, find[0]->templateName, nodeValue);
		}
	}

	char buffer[64];
	sprintf(buffer, "%p", graphNodeCYKTreeValuePtr(node));
	__auto_type rule = mapCYKRulePtrToGrammarRuleGet(grammar->rulePtrs, buffer);
	assert(rule != NULL);
	rule[0]->callBack(templates, rule[0]->ruleData);

	strGraphNodePDestroy(&leftRules);
	mapTemplateDataDestroy(templates, NULL);
	return templates;
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
struct parsing *grammarCreateParsingFromData(struct grammar *grammar,
                                             struct __vec *items,
                                             const strRuleP topLevelRules,
                                             long itemSize) {
	struct parsing *retVal = malloc(sizeof(struct parsing));
	retVal->binaryTable =
	    __cykBinary(grammar->cykRules, items, itemSize, grammar->maximumRuleValue,
	                grammarTerminalsValidate, grammar);
	retVal->nodeValueByPtr = mapCYKNodeValueByPtrCreate();
	retVal->explored = NULL;
	retVal->tops = NULL;

	strCYKEntry visited = NULL;
	if (__cykIteratorInitEnd(retVal->binaryTable, &retVal->iter)) {
		for (int firstRun = 1;; firstRun = 0) {
			for (long i = 0; i != strRulePSize(topLevelRules); i++) {
				__auto_type cyk = &topLevelRules[i]->cyk;
				for (long i2 = 0; i2 != strCYKRulesPSize(*cyk); i2++) {
					if (retVal->iter.r == cykRuleValue(cyk[0][i2])) {
						// Check ig already visited
						struct __CYKEntry dummy;
						dummy.r = retVal->iter.r, dummy.x = retVal->iter.x,
						dummy.y = retVal->iter.y;
						if (NULL != strCYKEntrySortedFind(visited, dummy, cykEntryPred))
							continue;

						// Compute node value.
						__auto_type node = CYKTree(
						    grammar->cykRules, grammar->maximumRuleValue, items,
						    retVal->binaryTable, retVal->iter.y, retVal->iter.x,
						    retVal->iter.r, itemSize, grammarTerminalsValidate, grammar);
						retVal->tops = strGraphNodeCYKTreePAppendItem(retVal->tops, node);
						updateNodeValue(node, retVal, grammar);

						// Add visited nodes to visited
						graphNodeCYKTreeVisitForward(node, &visited, visitPred,
						                             addToVisited);
					}
				}
			}

			if (!__cykIteratorPrev(retVal->binaryTable, &retVal->iter))
				break;
		}
	}

	return retVal;
}
