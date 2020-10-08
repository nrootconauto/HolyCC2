#include <assert.h>
#include <hashTable.h>
#include <linkedList.h>
#include <string.h>
struct event {
	void *data;
	const char *name;
	void (*func)(void *, void *);
};
LL_TYPE_DEF(struct event, Event);
LL_TYPE_FUNCS(struct event, Event);
MAP_TYPE_DEF(llEvent, LLEvent);
MAP_TYPE_FUNCS(llEvent, LLEvent);
typedef mapLLEvent eventPool;
static int ptrCmp(const void *a, const void *b) {
	if (a > b)
		return 1;
	else if (a == b)
		return 0;
	else
		return -1;
}
struct event *eventPoolAdd(eventPool pool, const char *name,
                           void (*func)(void *, void *), void *data) {
	struct event tmp;
	tmp.data = data, tmp.func = func;
	__auto_type retVal = llEventCreate(tmp);

	__auto_type find = mapLLEventGet(pool, name);
	if (NULL == find) {
		mapLLEventInsert(pool, name, retVal);
		find = mapLLEventGet(pool, name);
	} else {
		*find = llEventInsert(*find, retVal, ptrCmp);
	}

	llEventValuePtr(retVal)->name = mapLLEventValueKey(find);
	return llEventValuePtr(retVal);
}

void eventPoolRemove(eventPool pool, struct event *event) {
	llEvent *find = mapLLEventGet(pool, event->name);
	assert(find != NULL);

	__auto_type res=llEventFindRight(llEventFirst(*find), event, ptrCmp);
	assert(res!=NULL);
	*find=llEventRemove(res);
}
