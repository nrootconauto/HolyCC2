#pragma once
#include <stdio.h>
#include <str.h>
#include <textMapper.h>
struct includeMacro {
	struct __vec *fileName;
};
struct defineMacro {
	struct __vec *name;
	struct __vec *text;
};
FILE *createPreprocessedFile(struct __vec *text, strTextModify *mappings,
                             int *err);
