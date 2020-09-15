#pragma once
struct rwLock;
void rwReadStart(struct rwLock *lock);
void rwReadEnd(struct rwLock *lock);
struct rwLock *rwLockCreate();
void rwWriteStart(struct rwLock *lock);
void rwWriteEnd(struct rwLock *lock);
void rwLockDestroy(struct rwLock *lock);
