#include <assert.h>
#include <stdlib.h>
#include <str.h>
STR_TYPE(char, );
static int chrPred(void *a, void *b) { return *(char *)a - *(char *)b; }
void strTests() {
	// NULLS
	str str = NULL;
	strDestroy(str);
	str = NULL;
	str = strReserve(str, 0);
	assert(NULL == strConcat(NULL, NULL));
	assert(NULL == strResize(str, 0));
	// Actual data
	str = strAppendData(str, "abc", 3);
	assert(0 == strncmp(str, "abc", 3));
	str = strAppendItem(str, 'e');
	assert(0 == strncmp(str, "abce", 4));
	str = strSortedInsert(str, 'd', chrPred);
	assert(0 == strncmp(str, "abcde", 5));
}
