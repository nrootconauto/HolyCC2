#include <assert.h>
#include <preprocessor.h>
#include <stdarg.h>
#include <stdlib.h>
#include <str.h>
#include <unistd.h>
static struct __vec *file2Str(FILE *file) {
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
static char *uniqueFileName() {
	__auto_type tmp = tmpnam(NULL);
	char *retVal = malloc(strlen(tmp) + 1);
	strcpy(retVal, tmp);
	return retVal;
}
/**
 * Even items are the segments that are shared by source and preprocessed,odd
 * are preprocessor segments
 */
static void checkMappings(const strTextModify mappings,
                          const strFileMappings fileMappings,
                          const char *sourceStr, const char *processedStr,
                          ...) {
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
			long sourcePos = mapToSource(processedPtr - processedStr + len, mappings,
			                             fileMappings[0].mappingIndexStart) -
			                 len;

			assert(0 == strncmp(processedPtr, expected, len));
			assert(0 == strncmp(sourceStr + sourcePos, expected, len));
		} else {
			assert(0 == strncmp(processedPtr, expected, len));
		}
		processedPtr += len;
	}
	va_end(args);
}
static FILE *str2File(const char *name, const char *text) {
	__auto_type retVal = fopen(name, "w");
	fwrite(text, 1, strlen(text), retVal);
	fclose(retVal);

	return fopen(name, "r");
}
void preprocessorTests() {
	//
	// Test 1,define
	//
	__auto_type sourceFileName = uniqueFileName();
	const char *text = "#define x b\n"
	                   "a x c\n";
	__auto_type sourceFile = str2File(sourceFileName, text);
	strTextModify mappings;
	strFileMappings mappingsPerFile;
	int err;
	__auto_type resultFile =
	    createPreprocessedFile(sourceFileName, &mappings, &mappingsPerFile, &err);
	assert(err == 0);
	__auto_type resultStr = file2Str(resultFile);
	assert(0 ==
	       strcmp("\na b c\n",
	              (char *)resultStr)); // First newline for ignoring #define line
	checkMappings(mappings, mappingsPerFile, text, (const char *)resultStr,
	              "\na ", "b", " c\n", NULL);
	fclose(resultFile);
	fclose(sourceFile);
	__vecDestroy(resultStr);
	strTextModifyDestroy(&mappings);
	fileMappingsDestroy(&mappingsPerFile);
	//
	// Test 2,error on infinite recursion
	//
	text = "#define x y\n#define y x \n x";
	sourceFile = str2File(sourceFileName, text);
	resultFile =
	    createPreprocessedFile(sourceFileName, &mappings, &mappingsPerFile, &err);
	assert(err == 1);
	fclose(sourceFile);
	//
	// Test 3,Dont replace  in macro
	//
	text = "#define x #define \nx x 2\nx";
	sourceFile = str2File(sourceFileName, text);
	resultFile =
	    createPreprocessedFile(sourceFileName, &mappings, &mappingsPerFile, &err);
	assert(err == 0);
	resultStr = file2Str(resultFile);
	assert(0 == strcmp("\n\n2", (char *)resultStr));
	checkMappings(mappings, mappingsPerFile, text, (const char *)resultStr, "",
	              "\n", "", "\n2", "", NULL);
	fclose(sourceFile);
	__vecDestroy(resultStr);
	strTextModifyDestroy(&mappings);
	fclose(resultFile);
	fileMappingsDestroy(&mappingsPerFile);
	//
	// Test 4,Replace macro name
	//
	text = "#define x define\n#x y 2\ny";
	sourceFile = str2File(sourceFileName, text);
	resultFile =
	    createPreprocessedFile(sourceFileName, &mappings, &mappingsPerFile, &err);
	assert(err == 0);
	resultStr = file2Str(resultFile);
	assert(0 == strcmp("\n\n2", (char *)resultStr));
	fclose(resultFile);
	fclose(sourceFile);
	__vecDestroy(resultStr);
	strTextModifyDestroy(&mappings);
	fileMappingsDestroy(&mappingsPerFile);
	//
	// Test 5,include
	//
	__auto_type dummyFileName1 = uniqueFileName();
	__auto_type includeFile = fopen(dummyFileName1, "w");
	const char *includeText = "a\nb\nc";
	fwrite(includeText, 1, strlen(includeText), includeFile);
	fclose(includeFile);
	char buffer[1024];
	sprintf(buffer, "#include \"%s\"\n123", dummyFileName1);
	sourceFile = str2File(sourceFileName, buffer);
	resultFile =
	    createPreprocessedFile(sourceFileName, &mappings, &mappingsPerFile, &err);
	assert(err == 0);
	resultStr = file2Str(resultFile);
	assert(0 == strcmp("a\nb\nc\n123", (char *)resultStr));
	checkMappings(mappings, mappingsPerFile, (const char *)buffer,
	              (const char *)resultStr, "", "a\nb\nc", "\n123", NULL);
	fclose(resultFile);
	fclose(sourceFile);
	__vecDestroy(resultStr);
	strTextModifyDestroy(&mappings);
	fileMappingsDestroy(&mappingsPerFile);
	//
	// 6 #include #include
	//
	__auto_type dummyFileName2 = uniqueFileName();
	__auto_type includeFile2 = fopen(dummyFileName2, "w");
	sprintf(buffer, "\"%s\"\na", dummyFileName1);
	fwrite(buffer, 1, strlen(buffer), includeFile2);
	fclose(includeFile2);
	sprintf(buffer, "#include #include \"%s\"", dummyFileName2);
	sourceFile = str2File(sourceFileName, buffer);
	resultFile =
	    createPreprocessedFile(sourceFileName, &mappings, &mappingsPerFile, &err);
	assert(err == 0);
	resultStr = file2Str(resultFile);
	assert(0 == strcmp("a\nb\nc\na", (char *)resultStr));
	fclose(sourceFile);
	__vecDestroy(resultStr);
	strTextModifyDestroy(&mappings);
	fileMappingsDestroy(&mappingsPerFile);
	//
	remove(dummyFileName1);
	remove(dummyFileName2);
	free(dummyFileName1);
	free(dummyFileName2);
	free(sourceFileName);
}
