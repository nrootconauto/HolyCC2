#pragma once
#include <graph.h>
GRAPH_TYPE_DEF(struct __graphNode *, struct __graphEdge *, Sub);
GRAPH_TYPE_FUNCS(struct __graphNode *, struct __graphEdge *, Sub);
STR_TYPE_DEF(strGraphNodeSubP,Sub);
STR_TYPE_FUNCS(strGraphNodeSubP,Sub);
strSub isolateSubGraph(strGraphNodeP graph, strGraphNodeP sub);
