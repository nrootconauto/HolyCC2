#include <assert.h>
#include <diff.h>
#include <stdbool.h>
static bool checkDiff(const char *A, const char *B, const strDiff diffs) {
	int aI = 0, bI = 0;
	for (int i = 0; i != strDiffSize(diffs); i++) {
		if (diffs[i].type == DIFF_INSERT)
			bI += diffs[i].len;
		else if (diffs[i].type == DIFF_REMOVE)
			aI += diffs[i].len;
		else if (diffs[i].type == DIFF_SAME)
			bI += diffs[i].len, aI += diffs[i].len;
	}
	return strlen(A) == aI && strlen(B) == bI;
}
// See https://blog.robertelder.org/diff-algorithm/
void diffTests() {
	const char *A = "abgdef";
	const char *B = "gh";
	__auto_type res = __diff(A, B, strlen(A), strlen(B), sizeof(char));
	assert(checkDiff(A, B, res));
	strDiffDestroy(&res);
	//
	A = "abcabba";
	B = "cbabac";
	res = __diff(A, B, strlen(A), strlen(B), sizeof(char));
	assert(checkDiff(A, B, res));
	strDiffDestroy(&res);
}
