#include <assert.h>
#include <stdlib.h>
#include <str.h>
STR_TYPE_DEF(char, );
STR_TYPE_FUNCS(char, );
static int chrPred(void *a, void *b) { return *(char *)a - *(char *)b; }
void strTests() {
	// NULLS
	str str1 = NULL;
	strDestroy(&str1);
	str1 = NULL;
	str1 = strReserve(str1, 0);
	assert(NULL == strConcat(NULL, NULL));
	assert(NULL == strResize(str1, 0));
	// Actual data
	str1 = strAppendData(str1, "abc", 3);
	assert(0 == strncmp(str1, "abc", 3));
	str1 = strAppendItem(str1, 'e');
	assert(0 == strncmp(str1, "abce", 4));
	str1 = strSortedInsert(str1, 'd', chrPred);
	assert(0 == strncmp(str1, "abcde", 5));
	// Find
	assert('d' == *strSortedFind(str1, 'd', chrPred));
	// Difference
	str str2 = strAppendData(NULL, "ace", 3);
	str1 = strSetDifference(str1, str2, chrPred);
	assert(0 == strncmp(str1, "bd", 2));
	assert(2 == strSize(str1));
}
