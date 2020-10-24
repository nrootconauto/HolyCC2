#include <assert.h>
#include <graph.h>
#include <graphDominance.h>
GRAPH_TYPE_DEF(int, void *, Int);
GRAPH_TYPE_FUNCS(int, void *, Int);
void graphDominanceCheck(graphNodeInt node, const llDominators doms, int *items,
                         long count) {
	__auto_type find =
	    llDominatorsFindRight(llDominatorsFirst(doms), node, llDominatorCmp);
	assert(find != NULL);
	for (long i = 0; i != count; i++) {
		__auto_type doms = llDominatorsValuePtr(find)->dominators;
		for (long i2 = 0; i2 != strGraphNodePSize(doms); i2++)
			if (*graphNodeIntValuePtr(doms[i2]) == items[i])
				goto success;

		assert(0);
	success:;
	}
}
void graphDominanceTests() {
 {
	__auto_type one = graphNodeIntCreate(1, 0);
	__auto_type two = graphNodeIntCreate(2, 0);
	__auto_type three = graphNodeIntCreate(3, 0);
	__auto_type four = graphNodeIntCreate(4, 0);
	__auto_type five = graphNodeIntCreate(5, 0);
	__auto_type six = graphNodeIntCreate(6, 0);

	graphNodeIntConnect(one, two, NULL);
	graphNodeIntConnect(two, three, NULL);
	graphNodeIntConnect(two, four, NULL);
	graphNodeIntConnect(two, six, NULL);

	graphNodeIntConnect(three, five, NULL);
	graphNodeIntConnect(four, five, NULL);

	graphNodeIntConnect(five, two, NULL);

	__auto_type doms = graphComputeDominatorsPerNode(one);
	int oneTwo[]={1,2};
	graphDominanceCheck(two,doms,oneTwo,2);
	graphDominanceCheck(three,doms,oneTwo,2);
	graphDominanceCheck(four,doms,oneTwo,2);
	graphDominanceCheck(five,doms,oneTwo,2);
	graphDominanceCheck(six,doms,oneTwo,2);
	
	int one_2[]={1};
	graphDominanceCheck(one,doms,one_2,1);
	
	assert(two==graphDominatorIdom(doms,six));
	assert(two==graphDominatorIdom(doms,five));
	assert(two==graphDominatorIdom(doms,four));
	assert(two==graphDominatorIdom(doms,three));
	assert(one==graphDominatorIdom(doms,two));
 }
 {
	__auto_type one = graphNodeIntCreate(1, 0);
	__auto_type two = graphNodeIntCreate(2, 0);
	__auto_type three = graphNodeIntCreate(3, 0);
	__auto_type four = graphNodeIntCreate(4, 0);
	__auto_type five = graphNodeIntCreate(5, 0);
	__auto_type six = graphNodeIntCreate(6, 0);
	
	graphNodeIntConnect(six,five,NULL);
	graphNodeIntConnect(six,four,NULL);
	
	graphNodeIntConnect(four,two,NULL);
	graphNodeIntConnect(four,three,NULL);
	
	graphNodeIntConnect(three,two,NULL);
	graphNodeIntConnect(two,three,NULL);
	
	graphNodeIntConnect(two,one,NULL);
	graphNodeIntConnect(one,two,NULL);
	
	graphNodeIntConnect(five,one,NULL);
	
	__auto_type doms = graphComputeDominatorsPerNode(six);
	int six2[]={6};
	
	graphDominanceCheck(six,doms,six2,1);
	graphDominanceCheck(five,doms,six2,1);
	graphDominanceCheck(four,doms,six2,1);
	graphDominanceCheck(three,doms,six2,1);
	graphDominanceCheck(two,doms,six2,1);
	graphDominanceCheck(one,doms,six2,1);
 }
 {
	//http://pages.cs.wisc.edu/~fischer/cs701.f05/lectures/Lecture22.pdf
	__auto_type a= graphNodeIntCreate(1, 0);
	__auto_type b = graphNodeIntCreate(2, 0);
	__auto_type c= graphNodeIntCreate(3, 0);
	__auto_type d= graphNodeIntCreate(4, 0);
	__auto_type e= graphNodeIntCreate(5, 0);
	__auto_type f= graphNodeIntCreate(6, 0);
	
	graphNodeIntConnect(a,f,NULL);
	graphNodeIntConnect(a,b,NULL);
	
	graphNodeIntConnect(b,c,NULL);
	graphNodeIntConnect(b,d,NULL);
	
	graphNodeIntConnect(c,e,NULL);
	graphNodeIntConnect(d,e,NULL);
	
	graphNodeIntConnect(e,f,NULL);
	
	__auto_type doms = graphComputeDominatorsPerNode(a);
	int oneTwo[]={1,2};
	graphDominanceCheck(c,doms,oneTwo,2);
	graphDominanceCheck(d,doms,oneTwo,2);
	graphDominanceCheck(e,doms,oneTwo,2);
	int one[]={1};
	graphDominanceCheck(b,doms,one,1);
	graphDominanceCheck(f,doms,one,1);
	
	assert(b==graphDominatorIdom(doms,c));
	assert(b==graphDominatorIdom(doms,d));
	assert(b==graphDominatorIdom(doms,e));
	
	assert(a==graphDominatorIdom(doms,b));
	assert(a==graphDominatorIdom(doms,f));
 }
}