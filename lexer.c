#include <ctype.h>
#include <lexer.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
struct lexer {
	llLexerItem currentItems;
};
void *lexerItemValuePtr(struct lexerItem *item) {
	return (void *)item + sizeof(struct lexerItem);
}
static const char *keywords[] = {"if", "else",
                                 //
                                 "for", "while", "do", "break",
                                 //
                                 "goto",
                                 //
                                 "return",
                                 //
                                 "switch", "default", "case",
                                 //
                                 "class", "union",
                                 //
                                 "static", "public", "extern", "_extern",
                                 "import", "_import", "public",
                                 //
                                 ";", "{", "}", ":", "...",
																																	//
																																	"asm","@@","DU8","DU16","DU32","DU64","IMPORT","ALIGN","ORG","BINFILE"
};
const char *operators[] = {
    "++",
    "--",
    //
    "(",
    ")",
    "[",
    "]",
    //
    ".",
    "->",
    //
    "!",
    "~",
    //
    "*",
    "&",
    //
    "/",
    "%",
    //
    "+",
    "-",
    //
    "<<",
    ">>",
    //
    "<",
    ">",
    //
    ">=",
    "<=",
    //
    "==",
    "!=",
    //
    "^",
    "|",
    //
    "&&",
    "^^",
    "||",
    //
    "=",
    "+=",
    "-=",
    "*=",
    "/=",
    "%=",
    "<<=",
    ">>=",
    "&=",
    "^=",
    "|=",
    //
    ",",
};
static void sortKeywords();
static int kwSortPred(const void *a, const void *b) {
	return strcmp(*(const char **)a, *(const char **)b);
}
static void sortKeywords() {
	__auto_type count = sizeof(keywords) / sizeof(*keywords);
	qsort(keywords, count, sizeof(*keywords), kwSortPred);
}
static long countAlnum(const struct __vec *data, long pos) {
	long alNumCount = 0;
	for (void *ptr = (void *)data + pos; ptr < __vecSize(data) + (void *)data;
	     ptr++, alNumCount++)
		if (!(isalnum(*(char *)ptr) || *(char *)ptr == '_'))
			break;

	return alNumCount;
}
static int isKeyword(const struct __vec *new, long pos) {
	__auto_type len = countAlnum(new, pos);
	__auto_type count = sizeof(keywords) / sizeof(*keywords);

	for (long i = 0; i != count; i++) {
		__auto_type res =
		    strncmp((char *)new + pos, keywords[i], strlen(keywords[i]));
		if (res > 0)
			continue;
		else if (res == 0 && len == strlen(keywords[i]))
			return 1;
		else
			return 0;
	}
	return 0;
}
STR_TYPE_DEF(char *, Str);
STR_TYPE_FUNCS(char *, Str);
const void *skipWhitespace(const struct __vec *text, long from) {
	for (__auto_type ptr = (void *)text + from;
	     ptr != (void *)text + __vecSize(text); ptr++)
		if (!isblank(*(char *)ptr) && *(char *)ptr != '\n' &&
		    *(char *)ptr != '\r' && ptr != '\0')
			return ptr;
	return __vecSize(text) + (void *)text;
}
static struct __vec *intLex(const struct __vec *new, long pos, long *end,
                            int *err) {
	if (err != NULL)
		*err = 0;

	uint64_t valueU = 0;
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
	} else if (isdigit(pos[(char *)new])) {
		__auto_type alnumCount = countAlnum(new, pos);

		// Ensure are decimal digits
		for (int i = 0; i != alnumCount; i++)
			if (!isdigit(New[i]))
				goto malformed;

		__auto_type slice = __vecAppendItem(NULL, New, alnumCount + 1);
		((char *)slice)[alnumCount] = '\0';

		sscanf((char *)slice, "%lu", &valueU);

		New += alnumCount;
		goto dumpU;
	}
	return NULL;
dumpU : {
	__auto_type end2 = New - (char *)new;
	if (end != NULL)
		*end = end2;

	// Check to ensure isn't a float
	if (end2 + (char *)new < endPtr)
		if (((char *)new)[end2] == '.')
			return 0;

	struct lexerInt retVal;

	retVal.base = base;
	retVal.type = (UINT64_MAX <= valueU) ? INT_SLONG : INT_SINT;
	if (INT64_MAX < valueU)
		retVal.value.sLong = valueU;
	else
		retVal.value.sInt = valueU;

	return __vecAppendItem(NULL, &retVal, sizeof(retVal));
}
malformed : {
	*err = 1;
	return NULL;
}
}
static struct __vec *floatingLex(const struct __vec *vec, long pos, long *end,
                                 int *err) {
	if (err != NULL)
		*err = 0;

	__auto_type endPtr = strlen(vec) + (char *)vec;
	__auto_type currPtr = (char *)vec + pos;
	struct lexerFloating f;
	f.base = 0;
	f.frac = 0;
	f.zerosBeforeBase = 0;
	f.exponet = 0;

	__auto_type alnumCount = countAlnum(vec, pos);
	if (alnumCount == 0)
		return NULL;
	if (!isdigit(*((char *)vec + pos)))
		return NULL;

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
		    ((exponetIndex != -1) ? exponetIndex - pos : alnumCount));
		sscanf((char *)slice, "%lu", &f.base);

		currPtr += (exponetIndex == -1) ? alnumCount : exponetIndex + 1 - pos;
	}

	if (*currPtr == '.')
		goto dot;
	else if (exponetIndex != -1)
		goto exponet;
	else
		return NULL;
