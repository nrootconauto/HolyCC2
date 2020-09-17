#include <assert.h>
#include <diff.h>
// See https://blog.robertelder.org/diff-algorithm/
void diffTests() {
	const char *A = "abgdef";
	const char *B = "gh";
	__auto_type res = __diff(A, B, strlen(A), strlen(B), sizeof(char));
	assert(strDiffSize(res) == 7);
	char expected[] = {DIFF_REMOVE, DIFF_REMOVE, DIFF_SAME,  DIFF_REMOVE,
	                   DIFF_REMOVE, DIFF_INSERT, DIFF_REMOVE};
	for (int i = 0; i != 7; i++) {
		assert(res[i] == expected[i]);
	}
}
