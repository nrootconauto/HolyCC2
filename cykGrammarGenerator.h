#pragma once
#include <hashTable.h>
#include <str.h>
#include <cyk.h>
STR_TYPE_DEF(struct grammarRule *, RuleP);
STR_TYPE_FUNCS(struct grammarRule *, RuleP);
MAP_TYPE_DEF(strCYKRulesP,CYKRules);
MAP_TYPE_FUNCS(strCYKRulesP,CYKRules);
