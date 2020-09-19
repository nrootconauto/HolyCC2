#pragma once
#include <str.h>
enum diffType {
	DIFF_SAME,
	DIFF_REMOVE,
	DIFF_INSERT,
	DIFF_UNDEF,
};
struct __diff {
	enum diffType type;
	int len;
};
STR_TYPE_DEF(struct __diff, Diff);   // char here represents enum diffType
STR_TYPE_FUNCS(struct __diff, Diff); // char here represents enum diffType
strDiff __diff(const void *a, const void *b, long aSize, long bSize,
               long itemSize, int (*pred)(const void *a, const void *b));
