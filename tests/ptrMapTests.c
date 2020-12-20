#include <ptrMap.h>
#include <stdlib.h>
#include <assert.h>
PTR_MAP_DEF(Int);
PTR_MAP_FUNCS(int *, int, Int);
void ptrMapTests() {
		 ptrMapInt map=ptrMapIntCreate();
			long count=1024;
			int *allocs[count];
			for(long i=0;i!=count;i++) {
					int *mem=malloc(sizeof(int));
					*mem=i;

					ptrMapIntAdd(map, mem, i);
					allocs[i]=mem;
					assert(*ptrMapIntGet(map, allocs[i])==i);
			}

			for(long i=0;i!=count;i++) {
					assert(*ptrMapIntGet(map, allocs[i])==i);
					ptrMapIntRemove(map, allocs[i]);
			}

			ptrMapIntDestroy(map, NULL);
}
