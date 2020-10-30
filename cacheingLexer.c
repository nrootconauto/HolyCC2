#include <assert.h>
#include <cacheingLexer.h>
#include <diff.h>
#include <linkedList.h>
#include <str.h>
struct __lexerItemTemplate;
void *lexerItemValuePtr(const struct __lexerItem *item) {
	return (void *)item + sizeof(struct __lexerItem);
}
struct __lexer {
	struct __vec *oldSource;
	llLexerItem oldItems;
	strLexerItemTemplate templates;
	int (*charCmp)(const void *, const void *);
	const void *(*whitespaceSkip)(struct __vec *source, long pos);
};
static void killLexerItemsNode(void *item) {
	struct __lexerItem *ptr = item;
	if (ptr->template->killItemData != NULL)
		ptr->template->killItemData(lexerItemValuePtr(ptr));
}
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

	int err;
	lexerUpdate(retVal, data, &err);

	if (err)
		return NULL;
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
	llLexerItemDestroy(&lexer[0]->oldItems, killLexerItemsNode);

	free(*lexer);
}
static int findAfterPosPred(const void *a, const void *b) {
	const long *A = a;
	const struct __lexerItem *B = b;
	return *A > B->start ? 1 : 0;
}
static llLexerItem findAfterPos(struct __lexer *lexer, long col,
                                llLexerItem startFrom) {
	long pos;
	pos = col;

	__auto_type find = llLexerItemFindRight(startFrom, &pos, findAfterPosPred);
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
			if (pos <= startAt && startAt <= pos + diffs[i].len) {
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
static struct __diff *findNextSameDiff(long startAt, const strDiff diffs,
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
                                    long *endPos, int *err) {
	llLexerItem retVal = NULL;

	__auto_type biggestSize = 0;
	for (int i = 0; i != strLexerItemTemplateSize(lexer->templates); i++) {
		long end;
		__auto_type res = lexer->templates[i]->lexItem(
		    newData, pos, &end, lexer->templates[i]->data, err);

		if (*err != 0) {
			if (retVal != NULL) {
				__auto_type item = llLexerItemValuePtr(retVal);
				if (item->template->killItemData != NULL)
					item->template->killItemData(lexerItemValuePtr(item));
			}
			return NULL;
		}

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
static llLexerItem findEndOfConsumedItems(const llLexerItem new,
                                          const llLexerItem current,
                                          const strDiff diffs) {
	if (new == NULL)
		return NULL;

	__auto_type item = llLexerItemValuePtr(new);
	// Kill Nodes that exist witin new range
	long oldPos;
	long newPos;
	findSameDiffContaining(item->end, diffs, &newPos, &oldPos);
	__auto_type offset = item->end - newPos;

	for (llLexerItem node = current; node != NULL;) {
		__auto_type lexerItem = llLexerItemValuePtr(node);

		if (lexerItem->start < oldPos + offset) {
			node = llLexerItemNext(node);
			continue;
		} else {
			return node;
		}
	}
	return NULL;
}
/**
 * Only move pastitems that are completely destroyed,if they partially exist in
 * either same or insert,they may be modifed!!!
 */
static llLexerItem lexerMovePastDeleted(long startAt, llLexerItem currentItem,
                                        const strDiff diffs) {

	__auto_type retVal = currentItem;

	__auto_type oldPos = 0;
	for (long i = 0; i != strDiffSize(diffs) && retVal != NULL; i++) {
		if (startAt <= oldPos)
			break;

		if (diffs[i].type == DIFF_REMOVE) {
			__auto_type oldPosEnd = oldPos + diffs[i].len;

			for (;;) {
				__auto_type lexerItem = llLexerItemValuePtr(retVal);
				if (lexerItem->start < oldPosEnd || lexerItem->end <= oldPosEnd) {
					__auto_type next = llLexerItemNext(retVal);
					if (next == NULL)
						goto returnLabel;
					retVal = next;
				} else if (lexerItem->end > oldPosEnd)
					break;
			}

			oldPos = oldPosEnd;
		} else if (diffs[i].type == DIFF_SAME) {
			oldPos += diffs[i].len;
		}
	}

returnLabel:
	return retVal;
}
llLexerItem lexerItemClone(const llLexerItem toClone) {
	__auto_type item = llLexerItemValuePtr(toClone);
	__auto_type clone = llLexerItemCreate(*item);

	if (item->template->cloneData != NULL) {
		__auto_type res = item->template->cloneData(lexerItemValuePtr(item));
		clone = __llValueResize(clone, sizeof(struct __lexerItem) + __vecSize(res));

		__auto_type valuePtr = lexerItemValuePtr(llLexerItemValuePtr(toClone));
		memcpy(valuePtr, res, __vecSize(res));

		__vecDestroy(res);
	} else {
		clone = __llValueResize(clone, __llItemSize(toClone));
		__auto_type valuePtrClone = lexerItemValuePtr(llLexerItemValuePtr(clone));
		__auto_type valuePtrFrom = lexerItemValuePtr(llLexerItemValuePtr(toClone));
		memcpy(valuePtrClone, valuePtrFrom,
		       __llItemSize(toClone) - sizeof(struct __lexerItem));
	}

	return clone;
}
void lexerUpdate(struct __lexer *lexer, struct __vec *newData, int *err) {
	strDiff diffs __attribute__((cleanup(strDiffDestroy)));
	diffs = __diff(lexer->oldSource, newData, __vecSize(lexer->oldSource),
	               __vecSize(newData), 1,
	               lexer->charCmp); // TODO update item size

	llLexerItem currentItem = __llGetFirst(lexer->oldItems);
	llLexerItem retVal = NULL;
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
		    findNextSameDiff(newPos, diffs, &diffNewPos, &diffOldPos);
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

			currentItem = lexerMovePastDeleted(
			    newPos, currentItem, diffs); // Move past any deletions moved past

			/**
			 * The for loop looks for changed/unmodifed/deleted items,if newPos is
			 * past current diff,look for next same diff
			 */
			if (newPos >= sameDiffEnd)
				break;

			if (newPos == __vecSize(newData))
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
				                               newData, newPos, template->data, err);
				if (err != NULL)
					if (*err)
						goto error;

				if (state == LEXER_MODIFED) {
					long end;

					//
					// Make a new node containing the new value
					//
					__auto_type newValue =
					    template->update(lexerItemValuePtr(lexerItemPtr), slice, newData,
					                     newPos, &end, template->data, err);
					if (err != NULL)
						if (*err)
							goto error;
					// Clone the properites of the current item
					__auto_type newNode =
					    llLexerItemCreate(*llLexerItemValuePtr(currentItem));
					// Make room for the new data
					newNode = __llValueResize(newNode, sizeof(struct __lexerItem) +
					                                       __vecSize(newValue));
					lexerItemPtr = __llValuePtr(newNode);
					// Move in the new data
					memmove(lexerItemValuePtr(lexerItemPtr), newValue,
					        __vecSize(newValue));
					__vecDestroy(newValue);

					// Update node size
					lexerItemPtr->start = newPos;
					lexerItemPtr->end = end;

					// Update pos
					newPos = end;

					currentItem = findEndOfConsumedItems(
					    retVal, llLexerItemNext(currentItem),
					    diffs); // killConsumedItems returns next (availible) item

					llInsertListAfter(retVal, newNode);
					retVal = newNode;
					continue;
				} else if (state == LEXER_DESTROY) {
					currentItem = llLexerItemNext(currentItem);

					goto findNewItems;
				} else if (state == LEXER_UNCHANGED) {
					__auto_type clone = lexerItemClone(currentItem);
					__auto_type cloneItemPtr = llLexerItemValuePtr(clone);
					newPos = cloneItemPtr->end - diffOldPos + diffNewPos;
					cloneItemPtr->end = newPos;
					cloneItemPtr->start = cloneItemPtr->start - diffOldPos + diffNewPos;
					llLexerItemInsertListAfter(retVal, clone);
					retVal = clone;

					currentItem = llLexerItemNext(currentItem);
					continue;
				}
			}
		findNewItems : {
			do {
				newPos = lexer->whitespaceSkip(newData, newPos) - (void *)newData;
				long end;
				__auto_type res = getLexerCanidate(lexer, newData, newPos, &end, err);
				if (err != NULL)
					if (*err)
						goto error;
				newPos = end;
				assert(NULL != res);
				llLexerItemInsertListAfter(retVal, (llLexerItem)res);
				retVal = res;

				newPos = end;

				currentItem = findEndOfConsumedItems(
				    retVal, currentItem,
				    diffs); // killConsumedItems returns next (availible) item
			} while (newPos < diffNewPos);
		}
		}
	}

	lexer->oldSource = __vecAppendItem(NULL, newData, __vecSize(newData));
	lexer->oldItems = __llGetFirst(retVal);
	return;
error : { llLexerItemDestroy(&retVal, killLexerItemsNode); }
}
