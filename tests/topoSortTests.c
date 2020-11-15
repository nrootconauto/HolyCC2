#include <assert.h>
#include <topoSort.h>
GRAPH_TYPE_DEF(int, void*, Int);
GRAPH_TYPE_FUNCS(int, void*, Int);
//https://www.geeksforgeeks.org/topological-sorting-indegree-based-solution/?ref=lbp
static void validateTopoSort(strGraphNodeIntP res) {
		for(long i=0;i!=strGraphNodePSize(res);i++) {
				__auto_type outgoing; 
		}
}
int topoSortTests() {
		__auto_type zero=graphNodeIntCreate(0, 0);
		__auto_type one=graphNodeIntCreate(1, 0);
		__auto_type two=graphNodeIntCreate(2, 0);
		__auto_type three=graphNodeIntCreate(3, 0);
		__auto_type four=graphNodeIntCreate(4, 0);
		__auto_type five=graphNodeIntCreate(5, 0);

		graphNodeIntConnect(five,two, NULL);
		graphNodeIntConnect(five,zero, NULL);
		graphNodeIntConnect(four,zero, NULL);
		graphNodeIntConnect(four,one, NULL);
		graphNodeIntConnect(two,three, NULL);
		graphNodeIntConnect(three,one, NULL);

		strGraphNodeIntP vec=strGraphNodeIntPResize(NULL, 6);
		vec[0]=zero;
		vec[1]=one;
		vec[2]=two;
		vec[3]=three;
		vec[4]=four;
		vec[5]=five;

		__auto_type res=topoSort(vec);
		assert(res);
}
