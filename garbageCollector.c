#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>
#include <assert.h>
#include <setjmp.h>
#include "garbageCollector.h"
#define ALLOCATE(item) ({void *alloced=malloc(sizeof(item));memcpy(alloced,&item,sizeof(item));alloced;})
extern char __start_GC,__stop_GC;
static void *dummy GC_VARIABLE=NULL;
struct allocation {
		void *lookForPtr;
		long size;
		unsigned int marked:1;
		unsigned int excludeFromGC:1;
};
struct allocationLL {
		struct allocation *entry;
		struct allocationLL *next;
};
struct alias {
		void *lookForPtr;
		void *ptr;
		struct alias *next;
};
struct root {
		void *root;
		long size;
		struct root *next;
};
static __thread struct allocationLL **table=NULL;
static __thread struct alias **aliasTable=NULL;
static __thread long tableSize=0;
static __thread long allocCount=0;
static __thread int age=INT_MIN;
static __thread void *stackStart=NULL;
static __thread struct root *roots=NULL;
static __thread long allocedMem=0;
static __thread long lastCleanupMemSize=0;
static __thread int gcEnabled=0;
static unsigned long hashPtr(const void *a) {
		return ((size_t)a>>3)%tableSize;
}
void gcEnable() {
		gcEnabled=1;
}
void gcDisable() {
		gcEnabled=0;
}
static void *allocationGetDataPtr(const struct allocation *alloc) {
		return (void*)(alloc+1);
}
static void resizeTable(long newSize);
static void insertAllocationIntoMap(struct allocationLL *alloc) {
		alloc->next=NULL;
		__auto_type bucketIndex=hashPtr(allocationGetDataPtr(alloc->entry));
		__auto_type bucket=&table[bucketIndex];
		if(*bucket==NULL) {
				*bucket=alloc;
				return ;
		} else {
				//Sorted
				struct allocationLL *prev=NULL;
				for(__auto_type node=bucket[0];node!=NULL;node=node->next) {
						__auto_type at=allocationGetDataPtr(alloc->entry);
						if(at<allocationGetDataPtr(node->entry)) {
								if(prev) {
								insertAfter:;
										//Insert between
										__auto_type next=prev->next;
										prev->next=alloc;
										alloc->next=next;

										return;
								} else {
										//Insert as first
										alloc->next=node;
										*bucket=alloc;

										return;
								}
						}

						prev=node;
				}
				goto insertAfter;
		}
}
static long hits3=0;
static void removeAlias(const void *alias) {
		__auto_type bucket=hashPtr(alias);
		if(alias==0x80c6274l) {
				printf("Foop\n");
		}
		
		struct alias *prev=NULL,*next;
		for(__auto_type node=aliasTable[bucket];node!=NULL;node=node->next) {
				if(node->lookForPtr==alias) {
						next=node->next;
						free(node);
						break;
				}
				prev=node;
		}

		if(prev) {
				prev->next=next;
		} else {
				aliasTable[bucket]=next;
		}
}
static void resizeTable(long newSize) {
		//Keep old table
		__auto_type oldTable=table;

		//Make a new table
		table=calloc(newSize,sizeof(struct allocationLL*));
		
		for(long i=0;i!=tableSize;i++) {
				for(__auto_type node=oldTable[i];node!=NULL;) {
						__auto_type next=node->next;

						node->next=NULL;
						insertAllocationIntoMap(node);
						
						node=next;
				}
		}

		free(oldTable);
		
		//Make new alias table
		__auto_type oldAliases=aliasTable;
		aliasTable=calloc(newSize,sizeof(struct alias*));
		for(long i=0;i!=tableSize;i++) {
				for(__auto_type node=oldAliases[i];node!=NULL;) {
						gcAddLookForPtr(node->ptr, node->lookForPtr);
						__auto_type next=node->next;
						free(node);

						node=next;
				}
		}

		free(oldAliases);

		//Update table size
		tableSize=newSize;
}
static int needsCleanup() {
		return 64000l<=allocedMem&&allocedMem>=(lastCleanupMemSize/4l+lastCleanupMemSize);
}
void *gcMalloc(long size) {
		if(table==NULL)
				return malloc(size);

		struct allocation *allocation=malloc(sizeof(struct allocation)+size);
		allocation->size=size;
		allocation->marked=1;
		allocation->lookForPtr=allocationGetDataPtr(allocation);
		struct allocationLL ll;
		ll.entry=allocation;
		ll.next=NULL;

		allocedMem+=allocation->size;
		
		allocCount++;
		if(allocCount*32>tableSize&&tableSize>32) {
				resizeTable(tableSize*2);
		}
		
		__auto_type alloced=ALLOCATE(ll);
		insertAllocationIntoMap(alloced);

		if(needsCleanup())
				gcCollect();
		
		return allocationGetDataPtr(allocation);
}
void *gcRealloc(void *ptr,long newSize) {
		if(ptr==NULL)
				return gcMalloc(newSize);
		
		__auto_type bucket=hashPtr(ptr);
		struct allocationLL *prev=NULL;

		for(__auto_type node=table[bucket];node!=NULL;node=node->next) {
				if(allocationGetDataPtr(node->entry)<ptr) {
						prev=node;
						continue;
				} else if(allocationGetDataPtr(node->entry)==ptr) {
						//"Remove" node by making prev point to next elem
						if(prev) {
								prev->next=node->next;
								if(prev->next)
										assert(allocationGetDataPtr(prev->entry)<allocationGetDataPtr(prev->next->entry));
						} else {
								table[bucket]=node->next;
						}

						//Update allocated size
						allocedMem-=node->entry->size;
						allocedMem+=newSize;
																								
						//Update entry
						long offset=node->entry->lookForPtr-allocationGetDataPtr(node->entry);
						//Remove old alias
						if(offset!=0)
								removeAlias(node->entry->lookForPtr);
						
						node->entry=realloc(node->entry,sizeof(*node->entry)+newSize);
						node->entry->size=newSize;
						__auto_type oldLookForPtr=node->entry->lookForPtr;
						node->entry->lookForPtr=offset+allocationGetDataPtr(node->entry);

						//If lookForPtr and ptr aren't the same,remove old alias and insert a new one
						if(offset!=0)
								gcAddLookForPtr(allocationGetDataPtr(node->entry), node->entry->lookForPtr);
						//reinsert node
						node->next=NULL;
						insertAllocationIntoMap(node);
						
						if(needsCleanup())
								gcCollect();
																								
						return allocationGetDataPtr(node->entry);
				} else {
						break;
				}
		}

		//Fallback
		abort();
		return realloc(ptr,newSize);
}
static void *getPtrFromAlias(const void *ptr);
void gcFree(void *ptr) {
		__auto_type alias=getPtrFromAlias(ptr);
		if(alias) {
				gcFree(alias);
				return;
		}
		
		allocCount--;
		if(allocCount*32<tableSize&&tableSize/2>32) {
				resizeTable(tableSize/2);
		}
		
		__auto_type bucket=hashPtr(ptr);
		struct allocationLL *prev=NULL;

		for(__auto_type node=table[bucket];node!=NULL;node=node->next) {
				if(allocationGetDataPtr(node->entry) <ptr) {
						prev=node;
						continue;
				} else if(allocationGetDataPtr(node->entry)==ptr) {
						if(prev)
								prev->next=node->next;

						if(table[bucket]==node)
								table[bucket]=node->next;

						if(getPtrFromAlias(node->entry->lookForPtr)) {
								removeAlias(node->entry->lookForPtr);
						}
						
						allocedMem-=node->entry->size;

						free(node->entry);
						free(node);
						return ;
				} else {
						break;
				}
		}

		printf("Foo\n");
		//Fallback
		return free(ptr);
}
static long hits=0;
static void *getPtrFromAlias(const void *ptr) {
		__auto_type hash=hashPtr(ptr);
		for(__auto_type node=aliasTable[hash];node!=NULL;node=node->next) {
				if(node->lookForPtr==ptr) {
						return node->ptr;
				}
		}

		return NULL;
}
static void scanMemForPointers(const void *start,long size) {
		for(const void **p=(const void**)start;p<(const void**)(start+size);p=(void*)p+1) {
				void *ptr=*(char**)p;
				__auto_type fallBack=getPtrFromAlias(ptr);
				ptr=(fallBack)?fallBack:ptr;
				__auto_type bucketIndex=hashPtr(ptr);
				
				for(__auto_type node=table[bucketIndex];node!=NULL;node=node->next) {
						__auto_type allocedPtr=allocationGetDataPtr(node->entry);
						if(allocedPtr<=ptr&&ptr<allocedPtr+node->entry->size) {
								
								if(!node->entry->marked) {
										node->entry->marked=1;

										if(allocedPtr==0x80b4a3cl) {
												printf("Poop\n");
										}
										
										scanMemForPointers(allocedPtr,node->entry->size);
								}
						} else if(allocedPtr>ptr) {
								break;
						}
				}
		}

}
void gcInit(const void *frameStart) {
		allocedMem=0;
		lastCleanupMemSize=0;
		
		resizeTable(32);
		stackStart=(void*)frameStart;
		gcEnable();
}
void gcCollect();
extern char end,etext,edata;
static unsigned long absDiff(const void *a,const void *b) {
		return(a>b)?a-b:b-a;
}
__attribute__((destructor(101))) void gcDestroy() {
		for(long i=0;i!=tableSize;i++) {
		loop:
				for(__auto_type node=table[i];node!=NULL;node=node->next) {
						gcFree(allocationGetDataPtr(node->entry));
						goto loop;
				}
		}

		free(table);
		table=NULL;
		tableSize=0;
		free(aliasTable);
}
static void __gcCollect() {
		if(!gcEnabled)
				return;
		
		void *top=__builtin_frame_address(0);

		// Initialize all allocations as not marked
		for(long i=0;i!=tableSize;i++) {
				for(__auto_type node=table[i];node!=NULL;node=node->next) {
						node->entry->marked=0;
				}
		}
		
		//Scan stack
		if(top<stackStart)
				scanMemForPointers(stackStart-absDiff(stackStart,top), absDiff(stackStart,top));
		else
				scanMemForPointers(stackStart, absDiff(top,stackStart));
		
		//Scan custom section
		__auto_type start= &__start_GC;
		__auto_type end= &__stop_GC;
		if(start<end)
				scanMemForPointers(start, absDiff(end,start));
		else
				scanMemForPointers(start-absDiff(end,start), absDiff(end,start));
		
		//Free all elems that aren't marked
		for(long i=0;i!=tableSize;i++) {
		loop:
				for(__auto_type node=table[i];node!=NULL;node=node->next) {
						if(node->entry->marked==0) {
								gcFree(allocationGetDataPtr(node->entry));
								goto loop;
						}
				}
		}

		lastCleanupMemSize=allocedMem;
}
void gcCollect() {
		jmp_buf buf;
		setjmp(buf);
		__gcCollect();
}
unsigned long gcSize(const void *ptr) {
		if(!ptr)
				return 0;
				
		struct allocation *alloc=(void*)ptr-sizeof(struct allocation);
		return alloc->size;
}
static long hits2=0;
void gcAddLookForPtr(const void *a,const void *lookFor) {
		struct allocation *A=(void*)a;
		(A-1)->lookForPtr=(void*)lookFor;

		if(lookFor==0x80c6274l) {
				printf("Foop\n");
		}
		
		__auto_type hash=hashPtr(lookFor);

		struct alias new;
		new.lookForPtr=(void*)lookFor;
		new.ptr=(void*)a;
		new.next=NULL;
		struct alias *aliasLL=ALLOCATE(new);
		
		if(aliasTable[hash]==NULL) {
				aliasTable[hash]=aliasLL;
				return;
		}

		struct alias *prev=NULL;
		__auto_type node=aliasTable[hash];
		for(__auto_type node=aliasTable[hash];node!=NULL;) {
				if(prev)
						assert(prev->lookForPtr<=node->lookForPtr);
				if(node->lookForPtr<=lookFor) {
						prev=node;
						node=node->next;
						continue;
				}

		insert:;
				struct alias *next=NULL;
				if(prev) {
						next=prev->next;
						prev->next=aliasLL;
				} else {
						//Make first in bucket
						next=aliasTable[hash];
						aliasTable[hash]=aliasLL;
				}
				aliasLL->next=next;
				return;
		}

		goto insert;
}
void __gcFree(void *ptr) {
		gcFree(*(void**)ptr);
}
