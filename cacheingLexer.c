#include <assert.h>
#include <cacheingLexer.h>
#include <diff.h>
#include <linkedList.h>
#include <str.h>
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
llLexerItem lexerGetItems(struct __lexer *lexer) { return lexer->oldItems; }
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

	lexerUpdate(retVal, data);

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
static struct __diff *findSameDiffContaining(long startAt, const strDiff diffs,
                                             long *atNew, long *atOld) {
	long startAtDiff2 = 0;
	long pos = 0, posOld = 0;
	for (long i = startAtDiff2; i != strDiffSize(diffs); i++) {
		if (diffs[i].type == DIFF_SAME) {
			if (pos <= startAt && startAt < pos + diffs[i].len) {
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
}
static struct __diff *findNextSameDiff(const struct __lexer *lexer,
                                       long startAt, const strDiff diffs,
                                       long *atNew, long *atOld) {
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
                                    struct __vec *newData, long pos,
                                    long *endPos) {
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
				__auto_type itemValue = llLexerItemValuePtr(item);
				itemValue->start = pos;
				itemValue->end = end;
				itemValue->template = lexer->templates[i];

				biggestSize = size;

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

	if (endPos != NULL)
		*endPos = pos + biggestSize;
	return retVal;
}
static llLexerItem killConsumedItems(struct __lexer *lexer, llLexerItem current,
                                     const strDiff diffs) {
	for (;;) {
		__auto_type item = llLexerItemValuePtr(current);
		// Kill Nodes that exist witin new range
		long oldPos;
		long newPos;
		findSameDiffContaining(item->end, diffs, &newPos, &oldPos);
		__auto_type offset = item->end - newPos;

		for (__auto_type node = llLexerItemNext(current);;) {
			__auto_type lexerItem = llLexerItemValuePtr(node);

			if (lexerItem->start < oldPos + offset) {
				if (lexerItem->template->killItemData != NULL)
					lexerItem->template->killItemData(lexerItemValuePtr(lexerItem));

				current = llLexerItemNext(node);
				node = __llRemoveNode(node);

				continue;
			} else {
				return node;
			}
		}
	}
}
struct __lexerError *lexerUpdate(struct __lexer *lexer, struct __vec *newData) {
	strDiff diffs __attribute__((cleanup(strDiffDestroy)));
	diffs = __diff(lexer->oldSource, newData, __vecSize(lexer->oldSource),
	               __vecSize(newData), 1,
	               lexer->charCmp); // TODO update item size

	llLexerItem currentItem = __llGetFirst(lexer->oldItems);
	long newPos = 0;
	long diffOldPos = 0;
	long diffNewPos = 0; // Dummy value will be updated ahead
	for (int firstRun = 1; !(newPos == __vecSize(newData) && firstRun == 0);
	     firstRun = 0) {
		__auto_type skipTo = lexer->whitespaceSkip(newData, newPos);
		newPos = (skipTo == NULL) ? __vecSize(newData)
		                          : (void *)skipTo - (void *)newData;

		long sameDiffEnd;
		__auto_type nextSame =
		    findNextSameDiff(lexer, newPos, diffs, &diffNewPos, &diffOldPos);
		// If no same-diff found,imagine a zero-length same diff at the end
		if (nextSame == NULL) {
			diffNewPos = __vecSize(newData);
			sameDiffEnd = diffNewPos;
		} else {
			sameDiffEnd = diffNewPos + nextSame->len;
		}

		for (;;) {
			__auto_type skipTo = lexer->whitespaceSkip(newData, newPos);
			newPos = (skipTo == NULL) ? __vecSize(newData)
			                          : (void *)skipTo - (void *)newData;

			if (newPos >= sameDiffEnd)
				break;

			if (currentItem == NULL)
				goto findNewItems;
			__auto_type lexerItemPtr = llLexerItemValuePtr(currentItem);

			// Previous items will have been updated before pos,so go to current item
			if (lexerItemPtr->start == diffOldPos + newPos - diffNewPos) {
				struct __vec *slice __attribute__((cleanup(__vecDestroyPtr)));
				slice = lexerItemGetText(lexer, lexerItemPtr);

				__auto_type template = lexerItemPtr->template;
				__auto_type state =
				    template->validateOnModify(lexerItemValuePtr(lexerItemPtr), slice,
				                               newData, newPos, template->data);
				if (state == LEXER_MODIFED) {
					long end;

					__auto_type newValue =
					    template->update(lexerItemValuePtr(lexerItemPtr), slice, newData,
					                     newPos, &end, template->data);
					// Delete old value
					if (template->killItemData != NULL)
						template->killItemData(lexerItemValuePtr(lexerItemPtr));

					// Resize for new value
					__auto_type newSize = __vecSize(newValue);
					currentItem = __llValueResize(currentItem,
					                              sizeof(struct __lexerItem) + newSize);
					lexerItemPtr = __llValuePtr(currentItem);
					memmove(lexerItemValuePtr(lexerItemPtr), newValue, newSize);
					__vecDestroy(newValue);

					// Update node size
					lexerItemPtr->start = newPos;
					lexerItemPtr->end = end;

					// Update pos
					newPos = end;

					__auto_type oldCurrentItem = currentItem;
					currentItem = killConsumedItems(
					    lexer, currentItem,
					    diffs); // killConsumedItems returns next (availible) item
					currentItem = (currentItem == NULL) ? oldCurrentItem : currentItem;
					continue;
				} else if (state == LEXER_DESTROY) {
					__auto_type nodeBefore = llLexerItemPrev(currentItem);

					// Destroy item
					__auto_type newCurrentItem = (llLexerItemNext(currentItem) == NULL)
					                                 ? llLexerItemPrev(currentItem)
					                                 : llLexerItemNext(currentItem);
					llLexerItemRemove(currentItem);
					currentItem = newCurrentItem;

					__auto_type lexerItem = llLexerItemValuePtr(currentItem);
					if (lexerItem->template->killItemData != NULL)
						lexerItem->template->killItemData(lexerItemValuePtr(lexerItem));

					goto findNewItems;
				} else if (state == LEXER_UNCHANGED) {
					newPos = lexerItemPtr->end - diffOldPos + diffNewPos;
					lexerItemPtr->end = newPos;
					lexerItemPtr->start = lexerItemPtr->start - diffOldPos + diffNewPos;

					if (llLexerItemNext(currentItem) != NULL)
						currentItem = llLexerItemNext(currentItem);
					continue;
				}
			}
		findNewItems : {
			do {
				newPos = lexer->whitespaceSkip(newData, newPos) - (void *)newData;
				long end;
				__auto_type res = getLexerCanidate(lexer, newData, newPos, &end);
				newPos = end;
				assert(NULL != res);
				llLexerItemInsertListAfter(currentItem, (llLexerItem)res);
				currentItem = res;

				newPos = end;
				if (newPos > diffNewPos) {
					killConsumedItems(lexer, currentItem, diffs);
					break;
				}
			} while (newPos < diffNewPos);
		}
		}
	}

	lexer->oldSource = __vecAppendItem(NULL, newData, __vecSize(newData));
	lexer->oldItems = __llGetFirst(currentItem);
	return NULL;
}
