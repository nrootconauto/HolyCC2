#pragma once
#include <linkedList.h>
#include <str.h>
#include <stringParser.h>
struct lexer;
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
struct lexerItem;
struct lexerItemTemplate {
	struct __vec *(*lexItem)(const struct __vec *str, long pos, long *end, int *err);
	void (*killItemData)(void *item);
};
struct lexerItem {
	struct lexerItemTemplate *template;
	long start;
	long end;
	// Data appended after here
};
LL_TYPE_DEF(struct lexerItem, LexerItem);
LL_TYPE_FUNCS(struct lexerItem, LexerItem);
extern struct lexerItemTemplate intTemplate;
extern struct lexerItemTemplate strTemplate;
extern struct lexerItemTemplate floatTemplate;
extern struct lexerItemTemplate nameTemplate;
extern struct lexerItemTemplate opTemplate;
extern struct lexerItemTemplate kwTemplate;

llLexerItem lexText(const struct __vec *text, int *err) ;
void *lexerItemValuePtr(struct lexerItem *item);
