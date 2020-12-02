#include <assert.h>
#include <graph.h>
#include <stdbool.h>
GRAPH_TYPE_DEF(int, char, Int);
GRAPH_TYPE_FUNCS(int, char, Int);
static void appendToItems(graphNodeInt node, void *data) {
	strGraphNodeIntP *edges = data;
	*edges = strGraphNodeIntPAppendItem(*edges, node);
}
static int nodeIs2Or3(graphNodeInt node,void* data) {
		int val=*graphNodeIntValuePtr(node);
		return !(val==2||val==3);
}
void graphTests() {
		{
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
	assert(graphEdgeIntIncoming(bI[0]) == a);
	assert(graphEdgeIntIncoming(cI[0]) == a);
	foundB = false, foundC = false;
	for (int i = 0; i != 2; i++) {
		foundB |= graphEdgeIntIncoming(dI[i]) == b;
		foundC |= graphEdgeIntIncoming(dI[i]) == c;
	}
	assert(foundB && foundC);
	// navigate from a to d
	strGraphNodeIntP visited = NULL;
	graphNodeIntVisitForward(a, &visited, NULL, appendToItems);
	assert(strGraphNodePSize(visited) == 3);
	graphNodeInt toFind[4] = {b, c, d}; // Starts from a,doesnt visit it
	int foundCount = 0;
	for (int i = 0; i != 3; i++) {
		for (int i2 = 0; i2 != 3; i2++) {
			if (visited[i2] == toFind[i])
				foundCount++;
		}
	}
	assert(foundCount == 3);
	strGraphNodePDestroy(&visited);
	// detach b
	graphEdgeIntKill(b, d, NULL, NULL, NULL);
	graphEdgeIntKill(a, b, NULL, NULL, NULL);
	aO = graphNodeIntOutgoing(a);
	dI = graphNodeIntIncoming(d);
	assert(strGraphEdgeIntPSize(aO) == 1);
	assert(graphEdgeIntOutgoing(aO[0]) == c);
	assert(strGraphEdgeIntPSize(dI) == 1);
	assert(graphEdgeIntIncoming(dI[0]) == c);
	bI = graphNodeIntIncoming(b);
	bO = graphNodeIntOutgoing(b);
	assert(strGraphEdgeIntPSize(bI) == 0);
	assert(strGraphEdgeIntPSize(bO) == 0);
}
		//
		// Mapping (edges not preserved) and filter tests
		//
		{
				__auto_type a = graphNodeIntCreate(1, 0);
				__auto_type b = graphNodeIntCreate(2, 0);
				__auto_type c = graphNodeIntCreate(3, 0);
				__auto_type d = graphNodeIntCreate(4, 0);
				//
				//     /--b--\
				// a--*       *--d
				//     \--c--/
				//
				graphNodeIntConnect(a, b, 'b');
				graphNodeIntConnect(a, c, 'c');
				graphNodeIntConnect(b, d, 'd');
				graphNodeIntConnect(c, d, 'd');

				//Check for all nodes
				__auto_type all=graphNodeIntAllNodes(a);
				assert(strGraphNodeIntPSize(all)==4);
				graphNodeInt expected[]={a,b,c,d};
				for(long i=0;i!=4;i++) {
						for(long i2=0;i2!=4;i2++) {
								if(expected[i]==all[i2])
										expected[i]=NULL;
						}
				}
				for(long i=0;i!=4;i++)
						assert(!expected[i]);

				//Create map
				__auto_type map1=graphNodeCreateMapping(all[0], 1);
				__auto_type allMap1=graphNodeMappingAllNodes(map1);
				assert(strGraphNodeMappingPSize(allMap1)==4);
				graphNodeInt expected2[]={a,b,c,d};
				for(long i=0;i!=4;i++) {
						for(long i2=0;i2!=4;i2++) {
								__auto_type n=*graphNodeMappingValuePtr(allMap1[i2]);
								if(expected2[i]==n)
										expected2[i]=NULL;
						}
				}
				//Check if all found
				for(long i=0;i!=4;i++)
						assert(!expected2[i]);
				
				//filter out b(2)/c(3)
				__auto_type mapFiltered=createFilteredGraph(NULL,all, NULL,nodeIs2Or3);
				__auto_type allFiltered=graphNodeMappingAllNodes(mapFiltered);

				//
				// a->d
				//
				assert(strGraphNodeMappingPSize(allFiltered)==2);
				graphNodeInt expected3[]={a,d};
				for(long i=0;i!=2;i++) {
						for(long i2=0;i2!=2;i2++) {
								__auto_type n=*graphNodeMappingValuePtr(allFiltered[i2]);
								if(expected3[i]==n)
										expected3[i]=NULL;
						}
				}
				//Check if all found
				for(long i=0;i!=2;i++)
						assert(!expected3[i]);
		}
		//
		// Find all paths to node
		//
		{
				__auto_type a = graphNodeIntCreate(1, 0);
				__auto_type b = graphNodeIntCreate(2, 0);
				__auto_type c = graphNodeIntCreate(3, 0);
				__auto_type d = graphNodeIntCreate(4, 0);
				//
				//     /--b--\
				// a--*       *--d
				//     \--c--/
				//
				graphNodeIntConnect(a, b, 'b');
				graphNodeIntConnect(a, c, 'c');
				graphNodeIntConnect(b, d, 'd');
				graphNodeIntConnect(c, d, 'd');
		}
}
