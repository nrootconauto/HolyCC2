#include <linkedList.h>
#pragma once
enum lexerItemState { LEXER_MODIFED, LEXER_UNCHANGED, LEXER_DESTROY };
LL_TYPE_DEF(struct __lexerItem, LexerItem);
LL_TYPE_FUNCS(struct __lexerItem, LexerItem);

struct __lexerItemTemplate {
	void *data;
	struct __vec *(*lexItem)(struct __vec *str, long pos, long *end,
	                         const void *data);
	enum lexerItemState (*validateOnModify)(const void *lexerItemData,
	                                        struct __vec *oldStr,
	                                        struct __vec *newStr, long newPos,
	                                        const void *data);
	/**
	 * Returns new size
	 */
	long (*update)(void * lexerItemData, struct __vec *oldStr,
	               struct __vec *newStr, long newPos, const void *data); 
	void (*killItemData)(struct __lexerItem *item);
};

struct __lexerError {
	int pos;
	llLexerItem lastItem;
};

struct __lexer *lexerCreate(struct __vec *data, strLexerItemTemplate templates,
                            int (*charCmp)(const void *, const void *),
                            const void *(*whitespaceSkip)(struct __vec *source,
                                                          long pos));

void lexerDestroy(struct __lexer **lexer);

struct __lexerError *lexerUpdate(struct __lexer *lexer, struct __vec *newData);
