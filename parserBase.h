#pragma once
#include <linkedList.h>
#include <str.h>
struct rule;
STR_TYPE_DEF(struct rule *, RuleP);
STR_TYPE_FUNCS(struct rule *, RuleP);
struct rule *ruleOptCreate(struct rule *rule, double prec,
                           struct __vec *(*func)(const void *),
                           void (*killData)(void *));
struct rule *ruleOrCreate(const strRuleP rules, double prec,
                          struct __vec *(*func)(const void *),
                          void (*killData)(void *));
struct rule *ruleRepeatCreate(const struct rule *rule, double prec,
                              struct __vec *(*func)(const void **, long),
                              void (*killData)(void *));
struct rule *ruleSequenceCreate(const strRuleP rules, double prec,
                                struct __vec *(*func)(const void **, long),
                                void (*killData)(void *));
struct rule *
ruleTerminalCreate(double prec,
                   struct __vec *(*func)(const struct __lexerItem *),
                   void (*killData)(void *));
void ruleDestroy(struct rule *rule);
struct __vec *parse(const struct rule *top, llLexerItem lexerItemNode,
                             long *consumed, int *success);
