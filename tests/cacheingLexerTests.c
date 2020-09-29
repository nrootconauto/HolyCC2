#include <assert.h>
#include <cacheingLexer.h>
#include <ctype.h>
#include <str.h>
#include <string.h>
static const void *skipWhitespace(struct __vec *text, long from) {
	for (__auto_type ptr = text + from; ptr != text + __vecSize(text); ptr++)
		if (!isblank(*(char *)ptr))
			return ptr;
	return NULL;
}
static const char *keywords[] = {"graph", "--", "[", "]", "=",
                                 "Label", ";",  "{", "}"};
static struct __vec *keywordLex(struct __vec *new, long pos, long *end,
                                const void *data) {
	__auto_type count = sizeof(keywords) / sizeof(*keywords);
	for (int i = 0; i != count; i++) {
		__auto_type len = strlen(keywords[count]);
		if (__vecSize(new) - pos < len)
			continue;

		if (0 == strncmp((void *)new, keywords[count], len)) {
			*end = pos + len;
			return __vecAppendItem(NULL, &keywords[count], sizeof(*keywords));
		}
	}
	return NULL;
}
static enum lexerItemState keywordValidate(const void *itemData,
                                           struct __vec *old, struct __vec *new,
                                           long pos, const void *data) {
	const char *itemText = *(const char **)itemData;
	if (__vecSize(new) - pos < strlen(itemData))
		return LEXER_DESTROY;
	if (0 == strncmp(itemText, (void *)new + pos, strlen(itemText)))
		return LEXER_UNCHANGED;
	return LEXER_DESTROY;
}
static long keywordUpdate(void *data, struct __vec *old, struct __vec *new,
                          long x, const void *data2) {
	// Dummy function,should never reach here
	assert(0);
}
static struct __lexerItemTemplate keywordTemplateCreate() {
	struct __lexerItemTemplate retVal;

	retVal.data = keywords;
	retVal.killItemData = NULL;
	retVal.validateOnModify = keywordValidate;
	retVal.lexItem = keywordLex;
	retVal.update = keywordUpdate;

	return retVal;
}
static enum lexerItemState nameValidate(const void *itemData, struct __vec *old,
                                        struct __vec *new, long pos,
                                        const void *data) {
	long alNumCount = 0;
	for (void *ptr = (void *)new + pos; ptr < __vecSize(new) + (void *)new;
	     ptr++, alNumCount++)
		if (!isalnum(ptr))
			break;

	__auto_type count = sizeof(keywords) / sizeof(*keywords);
	for (int i = 0; i != count; i++) {
		__auto_type len = strlen(keywords[count]);

		if (len != alNumCount)
			continue;

		if (strncmp((void *)new + pos, keywords[count], len) == 0)
			return LEXER_DESTROY;
	}

	if (__vecSize(old) == alNumCount)
		if (0 == strcmp((void *)old, (void *)new + pos))
			return LEXER_UNCHANGED;

	return (alNumCount == 0) ? LEXER_DESTROY : LEXER_MODIFED;
}
static long nameUpdate(void *data, struct __vec *old, struct __vec *new, long x,
                       const void *data2) {}
static struct __lexerItemTemplate nameTemplateCreate() {
	struct __lexerItemTemplate retVal;

	retVal.data = keywords;
	retVal.killItemData = NULL;
	retVal.validateOnModify = keywordValidate;
	retVal.lexItem = keywordLex;
	retVal.update = keywordUpdate;

	return retVal;
}
void cachingLexerTests() {
	const char *text = "graph h20 {"
	                   "    h0 [Label=\"H\"];"
	                   "    h0 -- 01;"
	                   "    h0 -- 02;"
	                   "}";
}
