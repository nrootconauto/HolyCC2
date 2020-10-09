#pragma once
#include <stdio.h>
#include <str.h>
struct includeMacro {
	struct __vec *fileName;
};
struct defineMacro {
	struct __vec *name;
	struct __vec *text;
};
struct sourceMapping {
	long sourcePos;
	long processedStart;
	long processedEnd;
};

STR_TYPE_DEF(struct sourceMapping, SourceMapping);
STR_TYPE_FUNCS(struct sourceMapping, SourceMapping);
long mappedPosition(const strSourceMapping, long processedPos);
FILE *createPreprocessedFile(struct __vec *text, strSourceMapping *mappings,
                             int *err);