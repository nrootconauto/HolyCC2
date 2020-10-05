#include <assert.h>
#include <cacheingLexer.h>
#include <ctype.h>
#include <preprocessor.h>
#include <stdio.h>
#include <str.h>
#include <string.h>
#include <stringParser.h>
#include <unistd.h>
// TODO implement exe,if,ifdef,ifndef
void *findEndOfLine(struct __vec *text, long pos) {
	void *retVal = (void *)text + pos;
	for (; retVal < (void *)text + __vecSize(text); retVal++) {
		__auto_type chr = *(char *)retVal;
		if (chr == '\r' || chr == '\n')
			break;
	}
	return retVal;
}
void *findNextLine(struct __vec *text, long pos) {
	void *retVal = findEndOfLine(text, pos);
	for (; retVal < (void *)text + __vecSize(text); retVal++) {
		__auto_type chr = *(char *)retVal;
		if (chr != '\r' || chr != '\n')
			break;
	}
	return retVal;
}
static const void *skipNonMacro(struct __vec *text, long pos) {
	__auto_type end = (void *)text + __vecSize(text);

loop:;
	void *find = strchr(&pos[(void *)text], '#');
	if (find == NULL)
		return end;

	// Find things that invalidate macros
	void *findCommentS = strstr(&pos[(void *)text], "//");
	void *findCommentM = strstr(&pos[(void *)text], "/*");
	findCommentS = (findCommentS == NULL) ? end : findCommentS;
	findCommentM = (findCommentM == NULL) ? end : findCommentM;
	__auto_type lesserComment =
	    (findCommentS < findCommentM) ? findCommentS : findCommentM;

	void *findStr1 = strchr(&pos[(void *)text], '\'');
	void *findStr2 = strchr(&pos[(void *)text], '"');
	findStr1 = (findStr1 == NULL) ? end : findStr1;
	findStr2 = (findStr2 == NULL) ? end : findStr2;
	__auto_type lesserStr = (findStr1 < findStr2) ? findStr1 : findStr2;
	// Found macro before str/comment
	if (find < lesserStr && find < lesserComment) {
		return find;
	}

	if (lesserComment != end && lesserComment < lesserStr) {
		// Skip comments
		if (lesserComment == findCommentS) {
			// Single-line
			pos = findCommentS - (void *)text;
			pos = findNextLine(text, pos) - (void *)text;
		} else if (lesserComment == findCommentM) {
			// Multiline
			void *otherSide = strstr((void *)text, "*/");
			if (otherSide == NULL)
				return end;
			else
				pos = otherSide - (void *)text;
		}
	} else if (lesserStr != end) {
		// Skip strings
		pos = findStr1 - (void *)text;
		long endOfString;
		int err;
		stringParse(text, pos, &endOfString, NULL, &err);
		if (err)
			return end;
		goto loop;
	}

	return (find == NULL) ? end : find;
}
static void *skipWhitespace(struct __vec *text, long pos) {
	__auto_type len = __vecSize(text);
	void *where;
	for (where = &pos[(void *)text]; where < (void *)text + len; where++) {
		__auto_type chr = *(char *)where;
		if (isblank(chr))
			continue;
		if (chr == '\n')
			continue;
		if (chr == '\r')
			continue;
		break;
	}
	return where;
}
static int expectMacroAndSkip(const char *macroText, struct __vec *text,
                              long pos, long *end) {
	const char *text2 = (const char *)text;
	if (*text2 != '#')
		return 0;

	pos = skipWhitespace(text, pos + 1) - (const void *)text;

	__auto_type len = strlen(macroText);
	if (__vecSize(text) - pos < len)
		return 0;

	if (pos + len < __vecSize(text))
		if (isalnum((const char *)text + pos + len))
			return 0;

	return 0 == strncmp((void *)text + pos, macroText, len);
}
static struct __vec *includeMacroLex(struct __vec *text, long pos, long *end,
                                     const void *data, int *err) {
	if (err != NULL)
		*err = 0;

	if (!expectMacroAndSkip("include", text, pos, &pos))
		return NULL;

	pos = skipWhitespace(text, pos) - (void *)text;

	struct parsedString result;
	if (!stringParse(text, pos, end, &result, err))
		goto malformed;

	struct includeMacro retVal;
	retVal.fileName =
	    __vecAppendItem(NULL, (char *)result.text, __vecSize(result.text));
	return __vecAppendItem(NULL, &retVal, sizeof(struct includeMacro));
malformed : {
	*err = 1;
	return NULL;
}
}
static enum lexerItemState includeMacroValidate(const void *oldData,
                                                struct __vec *oldText,
                                                struct __vec *newText, long pos,
                                                const void *data, int *err) {
	if (err != NULL)
		*err = 0;

	if (!expectMacroAndSkip("include", newText, pos, &pos))
		return LEXER_DESTROY;

	struct parsedString str;
	long end;
	if (!stringParse(newText, pos, &end, &str, err)) {
		*err = 1;
		return LEXER_DESTROY;
	}

	enum lexerItemState retVal = LEXER_MODIFED;
	const struct includeMacro *oldMacro = oldData;
	if (__vecSize(oldMacro->fileName) == __vecSize(str.text))
		if (0 == strncmp((const char *)oldMacro->fileName, (char *)str.text,
		                 __vecSize(str.text)))
			retVal = LEXER_UNCHANGED;

	parsedStringDestroy(&str);
	return retVal;
}
static struct __vec *includeMacroUpdate(const void *oldData,
                                        struct __vec *oldText,
                                        struct __vec *newText, long pos,
                                        long *end, const void *data, int *err) {
	if (!expectMacroAndSkip("include", newText, pos, &pos))
		return NULL;

	pos = skipWhitespace(newText, pos) - (void *)newText;
	struct parsedString str;
	assert(0 != stringParse(newText, pos, end, &str, err));
	struct includeMacro retVal;
	retVal.fileName =
	    __vecAppendItem(NULL, (void *)str.text, __vecSize(str.text));
	parsedStringDestroy(&str);

	return __vecAppendItem(NULL, &retVal, sizeof(struct includeMacro));
}
static void defineMacroDestroy(void *macro) {
	struct defineMacro *macro2 = macro;
	__vecDestroy(macro2->name);
	__vecDestroy(macro2->text);
}
static void defineMacroDestroyLexerItem(struct __lexerItem *item) {
	defineMacroDestroy(lexerItemValuePtr(item));
}
static struct __vec *defineMacroLex(struct __vec *text, long pos, long *end,
                                    const void *data, int *err) {
	if (!expectMacroAndSkip("define", text, pos, &pos))
		return NULL;

	pos = skipWhitespace(text, pos) - (void *)text;

	int len = 0;
	while (isalnum((pos + len)[(char *)text]))
		len++;

	if (isdigit(pos[(char *)text]))
		goto malformed;

	__auto_type name = __vecAppendItem(NULL, &pos[(char *)text], len);

	pos += len;
	__auto_type end2 = findEndOfLine(text, pos) - (void *)text;
	// Ignore whitespace from end2
	for (end2--; isblank(end2[(char *)text]); end2--)
		;
	struct __vec *replacement =
	    __vecAppendItem(NULL, &pos[(char *)text], end2 - pos);
	if (end != NULL)
		*end = findNextLine(text, pos) - (void *)text;

	struct defineMacro retVal;
	retVal.name = name;
	retVal.text = replacement;
	return __vecAppendItem(NULL, &retVal, sizeof(struct defineMacro));
malformed : {
	*err = 1;
	return NULL;
}
}
static struct __vec *defineMacroUpdate(const void *oldData,
                                       struct __vec *oldText,
                                       struct __vec *newText, long pos,
                                       long *end, const void *data, int *err) {
	return defineMacroLex(newText, pos, end, data, err);
}
static enum lexerItemState defineMacroValidate(const void *oldData,
                                               struct __vec *oldText,
                                               struct __vec *newText, long pos,
                                               const void *data, int *err) {
	__auto_type res = defineMacroLex(newText, pos, NULL, data, err);
	if (res == NULL)
		return LEXER_DESTROY;

	const struct defineMacro *newDefine = (const void *)res;
	const struct defineMacro *oldDefine = oldData;
	enum lexerItemState retVal = LEXER_MODIFED;
	if (__vecSize(oldDefine->name) != __vecSize(newDefine->name))
		goto modified;
	if (__vecSize(oldDefine->text) != __vecSize(newDefine->text))
		goto modified;
	if (0 != strncmp((const char *)newDefine->name, (const char *)oldDefine->name,
	                 __vecSize(newDefine->name)))
		goto modified;
	if (0 != strncmp((const char *)newDefine->text, (const char *)oldDefine->text,
	                 __vecSize(newDefine->text)))
		goto modified;
	retVal = LEXER_UNCHANGED;
	goto returnLabel;
modified:
	retVal = LEXER_MODIFED;
returnLabel:
	defineMacroDestroy((struct defineMacro *)res);
	__vecDestroy(res);
	return retVal;
}
void includeMacroDestroy(struct __lexerItem *macro) {
	struct includeMacro *macro2 = lexerItemValuePtr(macro);
	__vecDestroy(macro2->fileName);
}
struct __lexerItemTemplate *createDefineMacroTemplate() {
	struct __lexerItemTemplate *template =
	    malloc(sizeof(struct __lexerItemTemplate));

