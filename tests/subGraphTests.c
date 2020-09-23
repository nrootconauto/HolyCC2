#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <subGraph.h>
GRAPH_TYPE_DEF(int, void *, Int);
GRAPH_TYPE_FUNCS(int, void *, Int);
int validateSubgraph1(strGraphNodeSubP subGraphs, graphNodeInt expected[3]) {
	graphNodeSub items[3];
	int result = 1;
	int found = 0;
	for (int i = 0; i != 3; i++) {
		__auto_type node = *graphNodeSubValuePtr(subGraphs[i]);
		for (int i = 0; i != 3; i++)
			if (expected[i] == node) {
				items[i] = subGraphs[i];
				found++;
			}
	}
	result = result && found == 3;
	result = result && (graphNodeSubConnectedTo(items[0], items[1]));
	result = result && (graphNodeSubConnectedTo(items[0], items[2]));
	result = result && (graphNodeSubConnectedTo(items[1], items[2]));
	return result;
}
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
	graphNodeInt expected[3] = {b, c, d};
	assert(validateSubgraph1(subGraphs[0], expected));

	graphNodeIntConnect(a, d, NULL);

	subGraphs = isolateSubGraph(graph, subGraph);
	assert(strSubSize(subGraphs) == 2);
	int found[2] = {0, 0};
	graphNodeInt expected2[3] = {a, b, d};
	for (int i = 0; i != 2; i++) {
		found[i] |= validateSubgraph1(subGraphs[i], expected);
		found[i] |= validateSubgraph1(subGraphs[i], expected2);
	}
	assert(found[0] && found[1]);
}
