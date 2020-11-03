#include <assert.h>
#include <cacheingLexer.h>
#include <cacheingLexerItems.h>
#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stringParser.h>
#include <utf8Encode.h>

static enum lexerItemState keywordValidate(const void *itemData,
                                           struct __vec *old, struct __vec *new,
                                           long pos, const void *data,
                                           int *err) {
	if (err != NULL)
		*err = 0;

	const char *itemText = *(const char **)itemData;
	if (__vecSize(new) - pos < strlen(itemText))
		return LEXER_DESTROY;
	if (0 == strncmp(itemText, (void *)new + pos, strlen(itemText))) {
		// If alpha-numeric,ensure same length of alhpa-numeric charactors
		__auto_type alnumCount = countAlnum(new, pos);
		if (alnumCount != 0)
			if (alnumCount != strlen((const char *)itemData))
				return LEXER_DESTROY;

		return LEXER_UNCHANGED;
	}
	return LEXER_DESTROY;
}
static struct __vec *keywordUpdate(const void *data, struct __vec *old,
                                   struct __vec *new, long x, long *end,
                                   const void *data2, int *err) {
	// Dummy function,should never reach here
	assert(0);
}
static void keywordsKill(void *data) { strStrDestroy(data); }
struct __lexerItemTemplate keywordTemplateCreate(const char **keywords,
                                                 long keywordCount) {
	struct __lexerItemTemplate retVal;

	strStr keywordsVec = strStrResize(NULL, keywordCount);
	for (long i = 0; i != keywordCount; i++) {
		keywordsVec[i] = keywords[i];
	}
	qsort(keywordsVec, keywordCount, sizeof(const char *),
	      (int (*)(const void *, const void *))strcmp);

	retVal.data = keywordsVec;
	retVal.killItemData = NULL;
	retVal.validateOnModify = keywordValidate;
	retVal.lexItem = keywordLex;
	retVal.update = keywordUpdate;
	retVal.killTemplateData = keywordsKill;
	retVal.cloneData = NULL;
	retVal.isAdjChar =NULL;

	return retVal;
}
int isKeyword(struct __vec *new, long pos, const strStr keywords) {
	__auto_type count = strStrSize(keywords);
	__auto_type alNumCount = countAlnum(new, pos);
	for (int i = 0; i != count; i++) {
		if (alNumCount != 0) {
			if (strlen(keywords[i]) == alNumCount)
				if (0 == strncmp(keywords[i], (char *)new + pos, alNumCount))
					return 1;
		} else {
			if (0 == strncmp(keywords[i], (char *)new + pos, strlen(keywords[i])))
				return 1;
		}
	}
	return 0;
}
static enum lexerItemState nameValidate(const void *itemData, struct __vec *old,
                                        struct __vec *new, long pos,
                                        const void *data, int *err) {
	if (err != NULL)
		*err = 0;

	__auto_type alNumCount = countAlnum(new, pos);
	if (isdigit(*((char *)new + pos)))
		return LEXER_DESTROY;

	if (isKeyword(new, pos, (const strStr)data))
		return LEXER_DESTROY;

	if (__vecSize(old) == alNumCount)
		if (0 == strncmp((void *)old, (void *)new + pos, alNumCount))
			return LEXER_UNCHANGED;

	return (alNumCount == 0) ? LEXER_DESTROY : LEXER_MODIFED;
}
static struct __vec *nameUpdate(const void *data, struct __vec *old,
                                struct __vec *new, long pos, long *end,
                                const void *data2, int *err) {
	if (err != NULL)
		*err = 0;

	__auto_type alNumCount = countAlnum(new, pos);

	*end = pos + alNumCount;

	char buffer[alNumCount + 1];
	memcpy(buffer, (char *)new + pos, alNumCount);
	buffer[alNumCount] = 0;

	return __vecAppendItem(NULL, buffer, alNumCount + 1);
}

