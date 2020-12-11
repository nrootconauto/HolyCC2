void gcInit(const void *frameStart) ;
void gcCollect();
void gcFree(void *ptr);
void *gcMalloc(long size);
void *gcRealloc(void *ptr,long newSize);
void gcAddRoot(const void *a,long size);
//https://stackoverflow.com/questions/16552710/how-do-you-get-the-start-and-end-addresses-of-a-custom-elf-section
#define GC_VARIABLE __attribute__((section("GC")))
void gcDestroy();
void gcAddLookForPtr(const void *a,const void *lookFor);
unsigned long gcSize(const void *ptr);

#define GC_MALLOC(size) gcMalloc(size)
#define GC_REALLOC(ptr,size) gcRealloc(ptr,size)
#define GC_FREE(ptr) gcFree(ptr)

void gcEnable();
void gcDisable();

void __gcFree(void *ptr);
#define GC_CLEANUP_DFT __attribute__((cleanup(__gcFree)))
#define GC_CLEANUP(func) __attribute__((cleanup(func)))