#include <assert.h>
#include <cacheingLexer.h>
#include <diff.h>
#include <linkedList.h>
#include <str.h>
static struct __lexerItemTemplate cloneTemplate;
static void initCloneTempate() __attribute__((constructor));
static void initCloneTempate() {
	cloneTemplate.cloneData = NULL;
	cloneTemplate.data = NULL;
	cloneTemplate.isAdjChar = NULL;
	cloneTemplate.killItemData = NULL;
	cloneTemplate.killTemplateData = NULL;
	cloneTemplate.lexItem = NULL;
	cloneTemplate.update = NULL;
	cloneTemplate.validateOnModify = NULL;
}
static int ptrPtrCmp(const void *a, const void *b) {
	if (*(void **)a > *(void **)b) {
		return 1;
	} else if (*(void **)a == *(void **)b) {
		return 0;
	} else {
		return -1;
	}
}
struct cacheLexerUpdate {
	strLexerCacheBlob affected;
};
static int longCmp(long a, long b) {
	if (a > b)
		return 1;
	else if (a == b)
		return 0;
	else
		return -1;
}
LL_TYPE_DEF(void *, Ptr);
LL_TYPE_FUNCS(void *, Ptr);
static __thread llPtr toUpdate = NULL;
static void toUpdateDestroy() __attribute__((destructor));
static void toUpdateDestroy() {
	llPtrDestroy(&toUpdate, NULL);
	toUpdate = NULL;
}
struct __lexerCacheBlob *blobCreate(struct cacheBlobTemplate *template,
                                    llLexerItem start, llLexerItem end,
                                    void *data) {
	struct __lexerCacheBlob *retVal = malloc(sizeof(struct __lexerCacheBlob));
	retVal->data = data;
	retVal->start = start;
	retVal->end = end;
	retVal->template = template;
	retVal->flags = 0;

	for (__auto_type node = start; node != end; node = llLexerItemNext(node)) {
		__auto_type item = llLexerItemValuePtr(node);
		if (item->blob == NULL) {
			item->blob = retVal;
		} else {
			__auto_type oldBlob = item->blob;
			for (__auto_type par = oldBlob->parent; par != NULL; par = par->parent)
				oldBlob = par;

			oldBlob->parent = retVal;
		}
	}

	return retVal;
}
static int lexerCacheBlobUpdateInsert(struct __lexerCacheBlob *blob,
                                      llLexerItem inserted) {
	__auto_type old = blob->flags;

	if (inserted == llLexerItemPrev(blob->start))
		goto adj;
	else if (inserted == llLexerItemNext(blob->end))
		goto adj;
	else {
		blob->flags |= blob->template->mask & CACHE_FLAG_INSERT;
	}
	goto end;
adj : { blob->flags |= blob->template->mask & CACHE_FLAG_INSERT_ADJ; }
end : {
	if (old != blob->flags) {
		if (NULL == llPtrFindLeft(llPtrFirst(toUpdate), &blob, ptrPtrCmp)) {
			toUpdate = llPtrInsert(toUpdate, llPtrCreate(blob), ptrPtrCmp);
		}
	}
	return old != blob->flags;
}
}
static int lexerCacheBlobUpdateRemove(struct __lexerCacheBlob *blob,
                                      llLexerItem toRemove) {
	__auto_type old = blob->flags;

	if (blob->end == toRemove)
		blob->end = llLexerItemPrev(blob->end);
	if (blob->end == blob->start)
		goto destroy;

	if (blob->start == toRemove)
		blob->start = llLexerItemNext(blob->start);
	if (blob->end == blob->start)
		goto destroy;
	blob->flags |= blob->template->mask & CACHE_FLAG_REMOVE;
	goto end;
destroy : {
	blob->flags |=
	    blob->template->mask & (CACHE_FLAG_REMOVE | CACHE_FLAG_ALL_REMOVED);
	goto end;
}
end:
	if (old != blob->flags) {
		if (NULL == llPtrFindLeft(llPtrFirst(toUpdate), &blob, ptrPtrCmp)) {
			toUpdate = llPtrInsert(toUpdate, llPtrCreate(blob), ptrPtrCmp);
			return 1;
		}
	}
	return 0;
}
static void llLexerItemKillSlice(llLexerItem start, llLexerItem end) {
	// Kill consumed nodes
	for (llLexerItem node2 = start; node2 != end;
	     node2 = llLexerItemNext(node2)) {
		__auto_type item = llLexerItemValuePtr(node2);

		if (item->blob != NULL)
			lexerCacheBlobUpdateRemove(item->blob, node2);

		__auto_type killFunc = item->template->killItemData;
		if (killFunc) {
			killFunc(lexerItemValuePtr(item));
		}
	}
}
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
				itemValue->blob = NULL;

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
	__auto_type originalNode = current;
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
			// Kill consumed nodes
			llLexerItemKillSlice(originalNode, node);

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
	llLexerItemKillSlice(currentItem, retVal);

	return retVal;
}
static void __cloneNodeReplace(llLexerItem cloneNode) {
	llLexerItem toClone =
	    *(llLexerItem *)lexerItemValuePtr(llLexerItemValuePtr(cloneNode));

	__auto_type toCloneItem = llLexerItemValuePtr(toClone);
	if (toCloneItem->template->cloneData != NULL) {

		__auto_type res = 
		    toCloneItem->template->cloneData(lexerItemValuePtr(toCloneItem));
		cloneNode =
		    __llValueResize(cloneNode, sizeof(struct __lexerItem) + __vecSize(res));

		__auto_type valuePtr = lexerItemValuePtr(llLexerItemValuePtr(toClone));
		memcpy(valuePtr, res, __vecSize(res));

		__vecDestroy(res);
	} else {
		cloneNode = __llValueResize(cloneNode, __llItemSize(toClone));
		__auto_type valuePtrClone =
		    lexerItemValuePtr(llLexerItemValuePtr(cloneNode));
		__auto_type valuePtrFrom = lexerItemValuePtr(llLexerItemValuePtr(toClone));
		memcpy(valuePtrClone, valuePtrFrom,
		       __llItemSize(toClone) - sizeof(struct __lexerItem));
	}
	__auto_type clonedItem=llLexerItemValuePtr(cloneNode);
	clonedItem->template=toCloneItem->template;

	/**
	 * Map blobs' old start/end ptrs to current node (if start/end points to old
	 * element pointed to by this node)
	 */
	__auto_type blob = llLexerItemValuePtr(toClone)->blob;
	for (__auto_type b = blob; b != NULL; b = b->parent) {
		if (toClone == b->start)
			b->start = toClone;
		if (toClone == b->end)
			b->end = toClone;
	}
}
static llLexerItem createCloneNode(const llLexerItem toClone) {
	__auto_type item = llLexerItemValuePtr(toClone);

	struct __lexerItem newItem;
	newItem.blob = item->blob;
	newItem.itemIndex = newItem.end = newItem.start = -1;
	newItem.template = &cloneTemplate;

	char buffer[sizeof(newItem) + sizeof(toClone)];
	memcpy(buffer, &newItem, sizeof(newItem));
	memcpy(buffer + sizeof(newItem), &toClone, sizeof(toClone));
	return __llCreate(buffer, sizeof(newItem) + sizeof(toClone));
}
static void blobDetach(struct __lexerCacheBlob *blob) {
	for (__auto_type node = blob->start; node != blob->end;
	     node = llLexerItemNext(node)) {
		// Remove from parents of node
		struct __lexerCacheBlob *oldBlob = NULL;
		for (__auto_type curBlob = llLexerItemValuePtr(node)->blob; curBlob != NULL;
		     curBlob = curBlob->parent) {
			if (curBlob == blob) {
				if (oldBlob != NULL)
					oldBlob->parent = curBlob->parent;
				else
					llLexerItemValuePtr(node)->blob = curBlob->parent;
			}
			oldBlob = curBlob;
		}
	}
}
void blobUpdateSpan(struct __lexerCacheBlob *blob) {
	blobDetach(blob);

	__auto_type parent = blob->parent;
	for (__auto_type node = blob->start; node != blob->end;
	     node = llLexerItemNext(node)) {
		/**
		 * Insert blob before parent
		 */
		struct __lexerCacheBlob *oldBlob = NULL;
		for (__auto_type curBlob = llLexerItemValuePtr(node)->blob; curBlob != NULL;
		     curBlob = curBlob->parent) {
			if (curBlob == parent) {
				if (oldBlob == NULL) {
					llLexerItemValuePtr(node)->blob = blob;
				} else {
					oldBlob->parent = blob;
				}
			}
		}
	}
	/**
	 * Blocks are in a hiearchy,so if child blob's end/start goes past boundary of
	 * parent,move boundary
	 */
	if (parent != NULL) {
		long endI1;
		if (llLexerItemValuePtr(blob->end))
			endI1 = llLexerItemValuePtr(blob->end)->itemIndex;
		else
			endI1 = 1 + llLexerItemValuePtr(llLexerItemLast(blob->start))->itemIndex;
		long startI1 = 0;
		startI1 = llLexerItemValuePtr(blob->start)->itemIndex;

		long endI2;
		if (llLexerItemValuePtr(parent->end))
			endI2 = llLexerItemValuePtr(parent->end)->itemIndex;
		else
			endI2 =
			    1 + llLexerItemValuePtr(llLexerItemLast(parent->start))->itemIndex;
		long startI2 = 0;
		startI2 = llLexerItemValuePtr(parent->start)->itemIndex;

		int changed = 0;
		if (startI1 < startI2) {
			changed = 1;
			for (long i = 0; i != startI2 - startI1; i++)
				parent->start = llLexerItemPrev(parent->start);
		}
		if (endI1 > endI2) {
			changed = 1;
			for (long i = 0; i != endI1 - endI2; i++)
				parent->end = llLexerItemNext(parent->end);
		}
		if (changed)
			blobUpdateSpan(parent);
	}
}
static void blobDestroy(struct __lexerCacheBlob **blob) {
	blobDetach(*blob);

	__auto_type killData = blob[0]->template->killData;
	if (killData)
		killData(blob[0]->data);
	free(blob[0]);
}
static int lexerItemIndexCmp(const void *a, const void *b) {
	const long *A = a;
	const struct __lexerItem *B = b;
	return longCmp(*A, B->itemIndex);
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
	long diffNewPos = 0;
	long index = 0;
	// Dummy value will be updated ahead
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
				/**
				 * Check if a new item appears at current pos before updating value
				 */
				__auto_type res = getLexerCanidate(lexer, newData, newPos, NULL, err);
				if (res != NULL) {
					if (llLexerItemValuePtr(res)->template != template) {
						llLexerItemValuePtr(res)->template->killItemData(
						    llLexerItemValuePtr(res));
						goto findNewItems;
					}
				}

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

					llLexerItemValuePtr(newNode)->itemIndex = index++;
					llInsertListAfter(retVal, newNode);
					retVal = newNode;
					continue;
				} else if (state == LEXER_DESTROY) {
					currentItem = llLexerItemNext(currentItem);

					goto findNewItems;
				} else if (state == LEXER_UNCHANGED) {
					__auto_type clone = createCloneNode(currentItem);
					__auto_type currItemPtr = llLexerItemValuePtr(currentItem);
					newPos = currItemPtr->end - diffOldPos + diffNewPos;
					currItemPtr->end = newPos;
					currItemPtr->start = currItemPtr->start - diffOldPos + diffNewPos;
					currItemPtr->itemIndex = index++;
					llLexerItemInsertListAfter(retVal, clone);
					retVal = clone;

					/**
					 * Check if no items comsumed,
					 * if so ,find last item in sameDiff ,check if has adjacent charators
					 * that can modify the value changed item(using isAdjChar). This
					 * allows skipping the items from the unmodifed item to the last
					 * modified item
					 */
					__auto_type oldItem = currentItem;
					currentItem = llLexerItemNext(currentItem);

					long offset = sameDiffEnd - diffNewPos;

					llLexerItem nodeBeforeDiffEnd = NULL;
					for (__auto_type node = currentItem; node != NULL;
					     node = llLexerItemNext(node)) {
						if (llLexerItemValuePtr(node)->end <= offset + diffOldPos) {
							nodeBeforeDiffEnd = node;
							continue;
						} else
							break;
					}

					if (nodeBeforeDiffEnd != NULL) {
						for (; nodeBeforeDiffEnd != oldItem;) {
							long offsetEnd =
							    llLexerItemValuePtr(nodeBeforeDiffEnd)->end - diffOldPos;
							long offsetStart =
							    llLexerItemValuePtr(nodeBeforeDiffEnd)->start - diffOldPos;

							for (long i = 0; i != strLexerItemTemplateSize(lexer->templates);
							     i++) {
								__auto_type template = lexer->templates[i];
								if (template->isAdjChar != NULL) {
									if (offsetEnd + diffNewPos < __vecSize(newData)) {
										if (template->isAdjChar(
										        ((char *)newData)[offsetEnd + diffNewPos]))
											goto couldBeModified;
									}
									if (offsetStart + diffNewPos - 1 >= 0) {
										if (template->isAdjChar(
										        ((char *)newData)[offsetStart + diffNewPos - 1]))
											goto couldBeModified;
									}
								} else
									goto couldBeModified;
							}
							/**
							 * Item is not adjacent to any charactors that affect the
							 * value of the item,quit continue as normal
							 */
							goto unmodified;
						couldBeModified : {
							nodeBeforeDiffEnd = llLexerItemPrev(nodeBeforeDiffEnd);
						}
						}
					}
					continue;
				unmodified:
					currentItem = llLexerItemNext(nodeBeforeDiffEnd);

					long offsetEnd =
					    llLexerItemValuePtr(nodeBeforeDiffEnd)->end - diffOldPos;
					newPos = diffNewPos + offsetEnd;

					// TODO write items to retVal
					for (__auto_type node = llLexerItemNext(oldItem); node != currentItem;
					     node = llLexerItemNext(node)) {
						__auto_type clone = createCloneNode(node);

						llLexerItemValuePtr(clone)->itemIndex = index++;
						llInsertListAfter(retVal, clone);
						retVal = llLexerItemLast(retVal);
					}

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

				llLexerItemValuePtr(res)->itemIndex = index++;
				llLexerItemInsertListAfter(retVal, (llLexerItem)res);
				retVal = res;
				/**
				 * Check for insert/adjacent insert
				 */
				for (int i = 0; i != 2; i++) {
					__auto_type node = (i == 0)
					                       ? llLexerItemValuePtr(llLexerItemPrev(retVal))
					                       : llLexerItemValuePtr(llLexerItemNext(retVal));
					if (node == NULL)
						continue;

					if (llLexerItemPrev(retVal) != NULL)
						if (node->blob != NULL)
							lexerCacheBlobUpdateInsert(node->blob, retVal);
				}

				newPos = end;

				__auto_type newCurrentItem = findEndOfConsumedItems(
				    retVal, currentItem,
				    diffs); // killConsumedItems returns next (availible) item
				currentItem = newCurrentItem;
			} while (newPos < diffNewPos);
		}
		}
	}
	/**
	 * Replace nodes with clone template with actual values
	 */
	for (__auto_type node = llLexerItemFirst(retVal); node != NULL;
	     node = llLexerItemNext(node)) {
		if (llLexerItemValuePtr(node)->template == &cloneTemplate)
			__cloneNodeReplace(node);
	}
	/**
	 * Trigger update for blobs
	 */
	for (__auto_type node = llPtrFirst(toUpdate); node != NULL;
	     node = llPtrNext(node)) {
		struct __lexerCacheBlob *blob = *llPtrValuePtr(node);
		if (blob->template->update) {
		blobLoop:;
			int useNULLs =
			    (blob->flags & CACHE_FLAG_ALL_REMOVED) == CACHE_FLAG_ALL_REMOVED;
			__auto_type start2 = (!useNULLs) ? blob->start : NULL;
			__auto_type end2 = (!useNULLs) ? blob->end : NULL;

			__auto_type retCode = blob->template->update(
			    blob->data, start2, end2, &blob->start, &blob->end, blob->flags);

			/**
			 * Check if old range is same range as before,if not update range
			 */
			if (start2 != blob->start || end2 != blob->end)
				blobUpdateSpan(blob);

			__auto_type parentBlob = blob;
			if (retCode == CACHE_BLOB_RET_DESTROY) {
				blobDestroy(&blob);
				goto goUpwards;
			}
			if (retCode == CACHE_BLOB_RET_KEEP) {
			}
			continue;
		goUpwards : {
			blob = parentBlob;
			goto blobLoop;
		}
		}
	}
	toUpdateDestroy();
	toUpdate = NULL;

	lexer->oldSource = __vecAppendItem(NULL, newData, __vecSize(newData));
	lexer->oldItems = __llGetFirst(retVal);

	return;
error : {
	toUpdateDestroy();
	llLexerItemDestroy(&retVal, killLexerItemsNode);
}
}
