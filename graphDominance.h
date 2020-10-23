#pragma once
struct graphDominators {
	struct __graphNode *node;
	strGraphNodeP dominators;
};
LL_TYPE_DEF(struct graphDominators, Dominators);
LL_TYPE_FUNCS(struct graphDominators, Dominators);
int llDominatorCmp(const void *a, const void *b);
