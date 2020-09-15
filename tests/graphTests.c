#include <assert.h>
#include <graph.h>
#include <stdbool.h>
GRAPH_TYPE_DEF(int, char, Int);
GRAPH_TYPE_FUNCS(int, char, Int);
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
	graphNodeIntConnect(a, b, 'b');
	graphNodeIntConnect(a, c, 'c');
	graphNodeIntConnect(b, d, 'd');
	graphNodeIntConnect(c, d, 'd');
	__auto_type aO = graphNodeIntOutgoing(a);
	__auto_type bO = graphNodeIntOutgoing(b);
	__auto_type cO = graphNodeIntOutgoing(c);
	assert(strGraphEdgeIntPSize(aO) == 2);
	assert(strGraphEdgeIntPSize(bO) == 1);
	assert(strGraphEdgeIntPSize(cO) == 1);
	bool foundB = false, foundC = false;
	for (int i = 0; i != 2; i++) {
		foundB |= graphEdgeIntOutgoing(aO[i]) == b;
		foundC |= graphEdgeIntOutgoing(aO[i]) == c;
	}
	assert(foundB && foundC);
	assert(graphEdgeIntOutgoing(cO[0]) == d);
	assert(graphEdgeIntOutgoing(bO[0]) == d);
	__auto_type dI = graphNodeIntIncoming(d);
	__auto_type bI = graphNodeIntIncoming(b);
	__auto_type cI = graphNodeIntIncoming(c);
	assert(graphEdgeIntOutgoing(bI[0]) == a);
	assert(graphEdgeIntOutgoing(cI[0]) == a);
	foundB = false, foundC = false;
	for (int i = 0; i != 2; i++) {
		foundB |= graphEdgeIntOutgoing(dI[i]) == b;
		foundC |= graphEdgeIntOutgoing(dI[i]) == c;
	}
	assert(foundB && foundC);
	// detach b
	graphEdgeIntKill(b, d, NULL, NULL, NULL);
	graphEdgeIntKill(a, b, NULL, NULL, NULL);
	aO = graphNodeIntOutgoing(a);
	dI = graphNodeIntIncoming(d);
	assert(strGraphEdgeIntPSize(aO) == 1);
	assert(graphEdgeIntOutgoing(aO[0]) == c);
	assert(strGraphEdgeIntPSize(dI) == 1);
	assert(graphEdgeIntOutgoing(dI[0]) == c);
	bI = graphNodeIntIncoming(b);
	bO = graphNodeIntOutgoing(b);
	assert(strGraphEdgeIntPSize(bI) == 0);
	assert(strGraphEdgeIntPSize(bO) == 0);
	//
}
