#include <linkedList.h>
#include <str.h>
#pragma once
enum lexerItemState { LEXER_MODIFED, LEXER_UNCHANGED, LEXER_DESTROY };
struct __lexerItem;
STR_TYPE_DEF(struct __lexerItem *, LexerItem);
STR_TYPE_FUNCS(struct __lexerItem *, LexerItem);
struct __lexerCacheBlob {
	strLexerItem items;
	long refCount;
	void *value;
};
STR_TYPE_DEF(struct __lexerCacheBlob *, LexerCacheBlob);
STR_TYPE_FUNCS(struct __lexerCacheBlob *, LexerCacheBlob);
struct __lexerItem {
	struct __lexerItemTemplate *template;
	long start;
	long end;
	strLexerCacheBlob blobs;
	// Data appended after here
};
LL_TYPE_DEF(struct __lexerItem, LexerItem);
LL_TYPE_FUNCS(struct __lexerItem, LexerItem);
struct __lexerItemTemplate {
	void *data;
	struct __vec *(*cloneData)(const void *);
	struct __vec *(*lexItem)(struct __vec *str, long pos, long *end,
	                         const void *data, int *err);
	enum lexerItemState (*validateOnModify)(const void *lexerItemData,
	                                        struct __vec *oldStr,
	                                        struct __vec *newStr, long newPos,
	                                        const void *data, int *err);
	struct __vec *(*update)(const void *lexerItemData, struct __vec *oldStr,
	                        struct __vec *newStr, long newPos, long *end,
	                        const void *data, int *err);
	int (*isAdjChar)(int chr);
	void (*killItemData)(struct __lexerItem *item);
	void (*killTemplateData)(void *item);
};

STR_TYPE_DEF(struct __lexerItemTemplate *, LexerItemTemplate);
STR_TYPE_FUNCS(struct __lexerItemTemplate *, LexerItemTemplate);

struct __lexer *lexerCreate(struct __vec *data, strLexerItemTemplate templates,
                            int (*charCmp)(const void *, const void *),
                            const void *(*whitespaceSkip)(struct __vec *source,
                                                          long pos));

void lexerDestroy(struct __lexer **lexer);

void lexerUpdate(struct __lexer *lexer, struct __vec *newData, int *err);

llLexerItem lexerGetItems(struct __lexer *lexer);
void *lexerItemValuePtr(const struct __lexerItem *item);
