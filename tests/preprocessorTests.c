#include <assert.h>
#include <preprocessor.h>
#include <str.h>
struct __vec *file2Str(FILE *file) {
	fseek(file, SEEK_END, 0);
	long end = ftell(file);
	fseek(file, SEEK_SET, 0);
	long start = ftell(file);

	char buffer[end - start + 1];
	fread(buffer, 1, end - start, file);
	__auto_type retVal = __vecResize(NULL, end - start + 1);
	memcpy(retVal, buffer, end - start);
	(end - start)[(char *)retVal] = '\0';

	return retVal;
}
void preprocessorTests() {
	const char *text = "#define x b\n"
	                   "a x c\n";
	int err;

	struct __vec *textSlice = __vecResize(NULL, strlen(text) + 1);
	strcpy((char *)textSlice, text);

	__auto_type resultFile = createPreprocessedFile(textSlice, &err);
	assert(err == 0);
	__auto_type resultStr = file2Str(resultFile);
	assert(0 == strcmp("a b c\n", (char *)resultStr));
	
	__vecDestroy(textSlice);
	__vecDestroy(resultStr);
}
