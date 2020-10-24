#include <assert.h>
#include <stdlib.h>
#include <str.h>
STR_TYPE_DEF(char, Char);
STR_TYPE_FUNCS(char, Char);
static int chrFind(const void *a, const void *b) {
	return *(const char *)a - *(const char *)b;
}
void strTests() {
	// NULLS
	strChar str1 = NULL;
	strCharDestroy(&str1);
	str1 = NULL;
	str1 = strCharReserve(str1, 0);
	assert(NULL == strCharConcat(NULL, NULL));
	assert(NULL == strCharResize(str1, 0));
	// Actual data
	str1 = strCharAppendData(str1, "abc", 3);
	assert(0 == strncmp(str1, "abc", 3));
	str1 = strCharAppendItem(str1, 'e');
	assert(0 == strncmp(str1, "abce", 4));
	str1 = strCharSortedInsert(str1, 'd', chrFind);
	assert(0 == strncmp(str1, "abcde", 5));
	// Find
	assert('d' == *strCharSortedFind(str1, 'd', chrFind));
	// Difference
	strChar str2 = strCharAppendData(NULL, "ace", 3);
	str1 = strCharSetDifference(str1, str2, chrFind);
	assert(0 == strncmp(str1, "bd", 2));
	assert(2 == strCharSize(str1));
	// Unique
	strChar str3 = strCharAppendData(NULL, "aabbbcccc", 2 + 3 + 4 + 1);
	str3 = strCharUnique(str3, chrFind, NULL);
	assert(0 == strcmp(str3, "abc"));
	// Intersection
	str2 = strCharAppendData(NULL, "456789", 6);
	str3 = strCharAppendData(NULL, "123456", 6);
	str2 = strCharSetIntersection(str2, str3, chrFind, NULL);
	assert(strCharSize(str2) == 3);
	assert(0 == strncmp(str2, "456", 3));
}
