#include <graphColoring.h>
GRAPH_TYPE_DEF(int, void *, Int);
GRAPH_TYPE_FUNCS(int, void *, Int);
void graphColoringTests() {
	{
		__auto_type a = graphNodeIntCreate(1, 0);
		__auto_type b = graphNodeIntCreate(2, 0);
		__auto_type c = graphNodeIntCreate(3, 0);
		__auto_type d = graphNodeIntCreate(4, 0);
		
		graphNodeIntConnect(a,b,NULL);
		graphNodeIntConnect(a,c,NULL);
		
		graphNodeIntConnect(b,c,NULL);
		
		graphNodeIntConnect(c,d,NULL);
		
		graphColor(a);
	}
}
