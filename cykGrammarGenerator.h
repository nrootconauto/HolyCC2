#pragma once
#include <cyk.h>
#include <hashTable.h>
#include <str.h>
STR_TYPE_DEF(struct grammarRule *, RuleP);
STR_TYPE_FUNCS(struct grammarRule *, RuleP);
MAP_TYPE_DEF(void *,TemplateData);
MAP_TYPE_FUNCS(void *,TemplateData);
MAP_TYPE_DEF(strCYKRulesP, CYKRules);
MAP_TYPE_FUNCS(strCYKRulesP, CYKRules);
struct grammarRule *grammarRuleOptCreate(const char *name,const char * templateName,  double prec,
                                         const struct grammarRule *rule);
struct grammarRule *grammarRuleRepeatCreate(const char *name,const char * templateName,  double prec,
                                            const struct grammarRule *rule);
struct grammarRule *grammarRuleSequenceCreate(const char *name,const char * templateName,  double prec,
                                              ...);
struct grammarRule *
grammarRuleTerminalCreate(const char *name, const char * templateName, double prec,
                          int (*validate)(const void *, const void *),
                          void *data, void (*destroyData)(void *));
void grammarRuleDestroy(struct grammarRule **rule);
struct grammar;
struct grammar *grammarCreate(const strRuleP grammarRules);
struct __cykBinaryMatrix *grammarParse(struct grammar *grammar,
                                       struct __vec *items, long itemSize);
void grammarDestroy(struct grammar **grammar);
