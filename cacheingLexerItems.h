#pragma once
#include <stringParser.h>
enum intType {
	INT_UINT,
	INT_SINT,
	INT_ULONG,
	INT_SLONG,
};
struct lexerInt {
	enum intType type;
	int base;
	union {
		signed int sInt;
		unsigned int uInt;
		signed long sLong;
		unsigned long uLong;
	} value;
};
struct lexerFloating {
	unsigned long base;
	unsigned long frac;
	int exponet;
};
const void *skipWhitespace(struct __vec *text, long from);
void *findNextLine(struct __vec *text, long pos);
void *findEndOfLine(struct __vec *text, long pos);
struct __lexerItemTemplate keywordTemplateCreate(const char **keywords,
                                                 long keywordCount);
struct __lexerItemTemplate nameTemplateCreate(const char **keywords,
                                              long keywordCount);
struct __lexerItemTemplate stringTemplateCreate();
struct __lexerItemTemplate intTemplateCreate();
struct __lexerItemTemplate floatingTemplateCreate();
