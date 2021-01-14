#pragma once
#include <linkedList.h>
#include <stdint.h>
#include <str.h>
#include <stringParser.h>
struct lexer;
enum intType {
	INT_ULONG,
	INT_SLONG,
};
struct lexerInt {
	enum intType type;
	int base;
	union {
		int32_t sInt;
		uint32_t uInt;
		int64_t sLong;
		uint64_t uLong;
	} value;
};
struct lexerFloating {
		double value;
};
struct lexerItem;
struct lexerItemTemplate {
	struct __vec *(*lexItem)(const struct __vec *str, long pos, long *end,
	                         int *err);
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

llLexerItem lexText(const struct __vec *text, int *err);
void *lexerItemValuePtr(struct lexerItem *item);
void initTemplates();
