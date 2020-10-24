#pragma once
#include <graph.h>
#include <linkedList.h>
struct vertexColoring {
	struct __graphNode *node;
	int color;
};
LL_TYPE_DEF(struct vertexColoring, VertexColor);
LL_TYPE_FUNCS(struct vertexColoring, VertexColor);
