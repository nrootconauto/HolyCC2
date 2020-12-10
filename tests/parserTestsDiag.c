#include <assert.h>
#include <diagMsg.h>
#include <parserA.h>
#include <preprocessor.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
static FILE *file = NULL;
static void createFile(const char *text) {
	if (file != NULL)
		fclose(file);

	char buffer[1024];
	strcpy(buffer, tmpnam(NULL));

	file = fopen(buffer, "w");
	fwrite(text, 1, strlen(text)+1, file);
	fclose(file);

	file = fopen(buffer, "r");
	strTextModify mods;
	strFileMappings fMappings;
	int err;
	FILE *res = createPreprocessedFile(buffer, &mods, &fMappings, &err);
	assert(!err);
	diagInstCreate(DIAG_ANSI_TERM, fMappings, mods, buffer, stdout);
	file = res;
}
void parserDiagTests() {
	//
	// Unterminated expression(no ";")
	//
	const char *text = "1+2";
	createFile(text);
	int err;
	__auto_type items = lexText(__vecAppendItem(NULL, text, strlen(text)+1), &err);
	assert(!err);
	parseStatement(items, NULL);
	//
	//"if" missing ")"
	//
	text = "if(1+2 {\n"
	       "    foo();\n"
	       "}";
	createFile(text);
	items = lexText(__vecAppendItem(NULL, text, strlen(text)+1), &err);
	assert(!err);
	parseStatement(items, NULL);
}
