#pragma once
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
struct lexerString {
	struct __vec *text;
	int isChar : 1;
};
const void *skipWhitespace(struct __vec *text, long from);
struct __lexerItemTemplate floatingTemplateCreate();
struct __lexerItemTemplate intTemplateCreate();
struct __lexerItemTemplate keywordTemplateCreate(const char **keywords,
                                                 long keywordCount);
struct __lexerItemTemplate nameTemplateCreate(const char **keywords,
                                                 long keywordCount) ;
struct __lexerItemTemplate stringTemplateCreate();
