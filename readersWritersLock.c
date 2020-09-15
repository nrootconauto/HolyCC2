#include <readersWritersLock.h>
#include <skinny_mutex.h>
#include <stdlib.h>
// https://en.wikipedia.org/wiki/Readers%E2%80%93writer_lock
struct rwLock {
	skinny_mutex_t mutexRead;
	skinny_mutex_t mutexWrite;
	int b;
};
void rwReadStart(struct rwLock *lock) {
	skinny_mutex_lock(&lock->mutexRead);
	lock->b++;
	if (lock->b == 1)
		skinny_mutex_lock(&lock->mutexWrite);
	skinny_mutex_unlock(&lock->mutexRead);
}
void rwReadEnd(struct rwLock *lock) {
	skinny_mutex_lock(&lock->mutexRead);
	if (--lock->b == 0)
		skinny_mutex_unlock(&lock->mutexWrite);
	skinny_mutex_unlock(&lock->mutexRead);
}
struct rwLock *rwLockCreate() {
	struct rwLock *retVal = malloc(sizeof(struct rwLock));
	skinny_mutex_init(&retVal->mutexRead);
	skinny_mutex_init(&retVal->mutexWrite);
	retVal->b = 0;
	return retVal;
}
void rwWriteStart(struct rwLock *lock) { skinny_mutex_lock(&lock->mutexWrite); }
void rwWriteEnd(struct rwLock *lock) { skinny_mutex_unlock(&lock->mutexWrite); }
void rwLockDestroy(struct rwLock *lock) {
	skinny_mutex_destroy(&lock->mutexRead);
	skinny_mutex_destroy(&lock->mutexWrite);
	free(lock);
}
