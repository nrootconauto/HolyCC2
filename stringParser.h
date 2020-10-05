#pragma once
#include <str.h>
struct parsedString {
	struct __vec *text;
	int isChar : 1;
};
void parsedStringDestroy(struct parsedString *str);
int stringParse(struct __vec *new, long pos, long *end,
                struct parsedString *retVal, int *err);