	template->cloneData = NULL;
	template->data = NULL;
	template->killItemData = defineMacroDestroyLexerItem;
	template->killTemplateData = NULL;
	template->lexItem = defineMacroLex;
	template->update = defineMacroUpdate;
	template->validateOnModify = defineMacroValidate;

	return template;
}
struct __lexerItemTemplate *createIncludeMacroTemplate() {
	struct __lexerItemTemplate *template =
	    malloc(sizeof(struct __lexerItemTemplate));

	template->lexItem = includeMacroLex;
	template->validateOnModify = includeMacroValidate;
	template->update = includeMacroUpdate;
	template->cloneData = NULL;
	template->killTemplateData = NULL;
	template->data = NULL;
	template->killItemData = includeMacroDestroy;

	return template;
}
/**
 * This takes a processed text(result of preprocessor),then maps it to source
 */
long mappedPosition(const strSourceMapping mapping, long processedPos) {
	long processedStart = 0;
	long sourcePrevEnd = 0;
	for (long i = 0; i != strSourceMappingSize(mapping); i++) {
		__auto_type sourceLen = mapping[i].end - mapping[i].start;

		/**
		 * Check if between gap
		 * Source:
		 * #define x ["123"]
		 * ["a"] x "[b"]
		 *
		 * Processed:
		 * ["a"] ["123"] ["b"]
		 *
		 * Gap is where "123" is in processed text( "123" comes from x)
		 */
		__auto_type oldProcessedStart = processedStart;
		processedStart += mapping[i].start - sourcePrevEnd;
		if (processedStart > processedPos && processedPos >= oldProcessedStart)
			return mapping[i].start;

		oldProcessedStart = processedStart;
		processedStart += sourceLen;
		if (processedStart > processedPos && processedPos >= oldProcessedStart)
			return processedPos - oldProcessedStart;

		sourcePrevEnd = mapping[i].end;
	}
	return -1;
}
struct __lexerItemTemplate *defineMacroTemplate;
struct __lexerItemTemplate *includeMacroTemplate;
static strLexerItemTemplate macroTemplates;
static void initMacroTemplates() __attribute__((constructor));
static void destroyMacroTemplates() __attribute__((destructor));
static void initMacroTemplates() {
	defineMacroTemplate = createDefineMacroTemplate();
	includeMacroTemplate = createIncludeMacroTemplate();
	const struct __lexerItemTemplate *templates[] = {defineMacroTemplate,
	                                                 includeMacroTemplate};
	__auto_type count = sizeof(templates) / sizeof(*templates);
	macroTemplates = strLexerItemTemplateAppendData(NULL, templates, count);
}
static void destroyMacroTemplates() {
	for (long i = 0; i != strLexerItemTemplateSize(macroTemplates); i++) {
		if (macroTemplates[i]->killTemplateData != NULL)
			macroTemplates[i]->killTemplateData(macroTemplates[i]->data);
		free(macroTemplates[i]);
	}

	strLexerItemTemplateDestroy(&macroTemplates);
}
static int charCmp(const void *a, const void *b) {
	return *(char *)a == *(char *)b;
}
struct __lexer *createPreprocessorLexer() {
	return lexerCreate(NULL, macroTemplates, charCmp, skipNonMacro);
}
static void insertFileIntoFile(FILE *writeTo, FILE *readFrom) {
	char textBuffer[1024];
	fseek(readFrom, 0, SEEK_END);
	__auto_type end = ftell(readFrom);
	fseek(readFrom, 0, SEEK_SET);
	__auto_type start = ftell(readFrom);
	while (start != end) {
		__auto_type size = (end - start >= 1024) ? 1024 : end - start;
		fread(textBuffer, 1, size, readFrom);
		fwrite(textBuffer, 1, size, writeTo);
		start += size;
	}
}
FILE *createPreprocessedFile(struct __lexer *lexer, struct __vec *text,
                             int *err) {
	strSourceMappingDestroy(&sourceMappings);
 
	if (preprocessedSource != NULL)
		fclose(preprocessedSource);
	preprocessedSource = tmpfile();

	lexerUpdate(lexer, text, err);
	__auto_type macro = llLexerItemFirst(lexerGetItems(lexer));

	long oldPos = 0;
	for (; macro != NULL; macro = llLexerItemNext(macro)) {
		__auto_type ptr = llLexerItemValuePtr(macro);

		// Push slice of source text
		fwrite(oldPos + (void *)text, 1, ptr->start - oldPos, preprocessedSource);

		if (ptr->template == defineMacroTemplate) {
			struct defineMacro *value = lexerItemValuePtr(ptr);
			fwrite((void *)value->text, 1, __vecSize(value->text),
			       preprocessedSource);
		} else if (ptr->template == includeMacroTemplate) {
			struct includeMacro *value = lexerItemValuePtr(ptr);
			__auto_type len = __vecSize(value->fileName);
			char nameBuffer[len + 1];
			memcpy(nameBuffer, value->fileName, len);
			nameBuffer[len] = '\0';
			__auto_type readFrom = fopen(nameBuffer, "r");
			assert(NULL != readFrom); // TODO whine if file doesnt exist
			insertFileIntoFile(preprocessedSource, readFrom);
			fclose(readFrom);
		} else {
			assert(0);
		}
		
		//Push mapping
		struct sourceMapping tmp;
		tmp.start=oldPos;
		tmp.end=ptr->end;
		strSourceMappingAppendItem(sourceMappings,tmp);
		
		oldPos=ptr->end;
	}

	return preprocessedSource;
}
