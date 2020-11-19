#include <linkedList.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
struct __ll {
	struct __ll *prev;
	struct __ll *next;
	long itemSize;
};
void *__llValuePtr(const struct __ll *node);
void llInsertListAfter(struct __ll *a, struct __ll *b) {
		struct __ll *oldNext = NULL;
	__auto_type bEnd = __llGetEnd(b);
	__auto_type bStart = __llGetFirst(b);
	if (a != NULL) {
		oldNext = a->next;
		a->next = bStart;
	}
	if (b != NULL) {
		bStart->prev = a;
		bEnd->next = oldNext;
		if (oldNext != NULL)
			oldNext->prev = bEnd;
	}
}
void llInsertListBefore(struct __ll *a, struct __ll *b) {
	struct __ll *oldPrev = NULL;
	__auto_type bEnd = __llGetEnd(b);
	__auto_type bStart = __llGetFirst(b);
	if (a != NULL) {
		oldPrev = a->prev;
		a->prev = bEnd;
	}
	if (b != NULL) {
		bEnd->next = a;
		bStart->prev = oldPrev;
	}
	if (oldPrev != NULL) {
		oldPrev->next = bStart;
	}
}
struct __ll *__llInsert(struct __ll *from, struct __ll *newItem,
                        int (*pred)(const void *, const void *)) {
	if (from == NULL)
		return newItem;
	if (pred == NULL) {
		llInsertListAfter(from, newItem);
		return newItem;
	} else {
		__auto_type current = from;
		bool wentForward = false;
		bool wentBackward = false;
		__auto_type lastCurrent = from;
		while (current != NULL) {
			lastCurrent = current;
			__auto_type result = pred(__llValuePtr(newItem), __llValuePtr(current));
			if (result == 0) {
				llInsertListAfter(current, newItem);
				return newItem;
			} else if (result < 0) {
				current = current->prev;
				wentBackward = true;
			} else if (result > 0) {
				current = current->next;
				wentForward = true;
			}
			//
			if (wentForward && wentBackward) {
				bool movedForward = lastCurrent->next == current;
				if (movedForward)
					llInsertListBefore(current, newItem);
				else
					llInsertListAfter(current, newItem);
				return newItem;
			}
		}
		if (wentBackward && !wentForward)
			llInsertListBefore(lastCurrent, newItem);
		if (wentForward && !wentBackward)
			llInsertListAfter(lastCurrent, newItem);
	}
	return newItem;
}
struct __ll *__llCreate(const void *item, long size) {
	struct __ll *retVal = malloc(sizeof(struct __ll) + size);
	retVal->next = NULL;
	retVal->prev = NULL;
	memcpy((void *)retVal + sizeof(struct __ll), item, size);
	retVal->itemSize = size;
	return retVal;
}
static void __llInsertNodeAfter(struct __ll *a, struct __ll *b) {
	if (a != NULL)
		a->next = b;
	if (b != NULL)
		b->prev = a;
}
struct __ll *__llRemoveNode(struct __ll *node) {
	__auto_type result = (node->prev == NULL) ? node->next : node->prev;
	__llInsertNodeAfter(node->prev, node->next);
	node->next = NULL;
	node->prev = NULL;
	return result;
}
void *__llValuePtr(const struct __ll *node) {
	if (node == NULL)
		return NULL;

	return (void *)node + sizeof(struct __ll);
}
static  void __llKillRight(struct __ll *node,
                                    void (*killFunc)(void *)) {
	for (__auto_type current = node->prev; current != NULL;) {
		__auto_type prev = current->prev;
		if (killFunc)
			killFunc(__llValuePtr(current));
		free(current);
		current = prev;
	}
}
static void __llKillLeft(struct __ll *node,
                                   void (*killFunc)(void *)) {
	for (__auto_type current = node->next; current != NULL;) {
		__auto_type next = current->next;
		if (killFunc)
			killFunc(__llValuePtr(current));
		free(current);
		current = next;
	}
}
void __llDestroy(struct __ll *node, void (*killFunc)(void *)) {
	if (node == NULL)
		return;
	 __llKillLeft(node, killFunc);
	 __llKillRight(node, killFunc);
	if (killFunc)
		killFunc(__llValuePtr(node));
}
struct __ll *__llNext(const struct __ll *node) {
	if (node == NULL)
		return NULL;
	return node->next;
}
struct __ll *__llPrev(const struct __ll *node) {
	if (node == NULL)
		return NULL;
	return node->prev;
}
int llLastPred(const void *a, const void *b) { return 1; }
int __llFirstPred(const void *a, const void *b) { return -1; }
struct __ll *__llFindLeft(const struct __ll *list, const void *data,
                          int (*pred)(const void *a, const void *b)) {
	if (list == NULL)
		return NULL;
	if (0 == pred(data, __llValuePtr(list)))
		return (struct __ll *)list;
	for (struct __ll *left = list->prev; left != NULL; left = left->prev) {
		if (pred(data, __llValuePtr(left)) == 0)
			return left;
		else if (pred(data, __llValuePtr(left)) > 0)
			break;
	}
	return NULL;
}
struct __ll *__llFindRight(const struct __ll *list, const void *data,
                           int (*pred)(const void *a, const void *b)) {
	if (list == NULL)
		return NULL;
	if (0 == pred(data, __llValuePtr(list)))
		return (struct __ll *)list;
	for (struct __ll *right = list->next; right != NULL; right = right->next) {
		if (pred(data, __llValuePtr(right)) == 0)
			return right;
		else if (pred(data, __llValuePtr(right)) < 0)
			break;
	}
	return NULL;
}
struct __ll *__llGetFirst(const struct __ll *list) {
	if (list == NULL)
		return NULL;
	__auto_type left = list;
	for (;;) {
		if (left->prev == NULL)
			return (struct __ll *)left;
		left = left->prev;
	}
	return NULL;
}
struct __ll *__llGetEnd(const struct __ll *list) {
	if (list == NULL)
		return NULL;
	__auto_type right = list;
	for (;;) {
		if (right->next == NULL)
			return (struct __ll *)right;
		right = right->next;
	}
	return NULL;
}
long __llSize(const struct __ll *list) {
	__auto_type first = __llGetFirst(list);
	long retVal = 0;
	for (; first != NULL; retVal++)
		first = first->next;
	return retVal;
}
struct __ll *__llValueResize(struct __ll *list, long newSize) {
	__auto_type oldPrev = list->prev;
	__auto_type oldNext = list->next;

	list = realloc(list, sizeof(struct __ll) + newSize);

	if (oldPrev != NULL)
		oldPrev->next = list;
	if (oldNext != NULL)
		oldNext->prev = list;

	list->itemSize = newSize;
	return list;
}
long __llItemSize(const struct __ll *list) { return list->itemSize; }
struct __ll* __llFind(const struct __ll *list,const void *data,int(*pred)(const void *,const void*)) {
		int prev=0;
		while(list!=NULL) {
				int cmp=pred(data,__llValuePtr(list)); 
				if(cmp==0) {
						return (struct __ll*)list;
				}
				//prev is used to check if not moving backwards and forwards forever if value is between next node and curr
				else if(cmp<0&&(cmp==prev||prev==0)) {
						list=__llPrev(list);
						continue;
				}
				//prev is used to check if not moving backwards and forwards forever if value is between previous node and curr
				else if(cmp>0&&(cmp==prev||prev==0)) {
						list=__llNext(list);
						continue;
				}
				return NULL;
		}
		
		return NULL;
}
