#include "str.h"
STR_TYPE_DEF(char *, LinkFns);
STR_TYPE_FUNCS(char *, LinkFns);
extern __thread int HCC_Debug_Enable;
extern __thread strLinkFns HCC_Link_To;
void compileFile(const char *fn, const char *dumpTo);
