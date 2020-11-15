#include <graph.h>
#include <linkedList.h>
strGraphNodeP nodes;
LL_TYPE_DEF(struct __graphNode*,GN);
LL_TYPE_FUNCS(struct __graphNode*,GN);
struct GNCount {
		struct __graphNode *node;
		long inDegree;
};
STR_TYPE_DEF(struct GNCount,GNCount);
STR_TYPE_FUNCS(struct GNCount,GNCount);
static int GNCountCmp(const void *a,const void *b) {
		const struct GNCount *A=a,*B=b;
		if(A->node>B->node)
				return 1;
		else if(A->node<B->node)
				return -1;
		else
				return 0;
}
strGraphNodeP topoSort(strGraphNodeP nodes) {
		strGNCount counts=NULL;
		llGN queue=NULL;
		//
		// Compute in-degree for all nodes and init count to 0
		//
		for(long i=0;i!=strGraphNodePSize(nodes);i++) {
				__auto_type incoming=__graphNodeIncomingNodes(nodes[i]);
				
				struct GNCount count;
				count.node=nodes[i];
				count.inDegree=strGraphNodePSize(incoming);
				counts=strGNCountSortedInsert(counts,  count, GNCountCmp);
				
				if(strGraphNodePSize(incoming)==0) {
						__auto_type newNode=llGNCreate(nodes[i]);
						llGNInsertListBefore(llGNFirst(queue) , newNode);
						queue=newNode;//New node is the begininng of the qeue as inserted at begining
				}

				strGraphNodePDestroy(&incoming);
		}
		strGraphNodeP retVal=NULL;
		//
		// Remove a node from the queue
		//
		long count=0;
		while(llGNSize(queue)) {
				__auto_type last=llGNLast(queue);
				queue= llGNRemove(last);
				__auto_type node= *llGNValuePtr(last);

				//Append to retVal;
				retVal=strGraphNodePAppendItem(retVal, node);

				__auto_type outgoing=__graphNodeOutgoingNodes(node);
				for(long i=0;i!=strGraphNodePSize(outgoing);i++) {
						struct GNCount dummy;
						dummy.node=outgoing[i];
						__auto_type find=strGNCountSortedFind(counts, dummy,GNCountCmp);;

						//Decr in-degree
						find->inDegree--;

						if(find->inDegree==0) {
								__auto_type newNode= llGNCreate(find->node);
								llGNInsertListBefore(llGNFirst(queue), newNode);
								//newNode is first element
								queue=newNode;
						}
				}

				//inc count
				count++;
				
				llGNDestroy(&last, NULL);
				strGraphNodePDestroy(&outgoing);
		}

		if(count!=strGraphNodePSize(nodes)) {
				strGraphNodePDestroy(&retVal);
				return NULL;
		}

		return retVal;
}

