#include <libdill.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
struct __ll {
	struct __ll *prev;
	struct __ll *next;
};
void *__llValuePtr(struct __ll *node);
static void llInsertAfter(struct __ll *a, struct __ll *b) {
	struct __ll *oldNext = NULL;
	if (a != NULL) {
		oldNext = a->next;
		a->next = b;
	}
	if (b != NULL) {
		b->prev = a;
		b->next = oldNext;
		if (oldNext != NULL)
			oldNext->prev = b;
	}
}
static void llInsertBefore(struct __ll *a, struct __ll *b) {
	struct __ll *oldPrev = NULL;
	if (a != NULL) {
		oldPrev = a->prev;
		a->prev = b;
	}
	if (b != NULL)
		b->next = a;
	b->prev = oldPrev;
	if (oldPrev == NULL) {
		oldPrev->next = b;
	}
}
struct __ll *__llInsert(struct __ll *from, struct __ll *newItem,
                        int (*pred)(void *, void *)) {
	if (from == NULL)
		return newItem;
	if (pred == NULL) {
		llInsertAfter(from, newItem);
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
				llInsertAfter(current, newItem);
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
					llInsertBefore(current, newItem);
				else
					llInsertAfter(current, newItem);
				return newItem;
			}
		}
		if (wentBackward && !wentForward)
			llInsertBefore(lastCurrent, newItem);
		if (wentForward && !wentBackward)
			llInsertAfter(lastCurrent, newItem);
	}
	return newItem;
}
struct __ll *__llCreate(void *item, long size) {
	struct __ll *retVal = malloc(sizeof(struct __ll) + size);
	retVal->next = NULL;
	retVal->prev = NULL;
	memcpy((void *)retVal + sizeof(struct __ll), item, size);
	return retVal;
}
void __llRemoveNode(struct __ll *node) {
	llInsertAfter(node->prev, node->next);
	node->next = NULL;
	node->prev = NULL;
}
void *__llValuePtr(struct __ll *node) {
	return (void *)node + sizeof(struct __ll);
}
static coroutine void __llKillRight(struct __ll *node,
                                    void (*killFunc)(void *)) {
	for (__auto_type current = node->prev; current != NULL;) {
		__auto_type prev = current->prev;
		if (killFunc)
			killFunc(__llValuePtr(current));
		free(current);
		current = prev;
	}
}
static coroutine void __llKillLeft(struct __ll *node,
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
	int b = bundle();
	bundle_go(b, __llKillLeft(node, killFunc));
	bundle_go(b, __llKillRight(node, killFunc));
	if (killFunc)
		killFunc(__llValuePtr(node));
	hclose(b);
}
struct __ll *__llNext(struct __ll *node) {
	if (node == NULL)
		return NULL;
	return node->next;
}
struct __ll *__llPrev(struct __ll *node) {
	if (node == NULL)
		return NULL;
	return node->prev;
}
int llLastPred(void *a, void *b) { return 1; }
int __llFirstPred(void *a, void *b) { return -1; }
struct __ll *__llFindLeft(struct __ll *list, void *data,
                          int (*pred)(void *a, void *b)) {
	if (list == NULL)
		return NULL;
	if (pred(list, data))
		return list;
	for (__auto_type left = list->prev; left != NULL; left = left->prev) {
		if (pred(__llValuePtr(left), data))
			return left;
	}
	return NULL;
}
struct __ll *__llFindRight(struct __ll *list, void *data,
                           int (*pred)(void *a, void *b)) {
	if (list == NULL)
		return NULL;
	if (pred(list, data))
		return list;
	for (__auto_type right = list->next; right != NULL; right = right->next) {
		if (pred(__llValuePtr(right), data))
			return right;
	}
	return NULL;
}
struct __ll *__llGetFirst(struct __ll *list) {
	if (list == NULL)
		return NULL;
	__auto_type left = list;
	for (;;) {
		if (left->prev == NULL)
			return left;
		left = left->prev;
	}
	return NULL;
}
struct __ll *__llGetEnd(struct __ll *list) {
	if (list == NULL)
		return NULL;
	__auto_type right = list;
	for (;;) {
		if (right->next == NULL)
			return right;
		right = right->next;
	}
	return NULL;
}
