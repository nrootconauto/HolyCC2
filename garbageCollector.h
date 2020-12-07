void gcInit(const void *frameStart) ;
void gcCollect();
void *gcMalloc(long size);
void *gcRealloc(void *ptr,long newSize);
