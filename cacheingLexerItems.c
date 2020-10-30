#include <assert.h>
#include <cacheingLexer.h>
#include <cacheingLexerItems.h>
#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stringParser.h>
#include <utf8Encode.h>
STR_TYPE_DEF(const char *, Str);
STR_TYPE_FUNCS(const char *, Str);
const void *skipWhitespace(struct __vec *text, long from) {
	for (__auto_type ptr = (void *)text + from;
	     ptr != (void *)text + __vecSize(text); ptr++)
		if (!isblank(*(char *)ptr) && *(char *)ptr != '\n' && *(char *)ptr != '\r')
			return ptr;
	return __vecSize(text) + (void *)text;
}
static long countAlnum(struct __vec *data, long pos) {
	long alNumCount = 0;
	for (void *ptr = (void *)data + pos; ptr < __vecSize(data) + (void *)data;
	     ptr++, alNumCount++)
		if (!isalnum(*(char *)ptr))
			break;

	return alNumCount;
}
static struct __vec *keywordLex(struct __vec *new, long pos, long *end,
                                const void *data, int *err) {
	if (err != NULL)
		*err = 0;

	strStr keywords = (strStr)data;
	__auto_type count = strStrSize(keywords);

	__auto_type alnumCount = countAlnum(new, pos);
	long largestIndex = -1;
	for (int i = 0; i != count; i++) {
		__auto_type len = strlen(keywords[i]);
		if (__vecSize(new) - pos < len)
			continue;

		if (0 == strncmp((void *)new + pos, keywords[i], len)) {
			if (alnumCount != 0)
				if (alnumCount != len)
					continue;

			if (alnumCount == 0) {
				if (largestIndex == -1)
					largestIndex = i;
				else if (strlen(keywords[largestIndex]) < len)
					largestIndex = i;

				*end = pos + strlen(keywords[largestIndex]);
			} else
				return __vecAppendItem(NULL, &keywords[i], sizeof(*keywords));
		}
	}
	if (alnumCount == 0 && largestIndex != -1)
		return __vecAppendItem(NULL, &keywords[largestIndex], sizeof(*keywords));

	return NULL;
}
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
static struct __vec *nameLex(struct __vec *new, long pos, long *end,
                             const void *data, int *err) {
	if (err != NULL)
		*err = 0;

	__auto_type alNumCount = countAlnum(new, pos);
	if (alNumCount == 0)
		return NULL;

	if (isdigit(((char *)new)[pos]))
		return NULL;

	if (isKeyword(new, pos, (const strStr)data))
		return NULL;

	*end = pos + alNumCount;

	int len = alNumCount;
	char z = 0;
	__auto_type retVal = __vecAppendItem(NULL, (char *)new + pos, len);
	return __vecAppendItem(retVal, &z, 1);
}
static int intParse(struct __vec *new, long pos, long *end,
                    struct lexerInt *retVal, int *err) {
	if (err != NULL)
		*err = 0;

	unsigned long valueU = 0;
	int base = 10;

	__auto_type New = (char *)new + pos;
	if (!isdigit(*(New)))
		return 0;
	__auto_type endPtr = (char *)new + __vecSize(new);
	if (*New == '0') {
		New++;
		if (endPtr == New) {
			valueU = 0;
			goto dumpU;
		} else if (!isalnum(*New)) {
			valueU = 0;
			goto dumpU;
		}

		if (*New == 'x') {
			base = 16;

			New++;
			__auto_type alnumCount = countAlnum(new, New - (char *)new);

			// Ensure all digits are hex digits
			for (int i = 0; i != alnumCount; i++)
				if (!isxdigit(New[i]))
					goto malformed;

			New += alnumCount;

			__auto_type startAt = (void *)new + pos;
			__auto_type slice =
			    __vecAppendItem(NULL, startAt, (void *)New - (void *)startAt);
			sscanf((char *)slice, "%lx", &valueU);
			__vecDestroy(slice);

			goto dumpU;
		} else if (*New >= 0 && *New <= '7') {
			base = 8;

			New++;
			__auto_type alnumCount = countAlnum(new, New - (char *)new);

			// Ensure all octal digits
			for (int i = 0; i != alnumCount; i++)
				if (!(New[i] >= '0' && New[i] <= '7'))
					goto malformed;

			New += alnumCount;

			__auto_type startAt = (void *)new + pos;
			__auto_type slice =
			    __vecAppendItem(NULL, startAt, (void *)New - (void *)startAt);
			sscanf((char *)slice, "%lo", &valueU);
			__vecDestroy(slice);

			goto dumpU;
		} else if (*New == 'b' || *New == 'B') {
			base = 2;

			New++;
			__auto_type alnumCount = countAlnum(new, New - (char *)new);

			// Ensure all digits are binary digits
			for (int i = 0; i != alnumCount; i++)
				if (!(New[i] == '0' && New[i] == '1'))
					goto malformed;

			for (int i = 0; i != alnumCount; i++)
				valueU = (valueU << 1) | (New[i] == '1' ? 1 : 0);

			New += alnumCount;
			goto dumpU;
		} else {
			goto malformed;
		}
	} else {
		__auto_type alnumCount = countAlnum(new, pos);

		// Ensure are decimal digits
		for (int i = 0; i != alnumCount; i++)
			if (!isdigit(New[i]))
				goto malformed;

		__auto_type slice = __vecAppendItem(NULL, New, alnumCount);
		sscanf((char *)slice, "%lu", &valueU);
		__vecDestroy(slice);

		New += alnumCount;
		goto dumpU;
	}
dumpU : {
	__auto_type end2 = New - (char *)new;
	if (end != NULL)
		*end = end2;

	// Check to ensure isn't a float
	if (end2 + (char *)new < endPtr)
		if (((char *)new)[end2] == '.')
			return 0;

	if (retVal == NULL)
		return 1;

	retVal->base = base;
	retVal->type = (UINT_MAX <= valueU) ? INT_SLONG : INT_SINT;
	if (INT_MAX < valueU)
		retVal->value.sLong = valueU;
	else
		retVal->value.sInt = valueU;

	return 1;
}
malformed : {
	*err = 1;
	return 0;
}
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

	return retVal;
}
static int floatingParse(struct __vec *vec, long pos, long *end,
                         struct lexerFloating *retVal, int *err) {
	if (err != NULL)
		*err = 0;

	__auto_type endPtr = __vecSize(vec) + (char *)vec;
	__auto_type currPtr = (char *)vec + pos;
	struct lexerFloating f;
	f.base = 0;
	f.frac = 0;
	f.exponet = 0;

	__auto_type alnumCount = countAlnum(vec, pos);
	if (alnumCount == 0)
		return 0;
	if (!isdigit(*((char *)vec + pos)))
		return 0;

	int exponetIndex = -1;
	for (int i = 0; i != alnumCount; i++) {
		if (!isdigit(currPtr[i])) {
			if ((currPtr[i] == 'e' || currPtr[i] == 'E') && exponetIndex == -1) {
				exponetIndex = pos + i;
			} else
				goto malformed;
		}
	}

	if (alnumCount != 0) {
		__auto_type slice = __vecAppendItem(
		    NULL, currPtr,
		    pos + ((exponetIndex != -1) ? exponetIndex : alnumCount));
		sscanf((char *)slice, "%lu", &f.base);
		__vecDestroy(slice);

		currPtr += (exponetIndex != -1) ? alnumCount : exponetIndex + 1;
	}

	if (*currPtr == '.')
		goto dot;
	else if (exponetIndex)
		goto exponet;
	else
		return 0;
dot : {
	currPtr++;
	__auto_type alnumCount = countAlnum(vec, currPtr - (char *)vec);
	for (int i = 0; i != alnumCount; i++)
		if (!isdigit(currPtr[i]))
			goto malformed;

	__auto_type slice = __vecAppendItem(NULL, currPtr, alnumCount);
	sscanf((char *)slice, "%lu", &f.frac);
	__vecDestroy(slice);

	currPtr += alnumCount;

	if (currPtr < endPtr)
		goto returnLabel;
	if (*currPtr == 'e' || *currPtr == 'E') {
		currPtr++;
		goto exponet;
	}
	goto returnLabel;
}
exponet : {
	int mult = 1;
	if (*currPtr == '-' || *currPtr == '+')
		mult = (*currPtr == '-' || *currPtr == '+') ? -1 : 1;

	__auto_type alnumCount = countAlnum(vec, currPtr - (char *)vec);
	for (int i = 0; i != alnumCount; i++)
		if (!isdigit(currPtr[i]))
			goto malformed;

	__auto_type slice = __vecAppendItem(NULL, currPtr, alnumCount);
	sscanf((char *)slice, "%d", &f.exponet);
	__vecDestroy(slice);
	f.exponet *= mult;

	currPtr += alnumCount;
	goto returnLabel;
}
returnLabel : {
	if (retVal != NULL)
		*retVal = f;
	if (end != NULL)
		*end = currPtr - (char *)vec;

	return 1;
}

malformed : {
	if (err != NULL)
		*err = 1;
}
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

	return retVal;
}
static struct __vec *stringLex(struct __vec *new, long pos, long *end,
                               const void *data, int *err) {
	struct parsedString find;
	if (stringParse(new, pos, end, &find, err))
		return __vecAppendItem(NULL, &find, sizeof(struct parsedString));
	return NULL;
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

	return retVal;
}
