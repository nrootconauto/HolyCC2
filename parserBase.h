#pragma once
#include <cacheingLexer.h>
#include <linkedList.h>
#include <str.h>
struct grammar;
struct grammarRule;
struct grammar *grammarCreate(struct grammarRule *top,
                              struct grammarRule **rules, long count);
void *parse(struct grammar *gram, llLexerItem items, int *success,
            void (*killData)(void *));
struct grammarRule *grammarRuleOptCreate(const char *name, double prec,
                                         const char *rule,
                                         void *(*func)(const void *));
struct grammarRule *grammarRuleRepeatCreate(const char *name, double prec,
                                            void *(*func)(const void **, long),
                                            ...);
struct grammarRule *
grammarRuleSequenceCreate(const char *name, double prec,
                          void *(*func)(const void **, long), ...);
struct grammarRule *
grammarRuleTerminalCreate(const char *name, double prec,
                          void *(*func)(const struct __lexerItem *));
void grammarRuleDestroy(void *ptr);
void grammarDestroy(struct grammar** gram) ;
