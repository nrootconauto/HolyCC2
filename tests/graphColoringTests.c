#include <graphColoring.h>
#include <stdio.h>
GRAPH_TYPE_DEF(int, void *, Int);
GRAPH_TYPE_FUNCS(int, void *, Int);
void graphColoringTests() {
	{
		__auto_type a = graphNodeIntCreate(1, 0);
		__auto_type b = graphNodeIntCreate(2, 0);
		__auto_type c = graphNodeIntCreate(3, 0);
		__auto_type d = graphNodeIntCreate(4, 0);

		graphNodeIntConnect(a, b, NULL);
		graphNodeIntConnect(a, c, NULL);

		graphNodeIntConnect(b, c, NULL);

		graphNodeIntConnect(c, d, NULL);

		__auto_type colors = graphColor(a);
		printf("Test graph has %li colors.\n", vertexColorCount(colors));
	}
	{
		// Petersen graph
		__auto_type n1 = graphNodeIntCreate(1, 0);
		__auto_type n2 = graphNodeIntCreate(2, 0);
		__auto_type n3 = graphNodeIntCreate(3, 0);
		__auto_type n4 = graphNodeIntCreate(4, 0);
		__auto_type n5 = graphNodeIntCreate(1, 0);
		__auto_type n6 = graphNodeIntCreate(2, 0);
		__auto_type n7 = graphNodeIntCreate(3, 0);
		__auto_type n8 = graphNodeIntCreate(4, 0);
		__auto_type n9 = graphNodeIntCreate(5, 0);
		__auto_type n10 = graphNodeIntCreate(10, 0);

		// Outside
		graphNodeIntConnect(n1, n2, NULL);
		graphNodeIntConnect(n2, n3, NULL);
		graphNodeIntConnect(n3, n4, NULL);
		graphNodeIntConnect(n4, n5, NULL);
		graphNodeIntConnect(n5, n1, NULL);

		// Inside
		graphNodeIntConnect(n6, n7, NULL);

		graphNodeIntConnect(n7, n9, NULL);

		graphNodeIntConnect(n9, n10, NULL);

		graphNodeIntConnect(n10, n8, NULL);

		graphNodeIntConnect(n6, n8, NULL);

		// Connect inside to outside
		graphNodeIntConnect(n1, n7, NULL);
		graphNodeIntConnect(n2, n10, NULL);
		graphNodeIntConnect(n3, n6, NULL);
		graphNodeIntConnect(n4, n9, NULL);
		graphNodeIntConnect(n5, n8, NULL);

		__auto_type colors = graphColor(n1);
		printf("Test graph has %li colors.\n", vertexColorCount(colors));
	}
}
