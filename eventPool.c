#include <assert.h>
#include <eventPool.h>
#include <hashTable.h>
#include <linkedList.h>
#include <string.h>
struct event {
	void *data;
	const char *name;
	void (*func)(void *, void *);
	void (*killData)(void *);
};
LL_TYPE_DEF(struct event, Event);
LL_TYPE_FUNCS(struct event, Event);
MAP_TYPE_DEF(llEvent, LLEvent);
MAP_TYPE_FUNCS(llEvent, LLEvent);
typedef mapLLEvent __eventPool;
static int ptrPtrCmp(const void *a, const void *b) {
	if (a > b)
		return 1;
	else if (a == b)
		return 0;
	else
		return -1;
}
struct event *eventPoolAdd(struct eventPool *pool, const char *name,
                           void (*func)(void *, void *), void *data,
                           void (*killData)(void *)) {
	__eventPool __pool = (void *)pool;

	struct event tmp;
	tmp.data = data, tmp.func = func, tmp.killData = killData;
	__auto_type retVal = llEventCreate(tmp);

	__auto_type find = mapLLEventGet(__pool, name);
	if (NULL == find) {
		mapLLEventInsert(__pool, name, retVal);
		find = mapLLEventGet(__pool, name);
	} else {
		*find = llEventInsert(*find, retVal, ptrPtrCmp);
	}

	llEventValuePtr(retVal)->name = mapLLEventValueKey(find);
	return llEventValuePtr(retVal);
}

void eventPoolRemove(struct eventPool *pool, struct event *event) {
	__eventPool __pool = (void *)pool;

	llEvent *find = mapLLEventGet(__pool, event->name);
	assert(find != NULL);

	__auto_type res = llEventFindRight(llEventFirst(*find), event, ptrPtrCmp);
	assert(res != NULL);
	*find = llEventRemove(res);

	// Clone name in buffer,will be removing value that is attached to the name
	char buffer[strlen(event->name) + 1];
	strcpy(buffer, event->name);
	if (find == NULL)
		mapLLEventRemove(__pool, buffer, NULL);

	if (event->killData != NULL)
		event->killData(event->data);
}
struct eventPool *eventPoolCreate() {
	return (void *)mapLLEventCreate();
}
static void killEvent(void *event) {
	struct event *e = event;
	if (e->killData != NULL)
		e->killData(e->data);
}
static void killLLEvent(void *ll) { llEventDestroy(ll, killEvent); }
void eventPoolDestroy(struct eventPool *pool) {
	__eventPool __pool = (void *)pool;

	mapLLEventDestroy(__pool, killLLEvent);
}
void eventPoolTrigger(struct eventPool *pool, const char *name, void *data) {
	__eventPool __pool = (void *)pool;

	__auto_type find = mapLLEventGet(__pool, name);
	if (find == NULL)
		return;

	for (__auto_type node = llEventFirst(*find); node != NULL;
	     node = llEventNext(node)) {
		__auto_type event = llEventValuePtr(node);
		if (event->func != NULL)
			event->func(data, event->data);
	}
}