dot : {
	currPtr++;
	while (*currPtr == '0')
		currPtr++, f.zerosBeforeBase++;

	long digitCount = 0;
	for (; isdigit(currPtr[digitCount]); digitCount++)
		;

	__auto_type slice = __vecAppendItem(NULL, currPtr, digitCount);
	slice = __vecAppendItem(slice, "\0", 1);
	sscanf((char *)slice, "%lu", &f.frac);

	currPtr += digitCount;

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
	slice = __vecAppendItem(slice, "\0", 1);
	sscanf((char *)slice, "%d", &f.exponet);
	f.exponet *= mult;

	currPtr += alnumCount;
	goto returnLabel;
}
returnLabel : {
	if (end != NULL)
		*end = currPtr - (char *)vec;

	return __vecAppendItem(NULL, &f, sizeof(f));
}

malformed : {
	if (err != NULL)
		*err = 1;
}
	return 0;
}
static struct __vec *nameLex(const struct __vec *new, long pos, long *end,
                             int *err) {
	if (err != NULL)
		*err = 0;

	__auto_type alNumCount = countAlnum(new, pos);
	if (alNumCount == 0)
		return NULL;

	if (isdigit(((char *)new)[pos]))
		return NULL;

	if (isKeyword(new, pos))
		return NULL;

	*end = pos + alNumCount;

	int len = alNumCount;
	char z = 0;
	__auto_type retVal = __vecAppendItem(NULL, (char *)new + pos, len);
	return __vecAppendItem(retVal, &z, 1);
}
static struct __vec *stringLex(const struct __vec *new, long pos, long *end,
                               int *err) {
	struct parsedString find;
	if (stringParse(new, pos, end, &find, err))
		return __vecAppendItem(NULL, &find, sizeof(struct parsedString));
	return NULL;
}
static struct __vec *__keywordLex(const char **keywords, long count,
                                  const struct __vec *new, long pos, long *end,
                                  int *err) {
	if (err != NULL)
		*err = 0;

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
			} else {
				*end = pos + strlen(keywords[i]);

				return __vecAppendItem(NULL, &keywords[i], sizeof(*keywords));
			}
		}
	}
	if (alnumCount == 0 && largestIndex != -1)
		return __vecAppendItem(NULL, &keywords[largestIndex], sizeof(*keywords));

	return NULL;
}
static struct __vec *keywordLex(const struct __vec *new, long pos, long *end,
                                int *err) {
	__auto_type count = sizeof(keywords) / sizeof(*keywords);
	return __keywordLex(keywords, count, new, pos, end, err);
}
static struct __vec *operatorLex(const struct __vec *new, long pos, long *end,
                                 int *err) {
	__auto_type count = sizeof(operators) / sizeof(*operators);
	return __keywordLex(operators, count, new, pos, end, err);
}

struct lexerItemTemplate intTemplate;
struct lexerItemTemplate strTemplate;
struct lexerItemTemplate floatTemplate;
struct lexerItemTemplate nameTemplate;
struct lexerItemTemplate opTemplate;
struct lexerItemTemplate kwTemplate;
static struct lexerItemTemplate *templates[] = {
    &intTemplate,  &strTemplate, &floatTemplate,
    &nameTemplate, &opTemplate,  &kwTemplate,
};
void initTemplates();
void initTemplates() {
	sortKeywords();
	intTemplate.killItemData = NULL;
	intTemplate.lexItem = intLex;

	strTemplate.lexItem = stringLex;
	strTemplate.killItemData = NULL;

	floatTemplate.killItemData = NULL;
	floatTemplate.lexItem = floatingLex;

	kwTemplate.lexItem = keywordLex;
	kwTemplate.killItemData = NULL;

	opTemplate.lexItem = operatorLex;
	opTemplate.killItemData = NULL;

	nameTemplate.lexItem = nameLex;
	nameTemplate.killItemData = NULL;
}
static void killLexerData(void *item) {
	struct lexerItem *item2 = item;
	if (item2->template->killItemData) {
		item2->template->killItemData(lexerItemValuePtr(item2));
	}
}
void llLexerItemDestroy2(llLexerItem *item) {
	llLexerItemDestroy(item, killLexerData);
}
llLexerItem lexText(const struct __vec *text, int *err) {
	llLexerItem retVal = NULL;
	int err2;

	__auto_type len = strlen((char *)text);
	long pos = 0;
	for (;;) {
		pos = (char *)skipWhitespace(text, pos) - (char *)text;
		if (pos >= len)
			break;

		__auto_type tCount = sizeof(templates) / sizeof(*templates);

		long maximumEnd = pos;
		struct lexerItemTemplate *itemTemplate = NULL;
		struct __vec *value = NULL;
		for (long i = 0; i != tCount; i++) {
			long end;
			__auto_type find = templates[i]->lexItem(text, pos, &end, &err2);
			if (err2)
				goto fail;

			if (find != NULL) {
				if (maximumEnd > end) {
					if (templates[i]->killItemData)
						templates[i]->killItemData((void *)find);
				} else {
					itemTemplate = templates[i];
					maximumEnd = end;
					value = find;
				}
			}
		}

		if (maximumEnd == pos)
			goto fail;

		if (value != NULL) {
			struct lexerItem newItem;
			newItem.start = pos;
			newItem.end = maximumEnd;
			newItem.template = itemTemplate;

			char buffer[sizeof(newItem) + __vecSize(value)];
			*(struct lexerItem *)buffer = newItem;
			memcpy(buffer + sizeof(newItem), value, __vecSize(value));

			__auto_type newNode =
			    __llCreate(buffer, sizeof(newItem) + __vecSize(value));
			llLexerItemInsertListAfter(retVal, newNode);
			retVal = newNode;

			pos = maximumEnd;
		} else
			goto fail;
	}

	if (err != NULL)
		*err = 0;
	return llLexerItemFirst(retVal);
fail : {
	llLexerItemDestroy2(&retVal);
	if (err != NULL)
		*err = 1;

	return NULL;
}
}
