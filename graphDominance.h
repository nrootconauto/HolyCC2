#pragma once
#include <linkedList.h>
#include <str.h>
#include <linkedList.h>
#include <graph.h>
struct graphDominators {
	struct __graphNode *node;
	strGraphNodeP dominators;
};
LL_TYPE_DEF(struct graphDominators, Dominators);
LL_TYPE_FUNCS(struct graphDominators, Dominators);
int llDominatorCmp(const void *a, const void *b);
llDominators graphComputeDominatorsPerNode(struct __graphNode *start);
struct graphDomFrontier {
	struct __graphNode *node;
	strGraphNodeP dominators;
};
LL_TYPE_DEF(struct graphDominatorFrontier , DomFrontier);
LL_TYPE_FUNCS(struct graphDomFrontier , DomFrontier);
llDomFrontier graphDominanceFrontiers(struct __graphNode *start,
                                      const llDominators doms);
struct __graphNode *graphDominatorIdom(const llDominators doms,
                                   struct __graphNode *node);
int llDomFrontierCmp(const void *a, const void *b);
