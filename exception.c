#include <assert.h>
#include <exception.h>
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
	jmp_buf from;
	struct __exceptionBlock *prev;
	long threadsCount;
	struct __exceptionAllocs **threadAllocs;
	skinny_mutex_t mutex;
	long typesCount;
	const struct exceptionType **types;
};
struct __exceptionAllocs {
	struct __exceptionBlock *from;
	struct mallocLL *allocs;
	pthread_t threadId;
};
__thread struct __exceptionBlock *currentExceptionBlock = NULL;
__thread struct __exceptionAllocs *currentExceptionAllocs = NULL;
__thread struct __exceptionAllocs *globalExceptionAllocs = NULL;

__thread const struct exceptionType *currentExceptionType = NULL;
__thread void *currentExceptionValue = NULL;

static struct __exceptionAllocs *
exceptionAllocsNew(struct __exceptionBlock *from) {
	struct __exceptionAllocs *new = malloc(sizeof(struct __exceptionAllocs));
	new->from = from;
	new->threadId = pthread_self();
	new->allocs = NULL;
	return new;
}
static struct __exceptionAllocs **threadGetAllocs() {
	if (currentExceptionAllocs == NULL) {
		if (globalExceptionAllocs == NULL) {
			struct __exceptionAllocs *new = malloc(sizeof(struct __exceptionAllocs));
			new->from = NULL;
			new->threadId = pthread_self();
			new->allocs = NULL;
			globalExceptionAllocs = new;
		}

		currentExceptionAllocs = globalExceptionAllocs;
	}

	return &currentExceptionAllocs;
}
void *__malloc(unsigned long size) {
	struct mallocLL *retVal = malloc(size + sizeof(struct mallocLL));
	__auto_type current = *threadGetAllocs();

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
static void exceptionAllocsDestroy(void *Allocs) {
	struct __exceptionAllocs *allocs = Allocs;
	if (allocs == NULL)
		return;

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
void exceptionLeaveBlock();
jmp_buf *exceptionEnterBlock() {
	__auto_type prev = currentExceptionBlock;
	struct __exceptionBlock *newBlock = malloc(sizeof(struct __exceptionBlock));
	newBlock->prev = prev;
	skinny_mutex_init(&newBlock->mutex);

	newBlock->threadsCount = 1;
	newBlock->threadAllocs = malloc(1 * sizeof(struct __exceptionAllocs *));
	newBlock->threadAllocs[0] = exceptionAllocsNew(newBlock);
	newBlock->typesCount = 0;
	newBlock->types = NULL;

	currentExceptionBlock = newBlock;
	*threadGetAllocs() = newBlock->threadAllocs[0];

	if (prev != NULL) {
		skinny_mutex_lock(&prev->mutex);

		prev->threadAllocs = realloc(
		    prev->threadAllocs,
		    sizeof(struct __exceptionAllocs *) *
		        ++prev->threadsCount); // prev->threadsCount incremented here
		prev->threadAllocs[prev->threadsCount - 1] = newBlock->threadAllocs[0];

		skinny_mutex_unlock(&prev->mutex);
	}

	return &currentExceptionBlock->from;
}
static void exceptionBlockDestroy(struct __exceptionBlock *block) {
	free(block->types);
	free(block->threadAllocs);
	skinny_mutex_destroy(&block->mutex);
	free(block);
}
void exceptionLeaveBlock() {
	struct __exceptionBlock *Block = currentExceptionBlock;
	__auto_type prev = Block->prev;

	skinny_mutex_lock(&Block->mutex);

	for (int i = 0; i != Block->threadsCount; i++) {
		if (pthread_equal(pthread_self(), Block->threadAllocs[i]->threadId)) {
			exceptionAllocsDestroy(Block->threadAllocs[0]);
			__auto_type ptr = &Block->threadAllocs[i];
			memmove(ptr, ptr + 1,
			        sizeof(struct __exceptionAllocs) * (Block->threadsCount - i - 1));
		}
	}

	if (--Block->threadsCount == 0)
		exceptionBlockDestroy(Block);
	else
		skinny_mutex_unlock(&Block->mutex);

	currentExceptionBlock = prev;
}
void exceptionFailBlock() {
	__auto_type block = currentExceptionBlock;
	assert(block != NULL);

	skinny_mutex_lock(&block->mutex);
	__auto_type waitForCount = block->threadsCount;
	pthread_t waitFor[waitForCount];
	for (int i = 0; i != waitForCount; i++)
		waitFor[i] = block->threadAllocs[i]->threadId;
	skinny_mutex_unlock(&block->mutex);

	for (int i = 0; i != waitForCount; i++)
		pthread_join(waitFor[i], NULL);

	exceptionAllocsDestroy(threadGetAllocs());
	currentExceptionBlock = currentExceptionBlock->prev;
}
struct __blockAndData {
	struct __exceptionBlock *block;
	void *data;
	void *(*func)(void *);
};
static void exceptionThreadExit(void *unused) {
	while (currentExceptionBlock != NULL)
		exceptionLeaveBlock();

	exceptionAllocsDestroy(threadGetAllocs());
}
static void *trySpawnForward(void *pair) {
	struct __blockAndData *Pair = pair;

	currentExceptionAllocs = exceptionAllocsNew(Pair->block);

	currentExceptionBlock = Pair->block;
	if (currentExceptionBlock != NULL) {
		skinny_mutex_lock(&currentExceptionBlock->mutex);

		__auto_type oldSize = Pair->block->threadsCount++;
		Pair->block->threadAllocs =
		    realloc(Pair->block->threadAllocs, Pair->block->threadsCount);
		Pair->block->threadAllocs[oldSize] = currentExceptionAllocs;

		skinny_mutex_unlock(&currentExceptionBlock->mutex);
	} else {
		globalExceptionAllocs = currentExceptionAllocs;
	}

	void *retVal;
	pthread_cleanup_push(exceptionAllocsDestroy, exceptionThreadExit);
	retVal = Pair->func(Pair->data);
	pthread_cleanup_pop(1);

	return retVal;
}
void trySpawn(void *(*func)(void *), void *arg) {
	struct __blockAndData pair;
	pair.block = currentExceptionBlock;
	pair.data = arg;
	pair.func = func;

	pthread_t thread;
	pthread_create(&thread, NULL, trySpawnForward, &pair);
}
void exceptionThrow(const struct exceptionType *type, void *data) {
	__auto_type block = currentExceptionBlock;
	for (; block != NULL;) {
		for (int i = 0; i != block->typesCount; i++) {
			if (block->types[i] == type) {
				currentExceptionType = type;
				currentExceptionValue = data;
				longjmp(block->from, 1);
			}
		}
		exceptionLeaveBlock();
		block = currentExceptionBlock;
	}

	// Couldn't find handler so exit
	if (block == NULL)
		pthread_exit(NULL);
}
void exceptionBlockAddCatchType(const struct exceptionType *type) {
	assert(currentExceptionBlock != NULL);

	__auto_type oldSize = currentExceptionBlock->typesCount++;
	currentExceptionBlock->types =
	    realloc(currentExceptionBlock->types,
	            sizeof(struct exceptionType) * currentExceptionBlock->typesCount);
	currentExceptionBlock->types[oldSize] = type;
}
void *exceptionValueByType(const struct exceptionType *type) {
	void *retVal = NULL;
	if (type == currentExceptionType) {
		retVal = currentExceptionValue;

		currentExceptionValue = NULL;
		currentExceptionType = NULL;
	}

	return retVal;
}
