#include <assert.h>
#include <graph.h>
#include <stdbool.h>
GRAPH_TYPE_DEF(int, Int);
GRAPH_TYPE_FUNCS(int, Int);
void graphTests() {
	__auto_type a = graphNodeIntCreate(1, 0);
	__auto_type b = graphNodeIntCreate(2, 0);
	__auto_type c = graphNodeIntCreate(3, 0);
	__auto_type d = graphNodeIntCreate(3, 0);
	//
	//     /--b--\
	// a--*       *--d
	//     \--c--/
	//
	graphNodeIntConnect(a, b);
	graphNodeIntConnect(a, c);
	graphNodeIntConnect(b, d);
	graphNodeIntConnect(c, d);
	__auto_type aO = graphNodeIntOutgoing(a);
	__auto_type bO = graphNodeIntOutgoing(b);
	__auto_type cO = graphNodeIntOutgoing(c);
	assert(strGraphNodeIntPSize(aO) == 2);
	assert(strGraphNodeIntPSize(bO) == 1);
	assert(strGraphNodeIntPSize(cO) == 1);
	bool foundB = false, foundC = false;
	for (int i = 0; i != 2; i++) {
		foundB |= aO[i] == b;
		foundC |= aO[i] == c;
	}
	assert(foundB && foundC);
	assert(cO[0] == d);
	assert(bO[0] == d);
	__auto_type dI = graphNodeIntIncoming(d);
	__auto_type bI = graphNodeIntIncoming(b);
	__auto_type cI = graphNodeIntIncoming(c);
	assert(bI[0] == a);
	assert(cI[0] == a);
	foundB = false, foundC = false;
	for (int i = 0; i != 2; i++) {
		foundB |= dI[i] == b;
		foundC |= dI[i] == c;
	}
	assert(foundB && foundC);
	// detach b
	graphNodeIntDetach(b, d);
	graphNodeIntDetach(a, b);
	aO = graphNodeIntOutgoing(a);
	dI = graphNodeIntIncoming(d);
	assert(strGraphNodeIntPSize(aO) == 1);
	assert(aO[0] == c);
	assert(strGraphNodeIntPSize(dI) == 1);
	assert(dI[0] == c);
	bI = graphNodeIntIncoming(b);
	bO = graphNodeIntOutgoing(b);
	assert(strGraphNodeIntPSize(bI) == 0);
	assert(strGraphNodeIntPSize(bO) == 0);
}
