#include <assert.h>
#include <cacheingLexer.h>
#include <graph.h>
#include <linkedList.h>
#include <parserBase.h>
#include <str.h>
enum ruleType {
	RULE_REPEAT,
	RULE_OPT,
	RULE_SEQUENCE,
	RULE_TERMINAL,
	RULE_OR,
};
struct rule {
	double prec;
	enum ruleType type;
	void (*killData)(void *);
};
struct ruleOr {
	struct rule base;
	strRuleP rules;
	struct __vec *(*func)(const void *);
};
struct ruleTerminal {
	struct rule base;
	struct __vec *(*func)(const struct __lexerItem *);
};
struct ruleRepeat {
	struct rule base;
	const struct rule *rule;
	struct __vec *(*func)(const void **, long);
};
struct ruleOpt {
	struct rule base;
	const struct rule *rule;
	struct __vec *(*func)(const void *);
};
struct ruleSequence {
	struct rule base;
	strRuleP rules;
	struct __vec *(*func)(const void **, long);
};
struct computedRuleGroup;
struct computedRule {
	struct computedRuleGroup *group;
	struct rule *rule;
	// Data appended here
};
STR_TYPE_DEF(struct computedRule *, ComputedRuleP);
STR_TYPE_FUNCS(struct computedRule *, ComputedRuleP);
struct computedRuleGroup {
	strComputedRuleP items;
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
static struct __vec *__parse(const struct rule *top, llLexerItem lexerItemNode,
                             long *consumed, int *success) {
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
		void *optValue = __parse(opt->rule, lexerItemNode, &consumed2, &success2);
		if (success2) {
			if (consumed != NULL)
				*consumed = consumed2;
			if (success != NULL)
				*success = 1;

			void *retVal = NULL;
			if (opt->func != NULL)
				retVal = opt->func(optValue);

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
			    __parse(rep->rule, lexerItemNode, &consumed2, &success2);
			if (!success2)
				break;
			values = strPtrAppendItem(values, retVal);

			lexerItemNode = moveForward(lexerItemNode, consumed2);
			consumed3 += consumed2;
		}
		struct __vec *retVal = NULL;
		if (rep->func != NULL)
			retVal = rep->func((const void **)values, strPtrSize(values));

		if (success != NULL)
			*success = 1;
		if (consumed != NULL)
			*consumed = consumed3;

		strPtrDestroy(&values);
		return retVal;
	}
	case RULE_SEQUENCE: {
		const struct ruleSequence *seq = (const void *)top;
		long consumed2, consumed3 = 0;
		int success2 = 0;
		long i;
		strPtr values = NULL;
		for (i = 0; i != strRulePSize(seq->rules); i++) {
			__auto_type value =
			    __parse(seq->rules[i], lexerItemNode, &consumed2, &success2);
			if (!success2) {
				// Destroy previous values
				for (long i2 = 0; i2 != i; i2++) {
					__auto_type rule = seq->rules[i];
					if (rule->killData != NULL)
						rule->killData(values[i2]);
				}

				strPtrDestroy(&values);
				goto fail;
			}

			values = strPtrAppendItem(values, value);

			consumed3 += consumed2;
			lexerItemNode = moveForward(lexerItemNode, consumed2);
		}

		struct __vec *retVal = NULL;
		if (seq->func != NULL)
			retVal = seq->func((const void **)values, strPtrSize(values));

		strPtrDestroy(&values);
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
			    __parse(or->rules[i], lexerItemNode, &consumed2, &success2);
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
struct rule *ruleOptCreate(struct rule *rule, double prec,
                           struct __vec *(*func)(const void *),
                           void (*killData)(void *)) {
	struct ruleOpt *retVal = malloc(sizeof(struct ruleOpt));
	retVal->base.prec = prec;
	retVal->base.type = RULE_OPT;
	retVal->base.killData = killData;
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
struct rule *ruleSequenceCreate(const strRuleP rules, double prec,
                                struct __vec *(*func)(const void **, long),
                                void (*killData)(void *)) {
	struct ruleSequence *retVal = malloc(sizeof(struct ruleSequence));
	retVal->base.prec = prec;
	retVal->base.type = RULE_SEQUENCE;
	retVal->base.killData = killData;
	retVal->func = func;
	retVal->rules = strRulePAppendData(NULL, (const struct rule **)rules,
	                                   strRulePSize(rules));

	return (struct rule *)retVal;
}
struct rule *ruleRepeatCreate(const struct rule *rule, double prec,
                              struct __vec *(*func)(const void **, long),
                              void (*killData)(void *)) {
	struct ruleRepeat *retVal = malloc(sizeof(struct ruleRepeat));
	retVal->base.prec = prec;
	retVal->base.type = RULE_REPEAT;
	retVal->base.killData = killData;
	retVal->func = func;
	retVal->rule = rule;

	return (struct rule *)retVal;
}
struct rule *ruleOrCreate(const strRuleP rules, double prec,
                          struct __vec *(*func)(const void *),
                          void (*killData)(void *)) {
	struct ruleOr *retVal = malloc(sizeof(struct ruleOr));
	retVal->base.prec = prec;
	retVal->base.type = RULE_OR;
	retVal->base.killData = killData;
	retVal->func = func;
	retVal->rules = strRulePAppendData(NULL, (const struct rule **)rules,
	                                   strRulePSize(rules));

	// Sort by precedence
	qsort(retVal->rules, strRulePSize(rules), sizeof(*rules), precCmp);
	return (struct rule *)retVal;
}
struct rule *
ruleTerminalCreate(double prec,
                   struct __vec *(*func)(const struct __lexerItem *),
                   void (*killData)(void *)) {
	struct ruleTerminal *retVal = malloc(sizeof(struct ruleTerminal));
	retVal->base.prec = prec;
	retVal->base.type = RULE_OR;
	retVal->base.killData = killData;
	retVal->func = func;

	return (struct rule *)retVal;
}
void ruleDestroy(struct rule *rule) {
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
	}
	free(rule);
}
