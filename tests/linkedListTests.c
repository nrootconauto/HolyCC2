#include <assert.h>
#include <linkedList.h>
#include <stdio.h>
LL_TYPE_DEF(int, Int);
LL_TYPE_FUNCS(int, Int);
static int orderPredIns(const int *a, const int *b) {
	const int *A = a, *B = b;
	return *A - *B;
}
static int orderPredGet(const void *a, const int *b) {
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
	llIntInsert(one, four, orderPredIns);
	assert(llIntNext(three) == four);
	assert(llIntPrev(five) == four);
	// Test insert between nodes from end
	__auto_type zero = llIntCreate(0);
	llIntInsert(five, zero, orderPredIns);
	assert(llIntNext(zero) == one);
	assert(llIntPrev(zero) == negOne);
	// Test sorted right find
	__auto_type first = llIntFirst(five);
	int tmp = 3;
	assert(*llIntValuePtr(llIntFindRight(first, &tmp, orderPredGet)) == 3);
	assert(llIntFindRight(four, &tmp, orderPredGet) == NULL);
	// Test sorted right left
	__auto_type last = llIntLast(one);
	assert(*llIntValuePtr(llIntFindLeft(last, &tmp, orderPredGet)) == 3);
	assert(llIntFindLeft(two, &tmp, orderPredGet) == NULL);

	// Find bi-dir
	tmp = 4;
	assert(llIntFind(one, &tmp, orderPredGet));
	tmp = 2;
	assert(llIntFind(five, &tmp, orderPredGet));
	//
	llIntDestroy(&ll, NULL);
}
