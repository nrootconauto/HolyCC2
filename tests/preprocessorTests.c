#include <assert.h>
#include <preprocessor.h>
#include <stdarg.h>
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
/**
 * Even items are the segments that are shared by source and preprocessed,odd
 * are preprocessor segments
 */
void checkMappings(const strSourceMapping mappings, const char *sourceStr,
                   const char *processedStr, ...) {
	const char *processedPtr = processedStr;

	va_list args;
	va_start(args, processedStr);
	for (int i = 0;; i++) {
		// See above for meaning of even
		int odd = i % 2;
		const char *expected = va_arg(args, const char *);
		if (expected == NULL)
			break;

		__auto_type len = strlen(expected);
		if (!odd) {
			long sourcePos = mappedPosition(mappings, processedPtr - processedStr+len)-len;

			assert(0 == strncmp(processedPtr, expected, len));
			assert(0 == strncmp(sourceStr + sourcePos, expected, len));
		} else {
			assert(0 == strncmp(processedPtr, expected, len));
		}
		processedPtr += len;
	}
	va_end(args);
}
void preprocessorTests() {
	//
	// Test 1,define
	//
	const char *text = "#define x b\n"
	                   "a x c\n";
	strSourceMapping mappings;
	int err;
	struct __vec *textSlice = __vecResize(NULL, strlen(text) + 1);
	strcpy((char *)textSlice, text);
	__auto_type resultFile = createPreprocessedFile(textSlice, &mappings, &err);
	assert(err == 0);
	__auto_type resultStr = file2Str(resultFile);
	assert(0 ==
	       strcmp("\na b c\n",
	              (char *)resultStr)); // First newline for ignoring #define line
	checkMappings(mappings, (const char *)textSlice, (const char *)resultStr,
	              "\na ", "b", " c\n", NULL);
	fclose(resultFile);
	__vecDestroy(textSlice);
	__vecDestroy(resultStr);
	strSourceMappingDestroy(&mappings);
	//
	// Test 2,error on infinite recursion
	//
	text = "#define x y\n#define y x \n x";
	textSlice = __vecResize(NULL, strlen(text) + 1);
	strcpy((char *)textSlice, text);
	resultFile = createPreprocessedFile(textSlice, &mappings, &err);
	fclose(resultFile);
	assert(err == 1);
	__vecDestroy(textSlice);
	//
	// Test 3,Dont replace  in macro
	//
	text = "#define x #define \nx x 2\nx";
	textSlice = __vecResize(NULL, strlen(text) + 1);
	strcpy((char *)textSlice, text);
	resultFile = createPreprocessedFile(textSlice, &mappings, &err);
	assert(err == 0);
	resultStr = file2Str(resultFile);
	assert(0 == strcmp("\n\n2", (char *)resultStr));
	checkMappings(mappings, (const char *)textSlice, (const char *)resultStr, "",
	              "\n", "", "\n2", "", NULL);
	fclose(resultFile);
	__vecDestroy(textSlice);
	__vecDestroy(resultStr);
	strSourceMappingDestroy(&mappings);
	//
	// Test 4,Replace macro name
	//
	text = "#define x define\n#x y 2\ny";
	textSlice = __vecResize(NULL, strlen(text) + 1);
	strcpy((char *)textSlice, text);
	resultFile = createPreprocessedFile(textSlice, &mappings, &err);
	assert(err == 0);
	resultStr = file2Str(resultFile);
	assert(0 == strcmp("\n\n2", (char *)resultStr));
	fclose(resultFile);
	__vecDestroy(textSlice);
	__vecDestroy(resultStr);
	strSourceMappingDestroy(&mappings);
	//
	// Test 5,include
	//
	__auto_type dummy = tmpnam(NULL);
	__auto_type includeFile = fopen(dummy, "w");
	const char *includeText = "a\nb\nc";
	fwrite(includeText, 1, strlen(includeText) + 1, includeFile);
	fclose(includeFile);
	char buffer[1024];
	sprintf(buffer, "#include \"%s\"\n", dummy);
	textSlice = __vecResize(NULL, strlen(buffer) + 1);
	strcpy((char *)textSlice, buffer);
	resultFile = createPreprocessedFile(textSlice, &mappings, &err);
	assert(err == 0);
	resultStr = file2Str(resultFile);
	assert(0 == strcmp("a\nb\nc\n", (char *)resultStr));
	checkMappings(mappings, (const char *)textSlice, (const char *)resultStr, "",
	              "a\nb\nc", NULL);
	fclose(resultFile);
	__vecDestroy(textSlice);
	__vecDestroy(resultStr);
	strSourceMappingDestroy(&mappings);
}
