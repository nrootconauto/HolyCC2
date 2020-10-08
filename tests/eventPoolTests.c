#include <assert.h>
#include <eventPool.h>
#include <stdlib.h>
static int a;
static int aTwo;
static int b;
static int c;
static void setOne(void *callValue, void *ptr) {
	assert(*(int *)callValue == 1);
	*(int *)ptr = 1;
}
static void setTwo(void *callValue, void *ptr) {
	assert(*(int *)callValue == 2);
	*(int *)ptr = 2;
}
static void setThree(void *callValue, void *ptr) {
	assert(*(int *)callValue == 3);
	*(int *)ptr = 3;
}
void eventPoolTests() {
	a = 0, b = 0, c = 0, aTwo = 0;
	int a2 = 1, b2 = 2, c2 = 3;
	__auto_type ep = eventPoolCreate();

	__auto_type aEvent = eventPoolAdd(ep, "a", setOne, &a, NULL);
	__auto_type aTwoEvent = eventPoolAdd(ep, "a", setOne, &aTwo, NULL);
	__auto_type bEvent = eventPoolAdd(ep, "b", setTwo, &b, NULL);
	__auto_type cEvent = eventPoolAdd(ep, "c", setThree, &c, NULL);

	eventPoolTrigger(ep, "a", &a2);
	eventPoolTrigger(ep, "b", &b2);
	eventPoolTrigger(ep, "c", &c2);

	assert(a == 1 && aTwo == 1 && b == 2 && c == 3);
	eventPoolRemove(ep, aEvent);
	eventPoolRemove(ep, bEvent);
	eventPoolRemove(ep, cEvent);
	a = 0, b = 0, c = 0, aTwo = 0;

	eventPoolTrigger(ep, "a", &a2);
	eventPoolTrigger(ep, "b", &b2);
	eventPoolTrigger(ep, "c", &c2);
	assert(a == 0 && aTwo == 1 && b == 0 && c == 0);

	eventPoolDestroy(ep);
}
