#pragma once
#include <str.h>
enum diffType {
	DIFF_SAME,
	DIFF_REMOVE,
	DIFF_INSERT,
	DIFF_UNDEF,
};
STR_TYPE_DEF(char, Diff);   // char here represents enum diffType
STR_TYPE_FUNCS(char, Diff); // char here represents enum diffType
strDiff __diff(const void *a, const void *b, long aSize, long bSize,
               long itemSize);