static struct __vec *intLex(struct __vec *new, long pos, long *end,
                            const void *data, int *err) {
	if (err != NULL)
		*err = 0;

	struct lexerInt find;
	if (intParse(new, pos, end, &find, err)) {
		return __vecAppendItem(NULL, &find, sizeof(struct lexerInt));
	} else
		return NULL;
}
static enum lexerItemState intValidate(const void *itemData, struct __vec *old,
                                       struct __vec *new, long pos,
                                       const void *data, int *err) {
	// Just check if new is same as old
	__auto_type alnumCount = countAlnum(new, pos);
	if (__vecSize(old) != alnumCount)
		return LEXER_DESTROY;
	if (0 == strncmp((char *)old, (char *)new, alnumCount))
		return LEXER_UNCHANGED;
	return (intParse(new, pos, NULL, NULL, err)) ? LEXER_MODIFED : LEXER_DESTROY;
}
static int floatIsChar(int chr) {
 return isdigit(chr)||NULL!=strchr("e+-.",chr);
} 
static int intIsChar(int chr) {
 return isdigit(chr)||NULL!=strchr("bxabcdefABCDEF",chr);
}
static int nameIsChar(int chr) {
 return isalnum(chr)||chr=='_';
}
static int strIsChar(int chr) {
 return 1;
} 
static struct __vec *intUpdate(const void *data, struct __vec *old,
                               struct __vec *new, long pos, long *end,
                               const void *data2, int *err) {
	struct lexerInt find;
	intParse(new, pos, end, &find, err);

	return __vecAppendItem(NULL, &find, sizeof(struct lexerInt));
}
struct __lexerItemTemplate nameTemplateCreate(const char **keywords,
                                              long keywordCount) {
	struct __lexerItemTemplate retVal;

	strStr keywordsVec = strStrResize(NULL, keywordCount);
	for (long i = 0; i != keywordCount; i++)
		keywordsVec[i] = keywords[i];
	qsort(keywordsVec, keywordCount, sizeof(const char *),
	      (int (*)(const void *, const void *))strcmp);

	retVal.data = keywordsVec;
	retVal.killItemData = NULL;
	retVal.validateOnModify = nameValidate;
	retVal.lexItem = nameLex;
	retVal.update = nameUpdate;
	retVal.killTemplateData = keywordsKill;
	retVal.cloneData = NULL;
	retVal.isAdjChar =nameIsChar;
	
	return retVal;
}
struct __lexerItemTemplate intTemplateCreate() {
	struct __lexerItemTemplate retVal;

	retVal.data = NULL;
	retVal.killItemData = NULL;
	retVal.validateOnModify = intValidate;
	retVal.lexItem = intLex;
	retVal.update = intUpdate;
	retVal.killTemplateData = NULL;
	retVal.cloneData = NULL;
	retVal.isAdjChar =intIsChar;

	return retVal;
}

static struct __vec *floatingUpdate(const void *data, struct __vec *old,
                                    struct __vec *new, long pos, long *end,
                                    const void *data2, int *err) {
	struct lexerFloating find;
	floatingParse(new, pos, end, &find, err);

	return __vecAppendItem(NULL, &find, sizeof(struct lexerFloating));
}
static struct __vec *floatingLex(struct __vec *new, long pos, long *end,
                                 const void *data, int *err) {
	struct lexerFloating item;
	if (floatingParse(new, pos, end, &item, err)) {
		return __vecAppendItem(NULL, &item, sizeof(struct lexerFloating));
	} else
		return NULL;
}
static enum lexerItemState floatingValidate(const void *itemData,
                                            struct __vec *old,
                                            struct __vec *new, long pos,
                                            const void *data, int *err) {
	long end;
	if (floatingParse(new, pos, &end, NULL, err)) {
		if (__vecSize(old) == end - pos)
			return (0 == strncmp((char *)old, (char *)new + pos, end - pos))
			           ? LEXER_UNCHANGED
			           : LEXER_MODIFED;
		return LEXER_MODIFED;
	} else
		return LEXER_DESTROY;
}
struct __lexerItemTemplate floatingTemplateCreate() {
	struct __lexerItemTemplate retVal;
	retVal.data = NULL;
	retVal.lexItem = floatingLex;
	retVal.update = floatingUpdate;
	retVal.validateOnModify = floatingValidate;
	retVal.killItemData = NULL;
	retVal.killTemplateData = NULL;
	retVal.cloneData = NULL;
	retVal.isAdjChar =floatIsChar;
	
	return retVal;
}
static struct __vec *stringUpdate(const void *data, struct __vec *old,
                                  struct __vec *new, long pos, long *end,
                                  const void *data2, int *err) {
	struct parsedString find;
	if (stringParse(new, pos, end, &find, err))
		return __vecAppendItem(NULL, &find, sizeof(struct parsedString));
	return NULL;
}
enum lexerItemState stringValidate(const void *itemData, struct __vec *old,
                                   struct __vec *new, long pos,
                                   const void *data, int *err) {
	struct parsedString find;
	long end;
	if (!stringParse(new, pos, &end, NULL, err))
		return LEXER_DESTROY;

	__auto_type oldSize = __vecSize(old);
	if (oldSize == end)
		if (0 == strncmp((char *)old, (char *)new + pos, oldSize))
			return LEXER_UNCHANGED;

	return LEXER_MODIFED;
}
struct __lexerItemTemplate stringTemplateCreate() {
	struct __lexerItemTemplate retVal;

	retVal.killItemData = NULL;
	retVal.lexItem = stringLex;
	retVal.update = stringUpdate;
	retVal.validateOnModify = stringValidate;
	retVal.data = NULL;
	retVal.killTemplateData = NULL;
	retVal.cloneData = NULL;
	retVal.isAdjChar =strIsChar;

	return retVal;
}
