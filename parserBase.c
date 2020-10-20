#include <assert.h>
#include <graph.h>
#include <hashTable.h>
#include <linkedList.h>
#include <parserBase.h>
#include <stdarg.h>
#include <str.h>
enum ruleType {
	RULE_REPEAT,
	RULE_OPT,
	RULE_SEQUENCE,
	RULE_TERMINAL,
	RULE_OR,
	RULE_ALIAS,
};
STR_TYPE_DEF(struct rule *, RuleP);
STR_TYPE_FUNCS(struct rule *, RuleP);
struct grammar {
	strRuleP rules;
	struct rule *top;
};
struct rule {
	double prec;
	const char *name;
	enum ruleType type;
};
struct ruleOr {
	struct rule base;
	strRuleP rules;
};
struct ruleTerminal {
	struct rule base;
	void *(*func)(const struct __lexerItem *);
};
struct ruleForward {
	struct rule base;
	const struct rule *alias;
};
struct ruleRepeat {
	struct rule base;
	const struct rule *rule;
	void *(*func)(const void **, long);
};
struct ruleOpt {
	struct rule base;
	const struct rule *rule;
	void *(*func)(const void *);
};
struct ruleSequence {
	struct rule base;
	strRuleP rules;
	void *(*func)(const void **, long);
};
STR_TYPE_DEF(void *, Ptr);
STR_TYPE_FUNCS(void *, Ptr);
static llLexerItem moveForward(llLexerItem from, long amount) {
	for (long i = 0; i != amount; i++) {
		if (from == NULL)
			break;
		from = llLexerItemNext(from);
	}
	return from;
}
static void *__parse(const struct rule *top, llLexerItem lexerItemNode,
                     long *consumed, int *success, void (*killData)(void *)) {
	if (lexerItemNode == NULL) {
		if (consumed != NULL)
			*consumed = 0;
		if (success != NULL)
			*success = 0;
		return NULL;
	}
	switch (top->type) {
	case RULE_OPT: {
		const struct ruleOpt *opt = (const void *)top;
		long consumed2;
		int success2 = 0;
		void *optValue =
		    __parse(opt->rule, lexerItemNode, &consumed2, &success2, killData);
		if (success2) {
			if (consumed != NULL)
				*consumed = consumed2;
			if (success != NULL)
				*success = 1;

			void *retVal = NULL;
			if (opt->func != NULL)
				retVal = opt->func(optValue);
			if (retVal == NULL)
				goto fail;

			return retVal;
		} else {
			if (consumed != NULL)
				*consumed = 0;
			if (success != NULL)
				*success = 1;
			return NULL;
		}
	}
	case RULE_REPEAT: {
		const struct ruleRepeat *rep = (const void *)top;
		long consumed2 = 0, consumed3 = 0;
		strPtr values = NULL;
		int success2 = 0;
		for (;;) {
			__auto_type retVal =
			    __parse(rep->rule, lexerItemNode, &consumed2, &success2, killData);
			if (!success2)
				break;
			values = strPtrAppendItem(values, retVal);

			lexerItemNode = moveForward(lexerItemNode, consumed2);
			consumed3 += consumed2;
		}
		void *retVal = NULL;
		if (rep->func != NULL)
			retVal = rep->func((const void **)values, strPtrSize(values));
		strPtrDestroy(&values);

		if (retVal == NULL)
			goto fail;

		if (success != NULL)
			*success = 1;
		if (consumed != NULL)
			*consumed = consumed3;

		return retVal;
	}
	case RULE_SEQUENCE: {
		const struct ruleSequence *seq = (const void *)top;
		long consumed2, consumed3 = 0;
		int success2 = 0;
		long i;
		strPtr values = NULL;
		for (i = 0; i != strRulePSize(seq->rules); i++) {
			__auto_type value = __parse(seq->rules[i], lexerItemNode, &consumed2,
			                            &success2, killData);
			if (!success2) {
				// Destroy previous values
				for (long i2 = 0; i2 != i; i2++) {
					__auto_type rule = seq->rules[i];
					if (killData != NULL)
						killData(values[i2]);
				}

				strPtrDestroy(&values);
				goto fail;
			}

			values = strPtrAppendItem(values, value);

			consumed3 += consumed2;
			lexerItemNode = moveForward(lexerItemNode, consumed2);
		}

		void *retVal = NULL;
		if (seq->func != NULL)
			retVal = seq->func((const void **)values, strPtrSize(values));

		strPtrDestroy(&values);

		if (retVal == NULL)
			goto fail;
		return retVal;
	}
	case RULE_TERMINAL: {
		const struct ruleTerminal *term = (const void *)top;
		const struct __lexerItem *value = llLexerItemValuePtr(lexerItemNode);

		__auto_type retVal = term->func(value);
		if (retVal == NULL) {
			if (success != NULL)
				*success = 1;
			if (consumed != NULL)
				*consumed = 1;
			return retVal;
		} else {
			goto fail;
		}
	}
	case RULE_OR: {
		const struct ruleOr * or = (const void *)top;

		// Rules are sorted by precedence
		long consumed2;
		int success2;
		for (long i = strRulePSize(or->rules) - 1; i >= 0; i--) {
			__auto_type data =
			    __parse(or->rules[i], lexerItemNode, &consumed2, &success2, killData);
			if (success2) {
				if (success != NULL)
					*success = 1;
				if (consumed != NULL)
					*consumed = consumed2;

				return data;
			}
		}

		goto fail;
	}
	default:
		assert(0);
	}
fail : {
	if (success != NULL)
		*success = 0;
	if (consumed != NULL)
		*consumed = 0;
	return NULL;
}
}
void *parse(struct grammar *gram, llLexerItem items, int *success,
            void (*killData)(void *)) {
	return __parse(gram->top, llLexerItemFirst(items), NULL, success, killData);
}
static struct rule *ruleOptCreate(const char *name, struct rule *rule,
                                  double prec, void *(*func)(const void *)) {
	struct ruleOpt *retVal = malloc(sizeof(struct ruleOpt));
	retVal->base.prec = prec;
	retVal->base.type = RULE_OPT;
	retVal->base.name = name;
	retVal->func = func;
	retVal->rule = rule;

	return (struct rule *)retVal;
}
static int precCmp(const void *a, const void *b) {
	const struct rule *A = a, *B = b;
	if (A->prec > B->prec) {
		return 1;
	} else if (A->prec < B->prec) {
		return -1;
	} else {
		return 0;
	}
}
static struct rule *ruleSequenceCreate(const char *name, strRuleP rules,
                                       double prec,
                                       void *(*func)(const void **, long)) {
	struct ruleSequence *retVal = malloc(sizeof(struct ruleSequence));
	retVal->base.prec = prec;
	retVal->base.type = RULE_SEQUENCE;
	retVal->base.name = name;
	retVal->func = func;
	retVal->rules = rules;

	return (struct rule *)retVal;
}
static struct rule *ruleRepeatCreate(const char *name, const struct rule *rule,
                                     double prec,
                                     void *(*func)(const void **, long)) {
	struct ruleRepeat *retVal = malloc(sizeof(struct ruleRepeat));
	retVal->base.prec = prec;
	retVal->base.type = RULE_REPEAT;
	retVal->base.name = name;

	retVal->func = func;
	retVal->rule = rule;

	return (struct rule *)retVal;
}
static struct rule *ruleOrCreate(const char *name, strRuleP rules,
                                 double prec) {
	struct ruleOr *retVal = malloc(sizeof(struct ruleOr));
	retVal->base.prec = prec;
	retVal->base.type = RULE_OR;
	retVal->base.name = name;
	retVal->rules = rules;

	return (struct rule *)retVal;
}
static struct rule *
ruleTerminalCreate(const char *name, double prec,
                   void *(*func)(const struct __lexerItem *)) {
	struct ruleTerminal *retVal = malloc(sizeof(struct ruleTerminal));
	retVal->base.prec = prec;
	retVal->base.type = RULE_TERMINAL;
	retVal->base.name = name;
	retVal->func = func;

	return (struct rule *)retVal;
}
static void ruleDestroy(struct rule *rule) {
	switch (rule->type) {
	case RULE_OPT: {
		break;
	}
	case RULE_OR: {
		struct ruleOr * or = (void *)rule;
		strRulePDestroy(& or->rules);
		break;
	}
	case RULE_REPEAT: {
		break;
	}
	case RULE_SEQUENCE: {
		struct ruleSequence *seq = (void *)rule;
		strRulePDestroy(&seq->rules);
		break;
	}
	case RULE_TERMINAL: {
		break;
	}
	case RULE_ALIAS: {
		break;
	}
	}
	free(rule);
}
STR_TYPE_DEF(const char *, Name);
STR_TYPE_FUNCS(const char *, Name);
struct grammarRule {
	enum ruleType type;
	const char *name;
	strName names;
	void *func;
	double prec;
};
struct grammarRule *grammarRuleOptCreate(const char *name, double prec,
                                         const char *rule,
                                         void *(*func)(const void *)) {
	struct grammarRule *retVal = malloc(sizeof(struct grammarRule));

	retVal->name = name;
	retVal->type = RULE_OPT;
	retVal->names = strNameAppendItem(NULL, rule);
	retVal->func = func;
	retVal->prec = prec;

	return retVal;
}
struct grammarRule *grammarRuleRepeatCreate(const char *name, double prec,
                                            void *(*func)(const void **, long),
                                            ...) {
	strName rules = NULL;
	va_list list;
	va_start(list, func);
	for (;;) {
		__auto_type r = va_arg(list, const char *);
		if (r == NULL)
			break;
		else
			rules = strNameAppendItem(rules, r);
	}
	va_end(list);

	struct grammarRule *retVal = malloc(sizeof(struct grammarRule));

	retVal->name = name;
	retVal->type = RULE_REPEAT;
	retVal->names = rules;
	retVal->func = func;
	retVal->prec = prec;

	return retVal;
}
struct grammarRule *
grammarRuleTerminalCreate(const char *name, double prec,
                          void *(*func)(const struct __lexerItem *)) {
	struct grammarRule *retVal = malloc(sizeof(struct grammarRule));

	retVal->name = name;
	retVal->type = RULE_TERMINAL;
	retVal->names = NULL;
	retVal->func = func;
	retVal->prec = prec;

	return retVal;
}
struct grammarRule *
grammarRuleSequenceCreate(const char *name, double prec,
                          void *(*func)(const void **, long), ...) {
	strName rules = NULL;
	va_list list;
	va_start(list, func);
	for (;;) {
		__auto_type r = va_arg(list, const char *);
		if (r == NULL)
			break;
		else
			rules = strNameAppendItem(rules, r);
	}
	va_end(list);

	struct grammarRule *retVal = malloc(sizeof(struct grammarRule));

	retVal->name = name;
	retVal->type = RULE_SEQUENCE;
	retVal->names = rules;
	retVal->func = func;
	retVal->prec = prec;

	return retVal;
}
STR_TYPE_DEF(struct grammarRule *, GrammarRule);
STR_TYPE_FUNCS(struct grammarRule *, GrammarRule);
MAP_TYPE_DEF(strGrammarRule, GrammarRule);
MAP_TYPE_FUNCS(strGrammarRule, GrammarRule);
MAP_TYPE_DEF(struct rule *, Rule);
MAP_TYPE_FUNCS(struct rule *, Rule);
void grammarRuleDestroy(void *ptr) {
	struct grammarRule **g = ptr;
	strNameDestroy(&g[0]->names);
	free(*g);
}
static void replaceAliases(strRuleP *rules, int sortByPrec) {
	for (long i = 0; i != strRulePSize(*rules); i++) {
		if (rules[0][i]->type == RULE_ALIAS) {
			struct ruleForward *alias = (void *)rules[0][i];
			rules[0][i] = (struct rule *)alias->alias;
		}
	}

	if (sortByPrec)
		qsort(*rules, strRulePSize(*rules), sizeof(**rules), precCmp);
}
static int scan(const char **array, const char *key, long len) {
	for (long i = 0; i != len; i++)
		if (0 == strcmp(array[i], key))
			return i;
	return -1;
}
struct grammar *grammarCreate(struct grammarRule *top,
                              struct grammarRule **rules, long count) {
	struct grammar *retVal = malloc(sizeof(struct grammar));
	mapGrammarRule rulesMap = mapGrammarRuleCreate();
	for (long i = 0; i != count; i++) {
	loop:;
		__auto_type find = mapGrammarRuleGet(rulesMap, rules[i]->name);
		if (NULL == find) {
			mapGrammarRuleInsert(rulesMap, rules[i]->name, NULL);
			goto loop;
		} else {
			*find = strGrammarRuleAppendItem(*find, rules[i]);
		}
	}

	long count2;
	mapGrammarRuleKeys(rulesMap, NULL, &count2);
	const char *keys[count2];
	mapGrammarRuleKeys(rulesMap, keys, &count2);

	__auto_type topRuleI = scan(keys, top->name, count2);
	assert(topRuleI != -1);
	struct rule *topRule = NULL;

	strRuleP rules2 = NULL;

	struct rule *aliases[count2];
	for (long i = 0; i != count2; i++) {
		aliases[i] = malloc(sizeof(struct ruleForward));
		aliases[i]->type = RULE_ALIAS;
		aliases[i]->prec = 1;
		((struct ruleForward *)aliases[i])->alias = NULL;
	}

	for (long i = 0; i != count2; i++) {
		__auto_type find = *mapGrammarRuleGet(rulesMap, keys[i]);

		strRuleP rules3 = NULL;
		for (long i2 = 0; i2 != strGrammarRuleSize(find); i2++) {
			struct rule *r = NULL;
			if (find[i2]->type == RULE_TERMINAL) {
				r = ruleTerminalCreate(find[i2]->name, find[i2]->prec, find[i2]->func);
			} else if (find[i2]->type == RULE_OPT) {
				__auto_type opt = aliases[scan(keys, find[i2]->names[0], count2)];
				r = ruleOptCreate(find[i2]->name, opt, find[i2]->prec, find[i2]->func);
			} else if (find[i2]->type == RULE_REPEAT) {
				__auto_type rep = aliases[scan(keys, find[i2]->names[0], count2)];
				r = ruleRepeatCreate(find[i2]->name, rep, find[i2]->prec,
				                     find[i2]->func);
			} else if (find[i2]->type == RULE_SEQUENCE) {
				__auto_type len = strNameSize(find[i2]->names);
				strRuleP seq = strRulePResize(NULL, len);
				for (long i = 0; i != len; i++) {
					__auto_type ruleI = scan(keys, find[i2]->names[i], count2);
					assert(ruleI != -1);
					__auto_type item = aliases[ruleI];
					seq[i] = item;
				}

				r = ruleSequenceCreate(find[i2]->name, seq, find[i2]->prec,
				                       find[i2]->func);
			} else {
				assert(0);
			}

			rules3 = strRulePAppendItem(rules3, r);
		}

		__auto_type alias = (struct ruleForward *)aliases[i];
		// If a single rule,use that rule,else "OR" the rules togheter
		if (strRulePSize(rules3) == 1) {
			alias->alias = rules3[0];

			if (topRuleI == i)
				topRule = rules3[0];

			rules2 = strRulePAppendItem(rules2, rules3[0]);

			strRulePDestroy(&rules3);
		} else {
			__auto_type newRule = ruleOrCreate(keys[i], rules3, 1);
			alias->alias = newRule;

			if (topRuleI == i)
				topRule = newRule;

			rules2 = strRulePAppendItem(rules2, newRule);
			rules2 = strRulePConcat(rules2, rules3);
		}
	}

	for (long i = 0; i != strRulePSize(rules2); i++) {
		if (rules2[i]->type == RULE_OR) {
			struct ruleOr * or = (void *)rules2[i];
			replaceAliases(& or->rules, 1);
		} else if (rules2[i]->type == RULE_SEQUENCE) {
			struct ruleSequence *seq = (void *)rules2[i];
			replaceAliases(&seq->rules, 0);
		}
	}

	retVal->rules = rules2;
	retVal->top = topRule;

	for (long i = 0; i != count2; i++)
		free(aliases[i]);

	mapGrammarRuleDestroy(rulesMap, NULL);
	return retVal;
}
void grammarDestroy(struct grammar **gram) {
	for (long i = 0; i != strRulePSize(gram[0]->rules); i++) {
		ruleDestroy(gram[0]->rules[i]);
	}
	strRulePDestroy(&gram[0]->rules);
}
