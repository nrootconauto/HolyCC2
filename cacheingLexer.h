#include <linkedList.h>
#include <str.h>
#pragma once
enum lexerItemState { LEXER_MODIFED, LEXER_UNCHANGED, LEXER_DESTROY };
struct __lexerItem;
STR_TYPE_DEF(struct __lexerItem *, LexerItem);
STR_TYPE_FUNCS(struct __lexerItem *, LexerItem);
struct __lexerCacheBlob;
STR_TYPE_DEF(struct __lexerCacheBlob *, LexerCacheBlob);
STR_TYPE_FUNCS(struct __lexerCacheBlob *, LexerCacheBlob);
struct __lexerItem {
	struct __lexerItemTemplate *template;
	long start;
	long end;
	struct __lexerCacheBlob *blob;
	long itemIndex;
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

enum cacheBlobFlags {
	CACHE_FLAG_INSERT = 2,
	CACHE_FLAG_REMOVE = 4,
	CACHE_FLAG_INSERT_ADJ = 8,
	CACHE_FLAG_ALL_REMOVED =16,
};
enum cacheBlobRetCode {
 CACHE_BLOB_RET_DESTROY=0,
 CACHE_BLOB_RET_KEEP=1,
};
#define CACHE_FLAG_ALL (CACHE_FLAG_INSERT |CACHE_FLAG_REMOVE | CACHE_FLAG_INSERT_ADJ ) 
llLexerItem lexerGetItems(struct __lexer *lexer);
void *lexerItemValuePtr(const struct __lexerItem *item);
struct cacheBlobTemplate {
	void (*killData)(void *data);
	enum cacheBlobFlags mask;
	/**
	 * Return 1 to keep blob,otherwise blob will be destroyed
	 */
	enum cacheBlobRetCode (*update)(void *data, llLexerItem start, llLexerItem end,
	               llLexerItem *start2, llLexerItem *end2, enum cacheBlobFlags flags);
};
struct __lexerCacheBlob *
blobCreate(struct cacheBlobTemplate *template,
                     llLexerItem start, llLexerItem end, void *data);
void blobUpdateSpan(struct __lexerCacheBlob *blob) ;
struct __lexerCacheBlob {
	struct cacheBlobTemplate *template;
	void *data;
	llLexerItem start, end;
	int order;
	enum cacheBlobFlags flags;
	struct __lexerCacheBlob *parent;
};
