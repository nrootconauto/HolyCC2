#include <assert.h>
#include <pthread.h>
#include <setjmp.h>
#include <skinny_mutex.h>
#include <stdlib.h>
#include <string.h>
struct mallocLL {
	struct mallocLL *next;
	struct mallocLL *prev;
};
struct __exceptionAllocs;
struct __exceptionBlock {
	jmp_buf *from;
	struct __exceptionBlock *prev;
	long threadsCount;
	struct __exceptionAllocs **threadAllocs;
	skinny_mutex_t mutex;
};
struct __exceptionAllocs {
	struct __exceptionBlock *from;
	struct mallocLL *allocs;
	pthread_t threadId;
};
__thread struct __exceptionBlock *currentExceptionBlock = NULL;
__thread struct __exceptionAllocs *currentExceptionAllocs = NULL;
static struct __exceptionAllocs *
exceptionAllocsNew(struct __exceptionBlock *from) {
	struct __exceptionAllocs *new = malloc(sizeof(struct __exceptionAllocs));
	new->from = from;
	new->threadId = pthread_self();
	new->allocs = NULL;
	return new;
}
static struct __exceptionAllocs *threadGetScope() {
	if (currentExceptionAllocs == NULL)
		currentExceptionAllocs = exceptionAllocsNew(NULL);

	return currentExceptionAllocs;
}
void *__malloc(unsigned long size) {
	struct mallocLL *retVal = malloc(size + sizeof(struct mallocLL));
	__auto_type current = threadGetScope();

	if (current->allocs != NULL)
		retVal->next = current->allocs;
	current->allocs = retVal;

	retVal->prev = NULL;
	return (void *)retVal + sizeof(struct mallocLL);
}
void *__calloc(unsigned long size) {
	__auto_type retVal = __malloc(size);

	memset(retVal, 0, size);

	return retVal;
}
void __free(void *ptr) {
	struct mallocLL *alloc = ptr - sizeof(struct mallocLL);

	if (alloc->prev != NULL)
		alloc->prev->next = alloc->next;

	if (alloc->next != NULL)
		alloc->next->prev = alloc->prev;

	free(alloc);
}
void *__realloc(void *ptr, unsigned long newSize) {
	struct mallocLL *alloc = ptr - sizeof(struct mallocLL);
	alloc = realloc(alloc, sizeof(struct mallocLL) + newSize);

	return (void *)alloc + sizeof(sizeof(struct mallocLL));
}
static void exceptionAllocsDestroy(struct __exceptionAllocs *allocs) {

	__auto_type currentAllocs = allocs->allocs;
	if (currentAllocs != NULL) {
		for (__auto_type node = currentAllocs->next; node != NULL;
		     node = node->next)
			free(node);
		for (__auto_type node = currentAllocs->prev; node != NULL;
		     node = node->prev)
			free(node);
		free(currentAllocs);
	}

	free(currentExceptionBlock);
}
void exceptionLeaveBlock(void *Block);
void exceptionEnterBlock(void *typePtr, jmp_buf *from) {
	__auto_type prev = currentExceptionBlock;
	struct __exceptionBlock *newBlock = malloc(sizeof(struct __exceptionBlock));
	newBlock->prev = prev;
	newBlock->from = from;
	skinny_mutex_init(&newBlock->mutex);

	newBlock->threadsCount = 1;
	newBlock->threadAllocs = malloc(1 * sizeof(struct __exceptionAllocs *));
	newBlock->threadAllocs[0] = exceptionAllocsNew(newBlock);

	currentExceptionBlock = newBlock;
	currentExceptionAllocs = newBlock->threadAllocs[0];

	pthread_cleanup_push(exceptionLeaveBlock, newBlock);
	pthread_cleanup_pop(0);

	if (prev != NULL) {
		skinny_mutex_lock(&prev->mutex);

		prev->threadAllocs =
		    realloc(prev->threadAllocs,
		            sizeof(struct __exceptionAllocs *) * ++prev->threadsCount); // prev->threadsCount incremented here
		prev->threadAllocs[prev->threadsCount - 1] = newBlock->threadAllocs[0];

		skinny_mutex_unlock(&prev->mutex);
	}
}
void exceptionLeaveBlock(void *Block) {
	struct __exceptionBlock *block = Block;
	__auto_type prev = block->prev;

	skinny_mutex_lock(&block->mutex);

	for (int i = 0; i != block->threadsCount; i++) {
		if (pthread_equal(pthread_self(), block->threadAllocs[i]->threadId)) {
			exceptionAllocsDestroy(block->threadAllocs[0]);
		}
	}

	for (int i = 0; i != block->threadsCount; i++) {
		pthread_cancel(block->threadAllocs[i]->threadId);
		pthread_join(block->threadAllocs[i]->threadId, NULL);
	}

	free(block->threadAllocs);
	skinny_mutex_destroy(&block->mutex);
}
void trySpawn(void(*func)(void *),void *arg) {
 __
}
