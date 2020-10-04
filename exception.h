#pragma once
#include <setjmp.h>
struct exceptionType {
	const char *name;
};
#define EXCEPTION_TYPE_DECLARE(name) extern const struct exceptionType name;
#define EXCEPTION_TYPE_DEFINE(name) const struct exceptionType name = {#name};
void exceptionThrow(const struct exceptionType *type, void *data);
void trySpawn(void *(*func)(void *), void *arg);
void exceptionFailBlock();
void exceptionLeaveBlock();
jmp_buf *exceptionEnterBlock();
void exceptionBlockAddCatchType(const struct exceptionType *type);
void *exceptionValueByType(const struct exceptionType *type);
#define try                                                                    \
	while (!setjump(exceptionEnterBlock()))                                      \
		for (int i = 0; i != 2; i++)                                               \
			if (i == 1) {                                                            \
				exceptionLeaveBlock()                                                  \
			} else
#define catch (type, value)                                                    \
	for (void *value = exceptionValueByType(type); value != NULL; value = NULL)
