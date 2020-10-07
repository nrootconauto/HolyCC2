#include <assert.h>
#include <preprocessor.h>
#include <str.h>
struct __vec *file2Str(FILE *file) {
	fseek(file, 0, SEEK_END);
	long end = ftell(file);
	fseek(file, 0, SEEK_SET);
	long start = ftell(file);

	char buffer[end - start + 1];
	fread(buffer, 1, end - start, file);
	__auto_type retVal = __vecResize(NULL, end - start + 1);
	memcpy(retVal, buffer, end - start);
	(end - start)[(char *)retVal] = '\0';

	return retVal;
}
void preprocessorTests() {
	//
	// Test 1,define
	//
	const char *text = "#define x b\n"
	                   "a x c\n";
	int err;
	struct __vec *textSlice = __vecResize(NULL, strlen(text) + 1);
	strcpy((char *)textSlice, text);
	__auto_type resultFile = createPreprocessedFile(textSlice, &err);
	assert(err == 0);
	__auto_type resultStr = file2Str(resultFile);
	assert(0 ==
	       strcmp("\na b c\n",
	              (char *)resultStr)); // First newline for ignoring #define line
	fclose(resultFile);
	__vecDestroy(textSlice);
	__vecDestroy(resultStr);

	//
	// Test 2,error on infinite recursion
	//
	text = "#define x y\n#define y x \n x";
	textSlice = __vecResize(NULL, strlen(text) + 1);
	strcpy((char *)textSlice, text);
	resultFile = createPreprocessedFile(textSlice, &err);
	fclose(resultFile);
	assert(err == 1);
	__vecDestroy(textSlice);
	//
	// Test 3,Dont replace  in macro
	//
	text = "#define x #define \nx x 2\nx";
	textSlice = __vecResize(NULL, strlen(text) + 1);
	strcpy((char *)textSlice, text);
	resultFile = createPreprocessedFile(textSlice, &err);
	assert(err == 0);
	resultStr = file2Str(resultFile);
	assert(0 ==
	       strcmp("\n\n2",
	              (char *)resultStr));
	fclose(resultFile);
	__vecDestroy(textSlice);
	__vecDestroy(resultStr);
}
