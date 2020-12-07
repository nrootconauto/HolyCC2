#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>
#define ALLOCATE(item)({void *alloced=malloc(sizeof(item));memcpy(alloced,&item,sizeof(item));alloced;})
struct allocation {
		void *alloced;
		void *lookForPtr;
		long size;
		unsigned int marked:1;
		unsigned int excludeFromGC:1;
};
struct allocationLL {
		struct allocation entry;
		struct allocationLL *next;
};
static __thread struct allocationLL **table=NULL;
static __thread long tableSize=0;
static __thread int age=INT_MIN;
static __thread void *stackStart=NULL;

static unsigned long hashPtr(const void *a) {
		return ((size_t)a>>3)%tableSize;
}
static void insertAllocationIntoMap(struct allocationLL *alloc) {
		alloc->next=NULL;

		__auto_type bucketIndex=hashPtr(alloc->entry.alloced);
		__auto_type bucket=&table[bucketIndex];
		if(*bucket==NULL) {
				*bucket=alloc;
		} else {
				//Sorted
				__auto_type prev=bucket[0];
				for(__auto_type node=bucket[0]->next;node!=NULL;node=node->next) {
						if(alloc->entry.alloced<=node->entry.alloced) {
								//Insert between
								__auto_type next=prev->next;
								prev->next=alloc;
								alloc->next=next;
								break;
						}

						prev=node;
				}
		}
}
static void rehashTable(long newSize) {
		//Archive old buckets
		struct allocationLL *oldBuckets[tableSize];
		long oldTableSize=tableSize;
		for(long i=0;i!=tableSize;i++)
				oldBuckets[i]=table[i];

		free(table);

		//Update table size
		tableSize=newSize;
		table=calloc(newSize,sizeof(*table));
		
		for(long i=0;i!=oldTableSize;i++) {
				for(__auto_type node=oldBuckets[i];node!=NULL;)	{
						//We stash the old next becuase we change the value of node ahead.
						__auto_type next=node->next;
						
						insertAllocationIntoMap(node);

						node=next;
				}
		}
}
void *gcMalloc(long size) {
		if(table==NULL)
				return malloc(size);
				
		struct allocation new;
		new.size=size;
		new.lookForPtr=malloc(size);
		new.alloced=new.lookForPtr;
		new.marked=0;
		struct allocationLL ll;
		ll.entry=new;
		ll.next=NULL;

		__auto_type alloced=ALLOCATE(ll);
		insertAllocationIntoMap(alloced);

		return new.lookForPtr;
}
void *gcRealloc(void *ptr,long newSize) {
		__auto_type bucket=hashPtr(ptr);
		struct allocationLL *prev=NULL;

		for(__auto_type node=table[bucket];node!=NULL;node=node->next) {
				if(node->entry.alloced<ptr) {
						prev=node;
						continue;
				} else if(node->entry.alloced==ptr) {
						long offset=node->entry.lookForPtr-node->entry.alloced;;
						node->entry.alloced=realloc(node->entry.alloced,newSize);
						node->entry.lookForPtr=node->entry.alloced+offset;
						node->entry.size=newSize;

						return node->entry.alloced;
				} else {
						break;
				}
		}

		//Fallback
		return realloc(ptr,newSize);
}
void gcFree(void *ptr) {
		__auto_type bucket=hashPtr(ptr);
		struct allocationLL *prev=NULL;

		for(__auto_type node=table[bucket];node!=NULL;node=node->next) {
				if(node->entry.alloced<ptr) {
						prev=node;
						continue;
				} else if(node->entry.alloced==ptr) {
						if(prev)
						prev->next=node->next;

						free(node->entry.alloced);
						free(node);
				} else {
						break;
				}
		}

		//Fallback
		return free(ptr);
}
static void scanMemForPointers(const void *start,long size) {
		for(const void *p=start;p<start+size;p++) {
				void *ptr=*(void**)p;
				__auto_type bucketIndex=hashPtr(ptr);
				
				for(__auto_type node=table[bucketIndex];node!=NULL;node=node->next) {
						if(node->entry.alloced<=ptr&&ptr<node->entry.alloced+node->entry.size) {
								if(!node->entry.marked) {
										node->entry.marked=1;
										
										scanMemForPointers(node->entry.alloced,node->entry.size);
								}
						} else if(node->entry.alloced<ptr) {
								break;
						}
				}
		}

}
void gcInit(const void *frameStart) {
		rehashTable(32);
		stackStart=(void*)frameStart;
}
void gcCollect() __attribute__((destructor));
void gcCollect() {
		void *top=__builtin_frame_address(0);

		// Initialize all allocations as not marked
		for(long i=0;i!=tableSize;i++) {
				for(__auto_type node=table[i];node!=NULL;node=node->next) {
						node->entry.marked=0;
				}
		}
		
		//Scan stack
		scanMemForPointers(stackStart, top-stackStart);

		//Free all elems that aren't marked
		for(long i=0;i!=tableSize;i++) {
				for(__auto_type node=table[i];node!=NULL;node=node->next) {
						if(node->entry.marked==0) {
								gcFree(node->entry.lookForPtr);
						}
				}
		}
}
