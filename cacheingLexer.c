#include <assert.h>
#include <diff.h>
#include <linkedList.h>
#include <str.h>
#include <cacheingLexer.h>
struct __lexerItemTemplate;
void *lexerItemValuePtr(struct __lexerItem *item) {
	return (void *)item + sizeof(struct __lexerItem);
}
struct __lexer {
	struct __vec *oldSource;
	llLexerItem oldItems;
	strLexerItemTemplate templates;
	int (*charCmp)(const void *, const void *);
	const void *(*whitespaceSkip)(struct __vec *source, long pos);
};
llLexerItem lexerGetItems(struct __lexer *lexer) {
 return lexer->oldItems;
}
struct __lexer *lexerCreate(struct __vec *data, strLexerItemTemplate templates,
                            int (*charCmp)(const void *, const void *),
                            const void *(*whitespaceSkip)(struct __vec *source,
                                                          long pos)) {
	struct __lexer *retVal = malloc(sizeof(struct __lexer));

	retVal->oldSource = NULL;
	retVal->charCmp = charCmp;
	retVal->oldItems = NULL;
	retVal->whitespaceSkip = whitespaceSkip;
	retVal->templates =
	    (strLexerItemTemplate)__vecConcat(NULL, (struct __vec *)templates);
			
	lexerUpdate(retVal,data);
			
	return retVal;
}
void lexerDestroy(struct __lexer **lexer) {
	strLexerItemTemplateDestroy(&lexer[0]->templates);

	__auto_type node = __llGetFirst(lexer[0]->oldItems);
	while (node != NULL) {
		__auto_type value = llLexerItemValuePtr(node);
		if (value->template->killItemData != NULL)
			value->template->killItemData(lexerItemValuePtr(value));
		node = llLexerItemNext(node);
	}
	llLexerItemDestroy(&lexer[0]->oldItems);

	free(*lexer);
}
static int findAfterPosPred(void *a, void *b) {
	long *A = a;
	struct __lexerItem *B = b;
	return *A > B->start ? 1 : 0;
}
static llLexerItem findAfterPos(struct __lexer *lexer, long col,
                                llLexerItem startFrom) {
	long pos;
	pos = col;

	__auto_type find = llLexerItemFindRight(&startFrom, &pos, findAfterPosPred);
	if (find == NULL)
		return __llGetEnd(startFrom);
	else
		return find;
}
STR_TYPE_DEF(llLexerItem, LLLexerItem);
STR_TYPE_FUNCS(llLexerItem, LLLexerItem);
static struct __vec *lexerItemGetText(struct __lexer *lexer,
                                      struct __lexerItem *item) {
	return __vecAppendItem(NULL, (void *)lexer->oldSource + item->start,
	                       item->end - item->start);
}
static struct __diff *findNextSameDiff(const struct __lexer *lexer,
                                       long startAt, strDiff diffs, long *atNew,
                                       long *atOld) {
	long startAtDiff2 = 0;
	long pos = 0, posOld = 0;
	for (long i = startAtDiff2; i != strDiffSize(diffs); i++) {
		if (diffs[i].type == DIFF_SAME) {
			if (pos >= startAt) {
				if (atNew != NULL)
					*atNew = pos;
				if (atOld != NULL)
					*atOld = posOld;
				return &diffs[i];
			}

			pos += diffs[i].len;
			posOld += diffs[i].len;
			continue;
		} else if (diffs[i].type == DIFF_REMOVE) {
			posOld += diffs[i].len;
			continue;
		} else if (diffs[i].type == DIFF_INSERT) {
			pos += diffs[i].len;
			continue;
		} else
			assert(0);
	}
	return NULL;
};
static void __vecDestroyPtr(struct __vec **vec) { __vecDestroy(*vec); }
static llLexerItem getLexerCanidate(struct __lexer *lexer,
                                    struct __vec *newData, long pos) {
	llLexerItem retVal = NULL;

	__auto_type biggestSize = 0;
	for (int i = 0; i != strLexerItemTemplateSize(lexer->templates); i++) {
		long end;
		__auto_type res = lexer->templates[i]->lexItem(newData, pos, &end,
		                                              lexer->templates[i]->data);
		__auto_type resSize = __vecSize(res);
		if (res != NULL) {
			__auto_type size = end - pos;
			if (size >= biggestSize) {
				// Create a linked list node
				char buffer[sizeof(struct __lexerItem) + resSize];
				memcpy(buffer + sizeof(struct __lexerItem), res, resSize);
				llLexerItem item =
				    __llCreate(buffer, sizeof(struct __lexerItem) + resSize);
				__auto_type itemValue = llLexerItemValuePtr(retVal);
				itemValue->start = pos;
				itemValue->end = end;
				itemValue->template = lexer->templates[i];

				// Destroy previous(if present)
				if (retVal != NULL) {
					__auto_type prevItem = llLexerItemValuePtr(retVal);
					__auto_type killFoo = prevItem->template->killItemData;
					if (killFoo != NULL)
						killFoo(lexerItemValuePtr(prevItem));
				}

				retVal = item;
			}
			__vecDestroy(res);
		}
	}

	return retVal;
}
struct __lexerError *lexerUpdate(struct __lexer *lexer, struct __vec *newData) {
	strDiff diffs __attribute__((cleanup(strDiffDestroy)));
	diffs = NULL;

