#include <assert.h>
#include <linkedList.h>
#include <stdio.h>
LL_TYPE_DEF(int, Int);
LL_TYPE_FUNCS(int, Int);
static int orderPred(const void *a, const void *b) {
	const int *A = a, *B = b;
	return *A - *B;
}
void llTests() {
	llInt ll = llIntEmpty();
	__auto_type negOne = llIntCreate(-1);
	__auto_type one = llIntCreate(1);
	__auto_type two = llIntCreate(2);
	__auto_type three = llIntCreate(3);
	__auto_type five = llIntCreate(5);
	ll = llIntInsert(ll, negOne, NULL);
	ll = llIntInsert(ll, one, NULL);
	ll = llIntInsert(ll, two, NULL);
	ll = llIntInsert(ll, three, NULL);
	assert(llIntNext(one) == two);
	assert(llIntNext(two) == three);
	assert(llIntPrev(three) == two);
	assert(llIntPrev(two) == one);
	ll = llIntInsert(ll, five, NULL);
	// Test insert between nodes from start
	__auto_type four = llIntCreate(4);
	llIntInsert(one, four, orderPred);
	assert(llIntNext(three) == four);
	assert(llIntPrev(five) == four);
	// Test insert between nodes from end
	__auto_type zero = llIntCreate(0);
	llIntInsert(five, zero, orderPred);
	assert(llIntNext(zero) == one);
	assert(llIntPrev(zero) == negOne);
	//
	llIntDestroy(&ll);
}
