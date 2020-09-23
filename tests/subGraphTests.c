#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <subGraph.h>
GRAPH_TYPE_DEF(int, void *, Int);
GRAPH_TYPE_FUNCS(int, void *, Int);

void subGraphTests() {
	//
	__auto_type sA = graphNodeIntCreate(1, 0);
	__auto_type sB = graphNodeIntCreate(2, 0);
	__auto_type sC = graphNodeIntCreate(3, 0);
	graphNodeIntConnect(sA, sB, NULL);
	graphNodeIntConnect(sA, sC, NULL);
	graphNodeIntConnect(sB, sC, NULL);

	__auto_type a = graphNodeIntCreate(1, 0);
	__auto_type b = graphNodeIntCreate(2, 0);
	__auto_type c = graphNodeIntCreate(3, 0);
	__auto_type d = graphNodeIntCreate(4, 0);
	graphNodeIntConnect(a, b, NULL);
	graphNodeIntConnect(b, c, NULL);
	graphNodeIntConnect(b, d, NULL);
	graphNodeIntConnect(c, d, NULL);

	__auto_type graph = strGraphNodePResize(NULL, 4);
	graph[0] = a, graph[1] = b, graph[2] = c, graph[3] = d;
	__auto_type subGraph = strGraphNodePResize(NULL, 3);
	subGraph[0] = sA, subGraph[1] = sB, subGraph[2] = sC;

	__auto_type subGraphs = isolateSubGraph(graph, subGraph);

	assert(strSubSize(subGraphs) == 1);
	graphNodeSub items[3];
	graphNodeInt expected[3] = {b, c, d};
	int found = 0;
	for (int i = 0; i != 3; i++) {
		__auto_type node = *graphNodeSubValuePtr(subGraphs[0][i]);
		for (int i = 0; i != 3; i++)
			if (expected[i] == node) {
				items[i] = subGraphs[0][i];
				found++;
			}
	}
	assert(found == 3);
	assert(graphNodeSubConnectedTo(items[0], items[1]));
	assert(graphNodeSubConnectedTo(items[0], items[2]));
	assert(graphNodeSubConnectedTo(items[1], items[2]));
}
