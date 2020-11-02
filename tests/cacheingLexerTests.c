#include <assert.h>
#include <cacheingLexer.h>
#include <cacheingLexerItems.h>
#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <str.h>
#include <string.h>
#include <utf8Encode.h>
STR_TYPE_DEF(char *, Str);
STR_TYPE_FUNCS(char *, Str);

static const char *keywords[] = {"graph", "subgraph", "--", "[", "]",
                                 "=",     ";",        "{",  "}"};
STR_TYPE_DEF(char, Char);
STR_TYPE_FUNCS(char, Char);
static int charEq(const void *a, const void *b) {
	return *(char *)a == *(char *)b;
}
static llLexerItem expectKeyword(llLexerItem node,
                                 struct __lexerItemTemplate *template,
                                 const char *spelling) {
	__auto_type lexerItem = llLexerItemValuePtr(node);
	assert(lexerItem->template == template);
	__auto_type value = *(const char **)lexerItemValuePtr(lexerItem);
	assert(0 == strcmp(value, spelling));
	return llLexerItemNext(node);
}
static llLexerItem expectName(llLexerItem node,
                              struct __lexerItemTemplate *template,
                              const char *spelling) {
	__auto_type lexerItem = llLexerItemValuePtr(node);
	assert(lexerItem->template == template);
	__auto_type value = (const char *)lexerItemValuePtr(lexerItem);
	assert(0 == strcmp(value, spelling));
	return llLexerItemNext(node);
}
static llLexerItem expectInt(llLexerItem node,
                             struct __lexerItemTemplate *template,
                             unsigned long value) {
	__auto_type lexerItem = llLexerItemValuePtr(node);
	assert(lexerItem->template == template);
	__auto_type value2 = (struct lexerInt *)lexerItemValuePtr(lexerItem);
	assert(value2->value.uLong == value);
	return llLexerItemNext(node);
}
static llLexerItem expectString(llLexerItem node,
                                struct __lexerItemTemplate *template,
                                const char *value, int isChar) {
	__auto_type lexerItem = llLexerItemValuePtr(node);
	assert(lexerItem->template == template);

	__auto_type value2 = (struct parsedString *)lexerItemValuePtr(lexerItem);
	assert(value2->isChar == isChar);
	assert(0 == strcmp((char *)value2->text, value));

	return llLexerItemNext(node);
}
static enum cacheBlobRetCode updateBlob(void *data, llLexerItem start,
                                        llLexerItem end, llLexerItem *start2,
                                        llLexerItem *end2,
                                        enum cacheBlobFlags flags) {
	enum cacheBlobFlags *expected = data;
	assert(*expected == flags);
	return CACHE_BLOB_RET_KEEP;
}
void cacheingLexerBlobTests() {
	enum cacheBlobFlags expected;
	struct cacheBlobTemplate blobTemplate;
	blobTemplate.killData = NULL;
	blobTemplate.mask = CACHE_FLAG_REMOVE | CACHE_FLAG_INSERT;
	blobTemplate.update = updateBlob;

	__auto_type nameTemplate = nameTemplateCreate(NULL, 0);
	__auto_type templates = strLexerItemTemplateResize(NULL, 1);
	templates[0] = &nameTemplate;

	const char *text = "a  c d";
	__auto_type str = strCharAppendData(NULL, (char *)text, strlen(text));
	__auto_type lexer =
	    lexerCreate((struct __vec *)str, templates, charEq, skipWhitespace);
	{
		__auto_type items = lexerGetItems(lexer);
		__auto_type node = __llGetFirst(items);
		node = expectName(node, &nameTemplate, "a");
		node = expectName(node, &nameTemplate, "c");
		node = expectName(node, &nameTemplate, "d");
	}
	/**
	 * Create a blob
	 */
	__auto_type firstItem = llLexerItemFirst(lexerGetItems(lexer));
	__auto_type thirdItem = llLexerItemNext(llLexerItemNext(firstItem));
	__auto_type blob1 =
	    blobCreate(&blobTemplate, firstItem, thirdItem, &expected);

	text = "a b c d";
	str = strCharAppendData(NULL, (char *)text, strlen(text));
	int err;
	lexerUpdate(lexer, (struct __vec *)str, &err);
	assert(!err);
	{
		__auto_type node = blob1->start;
		node = expectName(node, &nameTemplate, "a");
		node = expectName(node, &nameTemplate, "b");
		node = expectName(node, &nameTemplate, "c");
		assert(node == blob1->end);
	}
}
void cachingLexerTests() {
	__auto_type kwCount = sizeof(keywords) / sizeof(*keywords);

	__auto_type nameTemplate = nameTemplateCreate(keywords, kwCount);
	__auto_type keywordTemplate = keywordTemplateCreate(keywords, kwCount);
	__auto_type intTemplate = intTemplateCreate();
	__auto_type floatingTemplate = floatingTemplateCreate();
	__auto_type stringTemplate = stringTemplateCreate();
	__auto_type templates = strLexerItemTemplateResize(NULL, 5);
	templates[0] = &nameTemplate;
	templates[1] = &keywordTemplate;
	templates[2] = &intTemplate;
	templates[3] = &floatingTemplate;
	templates[4] = &stringTemplate;
	int err;
	{
		const char *text = "a--b--c";
		__auto_type str = strCharAppendData(NULL, (char *)text, strlen(text));
		__auto_type lexer =
		    lexerCreate((struct __vec *)str, templates, charEq, skipWhitespace);
		__auto_type items = lexerGetItems(lexer);
		__auto_type node = __llGetFirst(items);
		node = expectName(node, &nameTemplate, "a");
		node = expectKeyword(node, &keywordTemplate, "--");
		node = expectName(node, &nameTemplate, "b");
		node = expectKeyword(node, &keywordTemplate, "--");
		node = expectName(node, &nameTemplate, "c");
		// Insert(ignore items between first unchanged and last changed item,see
		// isAdjChar)

		strCharDestroy(&str);
		text = "a--b--c1234";
		str = strCharAppendData(NULL, (char *)text, strlen(text));
		lexerUpdate(lexer, (struct __vec *)str, &err);
		assert(err == 0);
		items = lexerGetItems(lexer);
		node = __llGetFirst(items);
		node = expectName(node, &nameTemplate, "a");
		node = expectKeyword(node, &keywordTemplate, "--");
		node = expectName(node, &nameTemplate, "b");
		node = expectKeyword(node, &keywordTemplate, "--");
		node = expectName(node, &nameTemplate, "c1234");

		// Insert
		strCharDestroy(&str);
		text = "a1234--b--c";
		str = strCharAppendData(NULL, (char *)text, strlen(text));
		lexerUpdate(lexer, (struct __vec *)str, &err);
		assert(err == 0);

		items = lexerGetItems(lexer);
		node = __llGetFirst(items);
		node = expectName(node, &nameTemplate, "a1234");
		node = expectKeyword(node, &keywordTemplate, "--");
		node = expectName(node, &nameTemplate, "b");
		node = expectKeyword(node, &keywordTemplate, "--");
		node = expectName(node, &nameTemplate, "c");
		// Delete (overwrite)
		strCharDestroy(&str);
		text = "a1234b--c";
		str = strCharAppendData(NULL, (char *)text, strlen(text));
		lexerUpdate(lexer, (struct __vec *)str, &err);
		assert(err == 0);
		items = lexerGetItems(lexer);
		node = __llGetFirst(items);
		node = expectName(node, &nameTemplate, "a1234b");
		node = expectKeyword(node, &keywordTemplate, "--");
		node = expectName(node, &nameTemplate, "c");
		strCharDestroy(&str);
		// Delete (Not part of any item)
		text = "a1234b c";
		str = strCharAppendData(NULL, (char *)text, strlen(text));
		lexerUpdate(lexer, (struct __vec *)str, &err);
		assert(err == 0);
		items = lexerGetItems(lexer);
		node = __llGetFirst(items);
		node = expectName(node, &nameTemplate, "a1234b");
		node = expectName(node, &nameTemplate, "c");
		strCharDestroy(&str);
	}
	{
		const char *text = "graph h2O {\n"
		                   "    h [Label=\"H\"];\n"
		                   "    h -- O1;\n"
		                   "    h -- O2;\n"
		                   "}";
		__auto_type str = strCharAppendData(NULL, (char *)text, strlen(text));
		__auto_type lexer =
		    lexerCreate((struct __vec *)str, templates, charEq, skipWhitespace);
		__auto_type items = lexerGetItems(lexer);

		__auto_type node = __llGetFirst(items);
		node = expectKeyword(node, &keywordTemplate, "graph");
		node = expectName(node, &nameTemplate, "h2O");
		node = expectKeyword(node, &keywordTemplate, "{");
		node = expectName(node, &nameTemplate, "h");
		node = expectKeyword(node, &keywordTemplate, "[");
		node = expectName(node, &nameTemplate, "Label");
		node = expectKeyword(node, &keywordTemplate, "=");
		node = expectString(node, &stringTemplate, "H", 0);
		node = expectKeyword(node, &keywordTemplate, "]");
		node = expectKeyword(node, &keywordTemplate, ";");
		node = expectName(node, &nameTemplate, "h");
		node = expectKeyword(node, &keywordTemplate, "--");
		node = expectName(node, &nameTemplate, "O1");
		node = expectKeyword(node, &keywordTemplate, ";");
		node = expectName(node, &nameTemplate, "h");
		node = expectKeyword(node, &keywordTemplate, "--");
		node = expectName(node, &nameTemplate, "O2");
		node = expectKeyword(node, &keywordTemplate, ";");
		node = expectKeyword(node, &keywordTemplate, "}");
	}

	cacheingLexerBlobTests();
}