	llLexerItem currentItem = __llGetFirst(lexer->oldItems);
	long pos = 0;
	long foundAt = 0; // Dummy value will be updated ahead
	for (; foundAt != __vecSize(lexer->oldSource);) {
		__auto_type skipTo = lexer->whitespaceSkip(lexer->oldSource, pos);
		pos = (skipTo == NULL) ? __vecSize(lexer->oldSource)
		                       : (void *)skipTo - (void *)lexer->oldSource;

		long sameDiffEnd;
		__auto_type nextSame = findNextSameDiff(lexer, pos, diffs, &foundAt, NULL);
		// If no same-diff found,imagine a zero-length same diff at the end
		if (nextSame == NULL) {
			foundAt = __vecSize(lexer->oldSource);
			sameDiffEnd = foundAt;
		} else {
			sameDiffEnd = foundAt + nextSame->len;
		}

		for (; pos <= foundAt;) {
			__auto_type skipTo = lexer->whitespaceSkip(lexer->oldSource, pos);
			pos = (skipTo == NULL) ? __vecSize(lexer->oldSource)
			                       : (void *)skipTo - (void *)lexer->oldSource;

			if (!(pos <= foundAt))
				break;

			__auto_type lexerItemPtr = llLexerItemValuePtr(currentItem);

			// Previous items will have been updated before pos,so go to current item
			if (lexerItemPtr->start >= pos) {
				pos = lexerItemPtr->start;

				struct __vec *slice __attribute__((cleanup(__vecDestroyPtr)));
				slice = lexerItemGetText(lexer, lexerItemPtr);

				__auto_type template = lexerItemPtr->template;
				__auto_type state =
				    template->validateOnModify(lexerItemValuePtr(lexerItemPtr), slice,
				                               newData, foundAt, template->data);
				if (state == LEXER_MODIFED) {
				 long end;
				 
					__auto_type newValue=
					    template->update(lexerItemValuePtr(lexerItemPtr), slice, newData,
					                     foundAt, &end,template->data);
							//Delete old value
							template->killItemData(lexerItemValuePtr(lexerItemPtr));
							
							//Resize for new value
							__auto_type newSize=__vecSize(newValue);
							currentItem=__llValueResize(currentItem,newSize);
							memmove(lexerItemValuePtr(lexerItemPtr),newValue,newSize);
							__vecDestroy(newValue);
							
					// Update node size
					lexerItemPtr->end = pos;

					// Update pos
					pos = end;

					goto killConsumedItems;
				} else if (state == LEXER_DESTROY) {
					__auto_type nodeBefore = llLexerItemPrev(currentItem);

					// Destroy item
					llLexerItemRemove(currentItem);
					__auto_type lexerItem = llLexerItemValuePtr(currentItem);
					if (lexerItem->template->killItemData != NULL)
						lexerItem->template->killItemData(lexerItemValuePtr(lexerItem));

					// Search for canidate
					__auto_type res = getLexerCanidate(lexer, newData, pos);
					if (res == NULL) {
						// No canidate found,return error
						struct __lexerError *retVal = malloc(sizeof(struct __lexerError));
						retVal->pos = pos;
						retVal->lastItem = nodeBefore;
						return retVal;
					}

					llLexerItemInsertListAfter(nodeBefore, res);
					currentItem = res;
					goto killConsumedItems;
				} else if (state == LEXER_UNCHANGED) {
					pos = lexerItemPtr->end;
				}
				continue;
			killConsumedItems : {
				// Kill Nodes that exist witin new range
				long oldPos;
				findNextSameDiff(lexer, lexerItemPtr->end, diffs, NULL, &oldPos);
				for (__auto_type node = llLexerItemNext(currentItem);;) {
					__auto_type lexerItem = llLexerItemValuePtr(node);

					if (lexerItem->start < oldPos) {
						if (lexerItem->template->killItemData != NULL)
							lexerItem->template->killItemData(lexerItemValuePtr(lexerItem));

						node = __llRemoveNode(node);
						currentItem = node;

						continue;
					} else {
						break;
					}
				}
			}
			}
		}
	}

	return NULL;
}
