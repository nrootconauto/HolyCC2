#include <assert.h>
#include <parserA.h>
#include <unistd.h>
#include <diagMsg.h>
#include <stdio.h>
#include <stdlib.h>
#include <preprocessor.h>
static FILE *file=NULL;
static void createFile(const char *text) {
		if(file!=NULL)
				fclose(file);

		char buffer[1024];
		strcpy(buffer, tmpnam(NULL));
		
		file=fopen(buffer, "w");
		fwrite(text, 1, strlen(text), file);
		fclose(file);

		file=fopen(buffer, "r");
		strTextModify mods;
		strFileMappings fMappings;
		int err;
		FILE *res=createPreprocessedFile(buffer, &mods, &fMappings, &err);
		assert(!err);
		diagInstCreate(DIAG_ANSI_TERM,  fMappings, mods, buffer, stdout);
		file=res;
}
void parserDiagTests() {
		//
		//Unterminated expression(no ";")
		//
		const char *text="1+2";
		createFile(text);
		int err;
		__auto_type items= lexText(__vecAppendItem(NULL, text, strlen(text)),&err);
		parseStatement(items, NULL);
}
