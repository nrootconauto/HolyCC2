#pragma once
#include <graph.h>
#include <linkedList.h>
#include <str.h>
struct graphDominators {
	struct __graphNode *node;
	strGraphNodeP dominators;
};
LL_TYPE_DEF(struct graphDominators, Dominators);
LL_TYPE_FUNCS(struct graphDominators, Dominators);
llDominators graphComputeDominatorsPerNode(struct __graphNode *start);
struct graphDomFrontier {
	struct __graphNode *node;
	strGraphNodeP nodes;
};
LL_TYPE_DEF(struct graphDominatorFrontier, DomFrontier);
LL_TYPE_FUNCS(struct graphDomFrontier, DomFrontier);
llDomFrontier graphDominanceFrontiers(struct __graphNode *start, const llDominators doms);
struct __graphNode *graphDominatorIdom(const llDominators doms, struct __graphNode *node);
int llDominatorCmp(const void *a, const struct graphDominators *b);
int llDomFrontierCmp(const void *a, const struct graphDomFrontier *B);
graphNodeMapping dominatorsTreeCreate(llDominators doms);
